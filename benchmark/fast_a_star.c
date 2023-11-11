#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "common/c_queue.h"

#include "client/cl_client.h"
#include "game/g_game.h"
#include "vk/vk_system.h"
#include "vk/vk_vulkan.h"
#include <cglm/vec2.h>

#include <string.h>

#define CORRIDOR 0

const float G_Horizontal = 1.0f;
const float G_Diagonal = 1.414213562f;

vec2 G_NodeDirections[8] = {
    [0] = {1, 0}, [1] = {0, 1},  [2] = {0, -1},  [3] = {-1, 0},
    [4] = {1, 1}, [5] = {-1, 1}, [6] = {-1, -1}, [7] = {1, -1},
};

typedef struct cpu_path_t {
  vec2 *points;
  unsigned count;
  unsigned current;
} cpu_path_t;

typedef struct cpu_tile_t {
  unsigned wall;
} cpu_tile_t;

typedef struct cpu_agent_t {
  vec2 target;
  float speed;
  enum agent_state_e {
    AGENT_PATH_FINDING,
    AGENT_MOVING,
    AGENT_NOTHING,
  } state;

  cpu_path_t computed_path;
} __attribute__((aligned(16))) cpu_agent_t;

typedef struct node_t node_t;

struct node_t {
  float f;
  vec2 position;
  float g;
  float h;

  node_t *next;
} __attribute__((aligned(16)));

typedef enum node_state_t {
  NODE_NOTHING,
  NODE_OPENED,
  NODE_CLOSED,
  NODE_PRUNED,
} node_state_t;

typedef struct worker_t {
  // Stuff when path finding is needed
  queue_t open_set;
  node_t *nodes;
  // 0 means nothing
  // 1 means in open set
  // 2 means in closed set
  // 3 means pruned
  unsigned *state_set;
  unsigned *open_set_bool;

  unsigned open_set_count;
  unsigned closed_set_count;

  game_t *game;
} worker_t;

struct game_t {
  vk_system_t *model_matrix_sys;
  vk_system_t *path_finding_sys;

  cpu_agent_t *cpu_agents;
  struct Agent *gpu_agents;
  struct Transform *transforms;
  cpu_tile_t *tiles;
  unsigned entity_count;

  worker_t workers[8];
};

void G_PrintNode(node_t *node) {
  printf("node_t {\n");
  printf("\tposition: %.03f, %.03f\n", node->position[0], node->position[1]);
  printf("\tg: %.03f\n", node->g);
  printf("\tf: %.03f\n", node->f);
  printf("\th: %.03f\n", node->h);
  printf("};\n");
}

// __attribute__((always_inline)) inline static float G_Distance(vec2 a, vec2 b)
// { return sqrtf(powf(a[0] - b[0], 2.0) + powf(a[1] - b[1], 2.0));
// }

float G_Distance(const vec2 a, const vec2 b) {
  // The Diagonal distance
  // from https://theory.stanford.edu/~amitp/GameProgramming/Heuristics.html#S7
  float dx = fabsf(a[0] - b[0]);
  float dy = fabsf(a[1] - b[1]);

  float D1 = 1.0;        // Cost of moving horizontally
  float D2 = G_Diagonal; // Cost of moving diagonally

  return D1 * (dx + dy) + (D2 - 2 * D1) * fminf(dx, dy);
}

static bool G_ValidCell(const vec2 position, const vec2 d,
                        const cpu_tile_t *const tiles) {
  if (tiles[(int)(position[1] * 256 + position[0])].wall) {
    return false;
  }

  // Check for diagonal
  if (fabsf(d[0]) == 1 && fabsf(d[1]) == 1) {
    unsigned through_a = 0.0f;
    unsigned through_b = 0.0f;
    {
      unsigned check_col = position[0];
      unsigned check_row = position[1] - d[1];

      through_a = tiles[check_row * 256 + check_col].wall;
    }
    {
      unsigned check_row = position[0] - d[0];
      unsigned check_col = position[1];

      through_b = tiles[check_row * 256 + check_col].wall;
    }

    if (through_a || through_b) {
      return false;
    }
  }

  return true;
}

