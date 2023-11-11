#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "client/cl_client.h"
#include "common/c_queue.h"
#include "game/g_game.h"
#include "stbi_image.h"
#include "vk/vk_system.h"
#include "vk/vk_vulkan.h"
#include <cglm/cam.h>
#include <cglm/vec2.h>

#include <SDL2/SDL_thread.h>

#define CORRIDOR 1

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
  vec2 position;
  float g;
  float h;
  float f;

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
  struct Tile *gpu_tiles;
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

float G_Distance(const vec2 a, const vec2 b) {
  // The Diagonal distance
  // from https://theory.stanford.edu/~amitp/GameProgramming/Heuristics.html#S7
  float dx = fabsf(a[0] - b[0]);
  float dy = fabsf(a[1] - b[1]);

  float D1 = 1.0;         // Cost of moving horizontally
  float D2 = sqrtf(2.0f); // Cost of moving diagonally
  
  return D1 * (dx + dy) + (D2 - 2 * D1) * fminf(dx, dy);
}

bool G_HasForcedNeighbors(const vec2 new_point, const vec2 next_point,
                          unsigned d, const cpu_tile_t *tiles) {
  int cn1x = new_point[0] + G_NodeDirections[d][1];
  int cn1y = new_point[1] + G_NodeDirections[d][0];
  int cn1_idx = cn1y * 256 + cn1x;

  int cn2x = new_point[0] - G_NodeDirections[d][1];
  int cn2y = new_point[1] - G_NodeDirections[d][0];
  int cn2_idx = cn2y * 256 + cn2x;

  int nn1x = next_point[0] + G_NodeDirections[d][1];
  int nn1y = next_point[1] + G_NodeDirections[d][0];
  int nn1_idx = nn1y * 256 + nn1x;

  int nn2x = next_point[0] - G_NodeDirections[d][1];
  int nn2y = next_point[1] - G_NodeDirections[d][0];
  int nn2_idx = nn2y * 256 + nn2x;

  bool a = !(cn1x < 0 || cn1y < 0 || cn1x >= 256 || cn1y >= 256 ||
             tiles[cn1_idx].wall == 1);
  bool b = !(nn1x < 0 || nn1y < 0 || nn1x >= 256 || nn1y >= 256 ||
             tiles[nn1_idx].wall == 1);
  if (a != b) {
    return true;
  }

  a = !(cn2x < 0 || cn2y < 0 || cn2x >= 256 || cn2y >= 256 ||
        tiles[cn2_idx].wall == 1);
  b = !(nn2x < 0 || nn2y < 0 || nn2x >= 256 || nn2y >= 256 ||
        tiles[nn2_idx].wall == 1);
  return a != b;
}

