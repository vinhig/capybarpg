#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "client/cl_client.h"
#include "game/g_game.h"
#include "stbi_image.h"
#include "vk/vk_system.h"
#include "vk/vk_vulkan.h"
#include <cglm/cam.h>
#include <cglm/vec2.h>

#include <SDL2/SDL_thread.h>

typedef struct cpu_path_t {
  vec2 *points;
  unsigned count;
  unsigned current;
} cpu_path_t;

typedef struct cpu_agent_t {
  vec2 target;
  float speed;
  enum agent_state_e {
    AGENT_PATH_FINDING,
    AGENT_MOVING,
    AGENT_NOTHING,
  } state;

  cpu_path_t computed_path;
} cpu_agent_t;

typedef struct node_t node_t;

struct node_t {
  vec2 position;
  float g;
  float h;
  float f;

  node_t *next;
};

typedef struct worker_t {
  // Stuff when path finding is needed
  node_t *open_set;
  node_t *closed_set;

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
  struct Tile *tiles;
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

// float G_Distance(vec2 a, vec2 b) {
  // return sqrtf(powf(a[0] - b[0], 2.0) + powf(a[1] - b[1], 2.0));
// }

float G_Distance(vec2 a, vec2 b) {
  return fabs(a[0] - b[0]) + fabs(a[1] - b[1]);
}

bool G_ValidCell(vec2 position, vec2 d, struct Tile *tiles) {
  if (!(position[0] >= 0 && position[1] >= 0 && position[0] < 256 &&
        position[1] < 256)) {
    return false;
  }

  float cost = tiles[(int)(position[1] * 256 + position[0])].cost;
  if (cost == 999) {
    return false;
  }

  // Check for diagonal
  if (fabs(d[0]) == 1 && fabs(d[1]) == 1) {
    float through_a;
    float through_b;
    {
      unsigned check_col = position[0] - d[0] + d[0];
      unsigned check_row = position[1] - d[1];

      through_a = tiles[check_row * 256 + check_col].cost;
    }
    {
      unsigned check_row = position[0]- d[0];
      unsigned check_col = position[1]- d[1] + d[1];

      through_b = tiles[check_row * 256 + check_col].cost;
    }

    if (through_a == 999.0f || through_b == 999.0f) {
      return false;
    }

    // exit(-1);
  }

  return true;
}

void G_PathFinding(worker_t *worker, unsigned entity) {
  node_t nodes[256][256];

  for (unsigned x = 0; x < 256; x++) {
    for (unsigned y = 0; y < 256; y++) {
      nodes[x][y].position[0] = (float)x;
      nodes[x][y].position[1] = (float)y;
      nodes[x][y].f = INT32_MAX;
      nodes[x][y].g = INT32_MAX;
      nodes[x][y].next = NULL;
    }
  }

  bool closed_list_bool[256][256];
  for (unsigned x = 0; x < 256; x++) {
    for (unsigned y = 0; y < 256; y++) {
      closed_list_bool[x][y] = false;
    }
  }

  cpu_agent_t *agent = &worker->game->cpu_agents[entity];
  struct Transform *transform = &worker->game->transforms[entity];
  nodes[(int)transform->position[0]][(int)transform->position[1]].f =
      G_Distance(transform->position, agent->target);
  nodes[(int)transform->position[0]][(int)transform->position[1]].h =
      G_Distance(transform->position, agent->target);
  nodes[(int)transform->position[0]][(int)transform->position[1]].g = 0.0f;

  node_t *open_list[256 * 256];
  open_list[0] =
      &nodes[(int)transform->position[0]][(int)transform->position[1]];
  unsigned open_list_count = 1;

  int iteration = 0;
  while (open_list_count > 0) {
    iteration++;

    unsigned current_idx = 0;
    for (unsigned i = 0; i < open_list_count; i++) {
      if (open_list[i]->f < open_list[current_idx]->f) {
        current_idx = i;
      }
    }

    node_t *current = open_list[current_idx];

    for (unsigned i = current_idx; i < open_list_count - 1; i++) {
      open_list[i] = open_list[i + 1];
    }
    open_list_count--;

    closed_list_bool[(int)current->position[0]][(int)current->position[1]] =
        true;

    if (current->position[0] == agent->target[0] &&
        current->position[1] == agent->target[1]) {
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

    vec2 directions[8];
    glm_vec2(&(vec2){1, 0}[0], directions[0]);
    glm_vec2(&(vec2){0, 1}[0], directions[1]);
    glm_vec2(&(vec2){0, -1}[0], directions[2]);
    glm_vec2(&(vec2){-1, 0}[0], directions[3]);
    glm_vec2(&(vec2){1, 1}[0], directions[4]);
    glm_vec2(&(vec2){-1, 1}[0], directions[5]);
    glm_vec2(&(vec2){-1, -1}[0], directions[6]);
    glm_vec2(&(vec2){1, -1}[0], directions[7]);

    vec2 neighbors[8];
    glm_vec2_add(current->position, directions[0], neighbors[0]);
    glm_vec2_add(current->position, directions[1], neighbors[1]);
    glm_vec2_add(current->position, directions[2], neighbors[2]);
    glm_vec2_add(current->position, directions[3], neighbors[3]);
    glm_vec2_add(current->position, directions[4], neighbors[4]);
    glm_vec2_add(current->position, directions[5], neighbors[5]);
    glm_vec2_add(current->position, directions[6], neighbors[6]);
    glm_vec2_add(current->position, directions[7], neighbors[7]);

    for (unsigned i = 0; i < 8; i++) {
      int new_row = (int)neighbors[i][0];
      int new_col = (int)neighbors[i][1];

      if (G_ValidCell(neighbors[i], directions[i], worker->game->tiles) &&
          !closed_list_bool[new_row][new_col]) {
        float tentative_g =
            current->g + worker->game->tiles[new_row * 256 + new_col].cost;

        if (tentative_g < nodes[new_row][new_col].g) {
          float h = G_Distance(neighbors[i], agent->target);

          nodes[new_row][new_col].g = tentative_g;
          nodes[new_row][new_col].f = tentative_g + h;
          nodes[new_row][new_col].next = current;

          open_list[open_list_count] = &nodes[new_row][new_col];
          open_list_count++;
        }
      }
    }
  }

  printf("didn't find anything...\n");
}

game_t *G_CreateGame(client_t *client, char *base) {
  // At the moment, randomly spawn 3000 pawns. And make them wander.
  game_t *game = calloc(1, sizeof(game_t));

  game->workers[0].open_set = calloc(256 * 256, sizeof(node_t));
  game->workers[0].closed_set = calloc(256 * 256, sizeof(node_t));
  game->workers[0].game = game;

  // Add "model" system at the end, just compute the model matrix for each
  // transform
  VK_AddSystem_Transform(CL_GetRend(client), "model_matrix system",
                         "model_matrix_frustum.comp.spv");

  return game;
}

void G_DestroyGame(game_t *game) { free(game); }

game_state_t G_TickGame(client_t *client, game_t *game) {
  game_state_t state;
  unsigned w, h;
  CL_GetViewDim(client, &w, &h);

  float ratio = (float)w / (float)h;

  glm_mat4_identity(state.fps.view);
  float zoom = 0.05f;
  float offset_x = 14.0f;
  float offset_y = 14.0f;
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
        d[0] = glm_min(fabs(d[0]), 0.1) * s[0];
        d[1] = glm_min(fabs(d[1]), 0.1) * s[1];

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
  if (t > 300) {
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

  for (unsigned i = 0; i < 300; i++) {
    unsigned texture = rand() % 4 * 3;

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
                [0] = ((float)(rand() % 8)) + 0.0f + 32.0f,
                [1] = ((float)(rand() % 8)) + 2.0f + 32.0f,
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
      tiles[x][y].cost = 1.2;
    }
  }

  for (unsigned i = 0; i < 3000; i++) {

    int x = rand() % 128 + 4;
    int y = rand() % 128 + 4;

    if (x != 25 && y != 25) {
      tiles[x][y].texture = 13;
      tiles[x][y].cost = 999;
    }
  }

  VK_SetMap(CL_GetRend(client), &tiles[0][0], 256, 256);

  // Get the mapped data from the renderer
  game->gpu_agents = VK_GetAgents(CL_GetRend(client));
  game->transforms = VK_GetTransforms(CL_GetRend(client));
  game->tiles = VK_GetMap(CL_GetRend(client));

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
              [0] = 0.0f,
              [1] = 0.0f,
          },
  };

  game->cpu_agents[entity] = cpu_agent;

  VK_Add_Transform(rend, entity, transform);
  VK_Add_Model_Transform(rend, entity, NULL);
  VK_Add_Agent(rend, entity, &agent);
  VK_Add_Sprite(rend, entity, sprite);
}