float G_CompareNodes(const void *a, const void *b) {
  return ((node_t *)b)->f - ((node_t *)a)->f;
}

void G_PathFinding(worker_t *worker, unsigned entity) {
  for (unsigned x = 0; x < 256; x++) {
    for (unsigned y = 0; y < 256; y++) {
      unsigned n_idx = x + y * 256;
      worker->nodes[n_idx].position[0] = (float)x;
      worker->nodes[n_idx].position[1] = (float)y;
      worker->nodes[n_idx].h = 0.0f;
      worker->nodes[n_idx].f = 0.0;
      worker->nodes[n_idx].g = INFINITY;
      worker->nodes[n_idx].next = NULL;
    }
  }

  worker->open_set.size = 0;
  worker->open_set_count = 0;

  memset(worker->open_set.data, 0, 256 * 256 * sizeof(unsigned));
  memset(worker->state_set, 0, 256 * 256 * sizeof(unsigned));
  memset(worker->open_set_bool, 0, 256 * 256 * sizeof(unsigned));

  cpu_agent_t *agent = &worker->game->cpu_agents[entity];
  const vec2 target = {
      [0] = agent->target[0],
      [1] = agent->target[1],
  };
  struct Transform *transform = &worker->game->transforms[entity];

  unsigned iteration = 0;

  node_t *start = &worker->nodes[(int)(transform->position[1] * 256 +
                                       transform->position[0])];
  start->g = 0.0;
  start->h = G_Distance(target, transform->position);
  start->f = start->g + start->h;

  C_QueueEnqueue(&worker->open_set, start);
  worker->state_set[(int)(transform->position[1] * 256 +
                          transform->position[0])] = NODE_OPENED;
  worker->open_set_count += 1;

  while (worker->open_set_count > 0) {
    node_t *current = C_QueueDequeue(&worker->open_set);
    worker->open_set_count -= 1;

    unsigned current_idx = current->position[1] * 256 + current->position[0];

    if (current->position[0] == target[0] &&
        current->position[1] == target[1]) {
      node_t *back = current;
      while (back != NULL) {
        agent->computed_path.count++;
        back = back->next;
      }
      agent->computed_path.points =
          calloc(agent->computed_path.count, sizeof(vec2));
      back = current;
      unsigned p = agent->computed_path.count;
      while (back != NULL) {
        glm_vec2(back->position, agent->computed_path.points[p - 1]);
        back = back->next;
        p--;
      }

      agent->state = AGENT_MOVING;
      return;
    }

    worker->state_set[current_idx] = NODE_CLOSED;

    for (unsigned i = 0; i < 8; i++) {
      vec2 neighbor;
      glm_vec2_add(current->position, G_NodeDirections[i], neighbor);

      int neighbor_col = neighbor[0];
      int neighbor_row = neighbor[1];
      int neighbor_idx = neighbor_row * 256 + neighbor_col;

      if (!(neighbor_col >= 0 && neighbor_row >= 0 && neighbor_col < 256 &&
            neighbor_row < 256)) {
        continue;
      }

      if (worker->game->tiles[neighbor_idx].wall) {
        continue;
      }

      float tentative_g = current->g;
      if (i >= 4) {
        tentative_g += G_Diagonal;
      } else {
        tentative_g += 1.0;
      }

      if (tentative_g < worker->nodes[neighbor_idx].g) {
        node_t *neighbor_node = &worker->nodes[neighbor_idx];
        if (neighbor_node->h == 0.0f) {
          neighbor_node->h = G_Distance(neighbor, target);
        }
        neighbor_node->g = tentative_g;
        neighbor_node->f = neighbor_node->g + neighbor_node->h;
        neighbor_node->next = current;

        // if (worker->state_set[neighbor_idx] != NODE_OPENED) {
        C_QueueEnqueue(&worker->open_set, neighbor_node);
        worker->state_set[neighbor_idx] = 2;
        worker->open_set_count += 1;
        // } else {
        // C_QueueSort(&worker->open_set, );
        // }
      }
    }
  }

  printf("didn't find anything after %d iterations...\n", iteration);
}