node_t *G_Jump(const vec2 position, const vec2 target, unsigned d,
               worker_t *worker) {
  if (d > 8) {
    return NULL;
  }

  vec2 next_position;
  glm_vec2_add((float *)position, G_NodeDirections[d], next_position);

  int col = (int)(next_position[0]);
  int row = (int)(next_position[1]);
  int idx = row * 256 + col;

  unsigned is_wall = worker->game->tiles[row].wall;

  if (!(next_position[0] < 0 && next_position[1] < 0 &&
        next_position[0] >= 256 && next_position[1] >= 256 && !is_wall)) {
    return NULL;
  }

  if (worker->state_set[idx] != 2) {
    // weird, the implem does "if it contains, add it"...
    // but i guess we want to prune it now
    worker->state_set[idx] = 2;
  }

  if (next_position[0] == target[0] && next_position[1] == target[1]) {
    return &worker->nodes[idx];
  }

  bool forced_neighbors =
      G_HasForcedNeighbors(position, next_position, d, worker->game->tiles);

  if (forced_neighbors) {
    return &worker->nodes[idx];
  }

  node_t *jump_node = G_Jump(next_position, target, d, worker);

  // over-shoot????

  return jump_node;
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

int G_CompareNodes(const void *a, const void *b) {
  return ((node_t *)b)->f - ((node_t *)a)->f;
}

void G_PathFinding(worker_t *worker, unsigned entity) {
  for (unsigned x = 0; x < 256; x++) {
    for (unsigned y = 0; y < 256; y++) {
      unsigned n_idx = x + y * 256;
      worker->nodes[n_idx].position[0] = (float)x;
      worker->nodes[n_idx].position[1] = (float)y;
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
        tentative_g += sqrtf(2.0f);
      } else {
        tentative_g += 1.0;
      }

      if (tentative_g < worker->nodes[neighbor_idx].g) {
        node_t *neighbor_node = &worker->nodes[neighbor_idx];
        neighbor_node->h = G_Distance(neighbor, target);
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

game_t *G_CreateGame(client_t *client, char *base) {
  // At the moment, randomly spawn 3000 pawns. And make them wander.
  game_t *game = calloc(1, sizeof(game_t));

  game->cpu_agents = calloc(3000, sizeof(cpu_agent_t));
  game->tiles = calloc(256 * 256, sizeof(cpu_tile_t));

  game->workers[0] = (worker_t){
      .game = game,
      .nodes = calloc(256 * 256, sizeof(node_t)),
      .state_set = calloc(256 * 256, sizeof(unsigned)),
      .open_set_bool = calloc(256 * 256, sizeof(unsigned)),
      .open_set_count = 0,
  };

  C_QueueNew(&game->workers[0].open_set, G_CompareNodes, 256 * 256);

  // Add "model" system at the end, just compute the model matrix for each
  // transform
  VK_AddSystem_Transform(CL_GetRend(client), "model_matrix system",
                         "model_matrix_frustum.comp.spv");

  return game;
}

void G_DestroyGame(game_t *game) {
  for (unsigned i = 0; i < 8; i++) {
    if (game->workers[i].state_set) {
      free(game->workers[i].state_set);
      C_QueueDestroy(&game->workers[i].open_set);
      free(game->workers[i].nodes);
    }
  }
  free(game->cpu_agents);
  free(game->tiles);
  free(game);
}

game_state_t G_TickGame(client_t *client, game_t *game) {
  game_state_t state;
  unsigned w, h;
  CL_GetViewDim(client, &w, &h);

  float ratio = (float)w / (float)h;

  glm_mat4_identity(state.fps.view);
  float zoom = 0.03f;
  float offset_x = 45.0f;
  float offset_y = 32.0f;
  glm_ortho(-1.0 * ratio / zoom + offset_x, 1.0 * ratio / zoom + offset_x,
            -1.0f / zoom + offset_y, 1.0f / zoom + offset_y, 0.01, 50.0,
            (vec4 *)&state.fps.view_proj);

  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);

  for (unsigned i = 0; i < game->entity_count; i++) {
    if (game->cpu_agents[i].state == AGENT_PATH_FINDING) {
      G_PathFinding(&game->workers[0], i);
    } else if (game->cpu_agents[i].state == AGENT_MOVING) {
      unsigned c = game->cpu_agents[i].computed_path.current;
      if (c < game->cpu_agents[i].computed_path.count) {
        vec2 next_pos;
        glm_vec2(game->cpu_agents[i].computed_path.points[c], next_pos);
        vec2 d;
        glm_vec2_sub(next_pos, game->transforms[i].position, d);
        vec2 s;
        glm_vec2_sign(d, s);
        d[0] = glm_min(fabs(d[0]), 0.8) * s[0];
        d[1] = glm_min(fabs(d[1]), 0.8) * s[1];

        glm_vec2_add(game->transforms[i].position, d,
                     game->transforms[i].position);
        if (game->transforms[i].position[0] == next_pos[0] &&
            game->transforms[i].position[1] == next_pos[1]) {
          game->cpu_agents[i].computed_path.current++;
        }
      } else {
        game->cpu_agents[i].state = AGENT_NOTHING;
      }
    }
  }

  clock_gettime(CLOCK_MONOTONIC_RAW, &end);

  time_t t = (end.tv_sec - start.tv_sec) * 1000000 +
             (end.tv_nsec - start.tv_nsec) / 1000;
  t /= 1000;
  if (t > 10) {
    printf("Anormaly long update time... %ldms\n", t);
  }

  VK_TickSystems(CL_GetRend(client));

  return state;
}

texture_t G_LoadSingleTexture(const char *path) {
  stbi_set_flip_vertically_on_load(true);
  int w, h, c;
  unsigned char *data = stbi_load(path, &w, &h, &c, 4);

  return (texture_t){
      .c = c,
      .data = data,
      .height = h,
      .width = w,
      .label = path,
  };
}

bool G_Load(client_t *client, game_t *game) {
  game->cpu_agents = calloc(3000, sizeof(cpu_agent_t));
  time_t seed = time(NULL);
  srand(seed);
  texture_t *textures = malloc(sizeof(texture_t) * 14);
  // Just load the hardcoded animals atm
  textures[0] = G_LoadSingleTexture("../base/Bison_east.png");
  textures[1] = G_LoadSingleTexture("../base/Bison_north.png");
  textures[2] = G_LoadSingleTexture("../base/Bison_south.png");

  textures[3] = G_LoadSingleTexture("../base/Thrumbo_east.png");
  textures[4] = G_LoadSingleTexture("../base/Thrumbo_north.png");
  textures[5] = G_LoadSingleTexture("../base/Thrumbo_south.png");

  textures[6] = G_LoadSingleTexture("../base/YorkshireTerrier_east.png");
  textures[7] = G_LoadSingleTexture("../base/YorkshireTerrier_north.png");
  textures[8] = G_LoadSingleTexture("../base/YorkshireTerrier_south.png");

  textures[9] = G_LoadSingleTexture("../base/DuckMale_east.png");
  textures[10] = G_LoadSingleTexture("../base/DuckMale_north.png");
  textures[11] = G_LoadSingleTexture("../base/DuckMale_south.png");

  textures[12] = G_LoadSingleTexture("../base/dirt.png");
  textures[13] = G_LoadSingleTexture("../base/Wall_single.png");

  for (unsigned i = 0; i < 100; i++) {
    unsigned texture = 0;

    struct Sprite sprite = {
        .current = texture,
        .texture_east = texture,
        .texture_north = texture + 1,
        .texture_south = texture + 2,
    };

    float scale = 0.0f;

    switch (texture) {
    case 0: {
      scale = 2.4;
      break;
    }
    case 3: {
      scale = 4.0;
      break;
    }
    case 6: {
      scale = 0.32;
      break;
    }
    case 9: {
      scale = 0.3;
      break;
    }
    }

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
                [0] = scale,
                [1] = scale,
                [2] = scale,
                [3] = 1.0f,
            },
    };
    G_AddPawn(client, game, &transform, &sprite);
  }

  VK_UploadTextures(CL_GetRend(client), textures, 14);

  struct Tile tiles[256][256];

  for (unsigned x = 0; x < 256; x++) {
    for (unsigned y = 0; y < 256; y++) {
      tiles[x][y].texture = 12;
      tiles[x][y].wall = 0;

      game->tiles[y * 256 + x].wall = 0;
    }
  }

  if (CORRIDOR) {
    unsigned room_1_count = 0;
    unsigned *room_1 = malloc(sizeof(unsigned) * 512);

    G_Rectangle((ivec2){10, 10}, (ivec2){60, 20}, room_1, &room_1_count);

    for (unsigned i = 0; i < room_1_count; i++) {
      game->tiles[room_1[i]].wall = 1;
      unsigned row = room_1[i] / 256;
      unsigned col = room_1[i] % 256;
      tiles[col][row].wall = 1;
      tiles[col][row].texture = 13;
    }

    game->tiles[room_1[9]].wall = 0;
    unsigned row = room_1[9] / 256;
    unsigned col = room_1[9] % 256;
    tiles[col][row].wall = 0;
    tiles[col][row].texture = 12;

    G_Rectangle((ivec2){10, 20}, (ivec2){60, 30}, room_1, &room_1_count);

    for (unsigned i = 0; i < room_1_count; i++) {
      game->tiles[room_1[i]].wall = 1;
      unsigned row = room_1[i] / 256;
      unsigned col = room_1[i] % 256;
      tiles[col][row].wall = 1;
      tiles[col][row].texture = 13;
    }

    game->tiles[room_1[40]].wall = 0;
    row = room_1[40] / 256;
    col = room_1[40] % 256;
    tiles[col][row].wall = 0;
    tiles[col][row].texture = 12;

    G_Rectangle((ivec2){10, 30}, (ivec2){60, 40}, room_1, &room_1_count);

    for (unsigned i = 0; i < room_1_count; i++) {
      game->tiles[room_1[i]].wall = 1;
      unsigned row = room_1[i] / 256;
      unsigned col = room_1[i] % 256;
      tiles[col][row].wall = 1;
      tiles[col][row].texture = 13;
    }

    game->tiles[room_1[9]].wall = 0;
    row = room_1[9] / 256;
    col = room_1[9] % 256;
    tiles[col][row].wall = 0;
    tiles[col][row].texture = 12;
  } else {
    for (unsigned i = 0; i < 9000; i++) {
      unsigned row = rand() % 256;
      unsigned col = rand() % 256;

      if (row != 0 && col != 0) {
        game->tiles[row * 256 + col].wall = 1;
        tiles[col][row].wall = 1;
        tiles[col][row].texture = 13;
      }
    }
  }

  VK_SetMap(CL_GetRend(client), &tiles[0][0], 256, 256);

  // Get the mapped data from the renderer
  game->gpu_agents = VK_GetAgents(CL_GetRend(client));
  game->transforms = VK_GetTransforms(CL_GetRend(client));
  game->gpu_tiles = VK_GetMap(CL_GetRend(client));

  return true;
}

void G_AddPawn(client_t *client, game_t *game, struct Transform *transform,
               struct Sprite *sprite) {
  game->entity_count += 1;
  vk_rend_t *rend = CL_GetRend(client);
  unsigned entity =
      VK_Add_Entity(rend, transform_signature | model_transform_signature |
                              agent_signature | sprite_signature);

  struct Agent agent = {
      .direction =
          {
              [0] = 0.0f,
              [1] = 0.0f,
          },
      .moving = false,
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

  game->cpu_agents[entity] = cpu_agent;

  VK_Add_Transform(rend, entity, transform);
  VK_Add_Model_Transform(rend, entity, NULL);
  VK_Add_Agent(rend, entity, &agent);
  VK_Add_Sprite(rend, entity, sprite);
}
