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

unsigned max_open_set_count = 0;

const float G_HeuristicWeight = 1.1f;

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
  float cost;
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
  vec2 position;
  float g;
  float h;
  float f;

  node_t *next;
} __attribute__((aligned(16)));

typedef struct worker_t {
  // Stuff when path finding is needed
  queue_t open_set;
  node_t *nodes;
  unsigned *closed_set;
  unsigned *open_set_bool;

  unsigned open_set_count;

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

  float D1 = 1.0;   // Cost of moving horizontally
  float D2 = 1.141; // Cost of moving diagonally

  return D1 * (dx + dy) + (D2 - 2 * D1) * fminf(dx, dy);
}

static bool G_ValidCell(const vec2 position, const unsigned d,
                        const cpu_tile_t *const tiles) {
  float cost = tiles[(int)(position[1] * 256 + position[0])].cost;
  if (cost == 999) {
    return false;
  }

  // Check for diagonal
  if (d >= 4) {
    float through_a = 0.0f;
    float through_b = 0.0f;
    {
      unsigned check_col = position[0];
      unsigned check_row = position[1] - G_NodeDirections[d][1];

      through_a = tiles[check_row * 256 + check_col].cost;
    }
    {
      unsigned check_row = position[0] - G_NodeDirections[d][0];
      unsigned check_col = position[1];

      through_b = tiles[check_row * 256 + check_col].cost;
    }

    if (through_a == 999.0f || through_b == 999.0f) {
      return false;
    }
  }

  return true;
}

int G_CompareNodes(const void *a, const void *b) {
  return ((node_t *)b)->f - ((node_t *)a)->f;
}

void G_PathFinding(worker_t *worker, unsigned entity) {
  for (unsigned x = 0; x < 256; x++) {
    for (unsigned y = 0; y < 256; y++) {
      unsigned n_idx = x + y * 256;
      worker->nodes[n_idx].position[0] = (float)x;
      worker->nodes[n_idx].position[1] = (float)y;
      worker->nodes[n_idx].f = INT32_MAX;
      worker->nodes[n_idx].g = INT32_MAX;
      worker->nodes[n_idx].next = NULL;
    }
  }

  worker->open_set.size = 0;

  memset(worker->open_set.data, 0, 256 * 256 * sizeof(unsigned));
  memset(worker->closed_set, 0, 256 * 256 * sizeof(unsigned));
  memset(worker->open_set_bool, 0, 256 * 256 * sizeof(unsigned));

  cpu_agent_t *agent = &worker->game->cpu_agents[entity];
  const vec2 target = {
      [0] = agent->target[0],
      [1] = agent->target[1],
  };
  struct Transform *transform = &worker->game->transforms[entity];

  float d_s = G_Distance(transform->position, target);
  unsigned idx =
      (int)transform->position[0] + (int)transform->position[1] * 256;

  worker->nodes[idx].f = d_s;
  worker->nodes[idx].h = G_HeuristicWeight * d_s;
  worker->nodes[idx].g = 0.0f;

  // worker->open_set[0] = &worker->nodes[idx];
  worker->open_set_count = 1;
  C_QueueEnqueue(&worker->open_set, &worker->nodes[idx]);

  int iteration = 0;
  while (worker->open_set_count > 0) {
    iteration++;

    node_t *current = C_QueueDequeue(&worker->open_set);

    worker->open_set_count--;

    worker->closed_set[(int)current->position[1] * 256 +
                       (int)current->position[0]] = true;

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

    vec2 neighbors[8];
    glm_vec2_add(current->position, G_NodeDirections[0], neighbors[0]);
    glm_vec2_add(current->position, G_NodeDirections[1], neighbors[1]);
    glm_vec2_add(current->position, G_NodeDirections[2], neighbors[2]);
    glm_vec2_add(current->position, G_NodeDirections[3], neighbors[3]);
    glm_vec2_add(current->position, G_NodeDirections[4], neighbors[4]);
    glm_vec2_add(current->position, G_NodeDirections[5], neighbors[5]);
    glm_vec2_add(current->position, G_NodeDirections[6], neighbors[6]);
    glm_vec2_add(current->position, G_NodeDirections[7], neighbors[7]);

    for (unsigned i = 0; i < 8; i++) {
      int new_col = (int)neighbors[i][0];
      int new_row = (int)neighbors[i][1];

      if (!(new_row >= 0 && new_col >= 0 && new_row < 256 && new_col < 256)) {
        continue;
      }

      unsigned neighbor_idx = new_row * 256 + new_col;

      if (!worker->open_set_bool[neighbor_idx] &&
          !worker->closed_set[neighbor_idx] &&
          G_ValidCell(neighbors[i], i, worker->game->tiles)) {
        float tentative_g = current->g + worker->game->tiles[neighbor_idx].cost;

        if (tentative_g < worker->nodes[neighbor_idx].g) {
          float h = G_Distance(neighbors[i], target);
          h *= h;

          worker->nodes[neighbor_idx].g = tentative_g;
          worker->nodes[neighbor_idx].f = tentative_g + G_HeuristicWeight * h;
          worker->nodes[neighbor_idx].next = current;

          // worker->open_set[worker->open_set_count] =
          // &worker->nodes[neighbor_idx];
          C_QueueEnqueue(&worker->open_set, &worker->nodes[neighbor_idx]);
          worker->open_set_bool[neighbor_idx] = true;
          worker->open_set_count++;

          if (worker->open_set_count > max_open_set_count) {
            max_open_set_count = worker->open_set_count;
          }
        }
      }
    }
  }

  printf("didn't find anything...\n");
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
                [0] = ((float)(rand() % 8)) + 0.0f + 128.0f,
                [1] = ((float)(rand() % 8)) + 2.0f + 128.0f,
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
                [0] = 255.0f,
                [1] = 255.0f,
            },
    };

    game.transforms[i] = transform;
    game.cpu_agents[i] = cpu_agent;

    for (unsigned x = 0; x < 256; x++) {
      for (unsigned y = 0; y < 256; y++) {
        game.tiles[x * 256 + y].cost = 1.2;
      }
    }

    for (unsigned i = 0; i < 3000; i++) {
      int x = rand() % 128 + 4;
      int y = rand() % 128 + 4;

      if (x != 25 && y != 25) {
        game.tiles[x * 256 + y].cost = 999;
      }
    }
  }

  worker_t worker = {
      .game = &game,
      .nodes = calloc(256 * 256, sizeof(node_t)),
      .closed_set = calloc(256 * 256, sizeof(unsigned)),
      .open_set_bool = calloc(256 * 256, sizeof(unsigned)),
      .open_set_count = 0,
  };

  C_QueueNew(&worker.open_set, G_CompareNodes, 256 * 256);

  for (unsigned i = 0; i < 3000; i++) {
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

  printf("hello -> %d\n", max_open_set_count);

  return 0;
}