int main(int argc, char const *argv[]) {
  game_t game;

  time_t seed = time(NULL);
  srand(seed);

  game.cpu_agents = calloc(3000, sizeof(cpu_agent_t));
  game.transforms = calloc(3000, sizeof(struct Transform));
  game.tiles = calloc(256 * 256, sizeof(cpu_tile_t));

  for (unsigned i = 0; i < 3000; i++) {
    struct Transform transform = {
        .position =
            {
                [0] = ((float)(rand() % 8) * 10.0f) + 0.0f + 100.0f,
                [1] = ((float)(rand() % 8) * 10.0f) + 2.0f + 100.0f,
                [2] = 0.0f,
                [3] = 1.0f,
            },
        .scale =
            {
                [0] = 1.0,
                [1] = 1.0,
                [2] = 1.0,
                [3] = 1.0f,
            },
    };

    cpu_agent_t cpu_agent = {
        .state = AGENT_PATH_FINDING,
        .speed = 1.0,
        .target =
            {
#if CORRIDOR
                [0] = 55.0f,
                [1] = 35.0f,
#else
                [0] = 0.0f,
                [1] = 0.0f,
#endif
            },
    };

    game.transforms[i] = transform;
    game.cpu_agents[i] = cpu_agent;
  }

  for (unsigned x = 0; x < 256; x++) {
    for (unsigned y = 0; y < 256; y++) {
      game.tiles[x * 256 + y].wall = 0;
    }
  }

  worker_t worker = {
      .game = &game,
      .nodes = calloc(256 * 256, sizeof(node_t)),
      .state_set = calloc(256 * 256, sizeof(unsigned)),
      .open_set_bool = calloc(256 * 256, sizeof(unsigned)),
      .open_set_count = 0,
  };

  if (CORRIDOR) {
    unsigned room_1_count = 0;
    unsigned *room_1 = malloc(sizeof(unsigned) * 512);

    G_Rectangle((ivec2){10, 10}, (ivec2){60, 20}, room_1, &room_1_count);

    for (unsigned i = 0; i < room_1_count; i++) {
      game.tiles[room_1[i]].wall = 1;
    }

    game.tiles[room_1[9]].wall = 0;

    G_Rectangle((ivec2){10, 20}, (ivec2){60, 30}, room_1, &room_1_count);

    for (unsigned i = 0; i < room_1_count; i++) {
      game.tiles[room_1[i]].wall = 1;
    }

    game.tiles[room_1[40]].wall = 0;

    G_Rectangle((ivec2){10, 30}, (ivec2){60, 40}, room_1, &room_1_count);

    for (unsigned i = 0; i < room_1_count; i++) {
      game.tiles[room_1[i]].wall = 1;
    }

    game.tiles[room_1[9]].wall = 0;
  } else {
    for (unsigned i = 0; i < 9000; i++) {
      unsigned row = rand() % 256;
      unsigned col = rand() % 256;

      if (row != 0 && col != 0) {
        game.tiles[row * 256 + col].wall = 1;
      }
    }
  }

  C_QueueNew(&worker.open_set, G_CompareNodes, 256 * 256);

  for (unsigned i = 0; i < 100; i++) {
    G_PathFinding(&worker, i);
    // if (i == 0) {
    //   for (unsigned x = 0; x < 256; x++) {
    //     for (unsigned y = 0; y < 256; y++) {
    //       printf("%d ", worker.closed_set[y * 256 + x]);
    //     }
    //     printf("\n");
    //   }
    // }
  }

  return 0;
}
