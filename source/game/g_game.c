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

#include <SDL2/SDL_thread.h>
#include <cglm/cam.h>
#include <cglm/vec2.h>
#include <intlist.h>
#include <jps.h>

#define CORRIDOR 1

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
} __attribute__((aligned(16))) cpu_agent_t;

typedef struct node_t node_t;

typedef struct worker_t {

  game_t *game;
} worker_t;

struct game_t {
  vk_system_t *model_matrix_sys;
  vk_system_t *path_finding_sys;

  struct map *map;

  cpu_agent_t *cpu_agents;
  struct Agent *gpu_agents;
  struct Transform *transforms;
  struct Tile *gpu_tiles;
  unsigned entity_count;
  unsigned *entities;

  worker_t workers[8];
};

game_t *G_CreateGame(client_t *client, char *base) {
  // At the moment, randomly spawn 3000 pawns. And make them wander.
  game_t *game = calloc(1, sizeof(game_t));

  game->cpu_agents = calloc(3000, sizeof(cpu_agent_t));

  game->map = jps_create(256, 256);

  game->workers[0] = (worker_t){
      .game = game,
  };

  // Add "model" system at the end, just compute the model matrix for each
  // transform
  VK_AddSystem_Transform(CL_GetRend(client), "model_matrix system",
                         "model_matrix_frustum.comp.spv");

  return game;
}

void G_DestroyGame(game_t *game) {
  for (unsigned i = 0; i < 8; i++) {
  }
  free(game->cpu_agents);
  free(game);
}

void G_UpdateAgents(game_t *game, unsigned i) {
  if (game->cpu_agents[i].state == AGENT_PATH_FINDING) {
    // G_PathFinding(&game->workers[0], i);
    jps_set_start(game->map, game->transforms[i].position[0],
                  game->transforms[i].position[1]);
    jps_set_end(game->map, game->cpu_agents[i].target[0],
                game->cpu_agents[i].target[1]);

    IntList *list = il_create(2);
    jps_path_finding(game->map, 2, list);

    unsigned size = il_size(list);

    game->cpu_agents[i].computed_path.count = size;
    game->cpu_agents[i].computed_path.current = 0;
    game->cpu_agents[i].computed_path.points = calloc(size, sizeof(vec2));
    for (unsigned p = 0; p < size; p++) {
      int x = il_get(list, (size - p - 1), 0);
      int y = il_get(list, (size - p - 1), 1);
      game->cpu_agents[i].computed_path.points[p][0] = x;
      game->cpu_agents[i].computed_path.points[p][1] = y;
    }

    game->cpu_agents[i].state = AGENT_MOVING;
  } else if (game->cpu_agents[i].state == AGENT_MOVING) {
    unsigned c = game->cpu_agents[i].computed_path.current;
    if (c < game->cpu_agents[i].computed_path.count) {
      // Compute the direction to take
      vec2 next_pos;
      glm_vec2(game->cpu_agents[i].computed_path.points[c], next_pos);
      vec2 d;
      glm_vec2_sub(next_pos, game->transforms[i].position, d);
      vec2 s;
      glm_vec2_sign(d, s);
      d[0] = glm_min(fabs(d[0]), game->cpu_agents[i].speed) * s[0];
      d[1] = glm_min(fabs(d[1]), game->cpu_agents[i].speed) * s[1];

      // Reflect the direction on the related GPU agent
      // Visual and Animation is supposed to change
      vec2 supposed_d = {
          1.0f * s[0],
          1.0f * s[1],
      };
      if (supposed_d[0] != 0.0f || supposed_d[1] != 0.0f) {
        game->gpu_agents[i].direction[0] = supposed_d[0];
        game->gpu_agents[i].direction[1] = supposed_d[1];
      }

      // Apply the movement
      glm_vec2_add(game->transforms[i].position, d,
                   game->transforms[i].position);
      if (game->transforms[i].position[0] == next_pos[0] &&
          game->transforms[i].position[1] == next_pos[1]) {
        game->cpu_agents[i].computed_path.current++;
      }
    } else {
      game->cpu_agents[i].state = AGENT_NOTHING;
      game->gpu_agents[i].direction[0] = 0.0f;
      game->gpu_agents[i].direction[1] = 0.0f;
    }
  }
}

game_state_t G_TickGame(client_t *client, game_t *game) {
  game_state_t state;
  unsigned w, h;
  CL_GetViewDim(client, &w, &h);

  float ratio = (float)w / (float)h;

  glm_mat4_identity(state.fps.view);
  float zoom = 0.05f;
  float offset_x = 85.0f / 3.0f;
  float offset_y = 85.0f / 3.0f;
  glm_ortho(-1.0 * ratio / zoom + offset_x, 1.0 * ratio / zoom + offset_x,
            -1.0f / zoom + offset_y, 1.0f / zoom + offset_y, 0.01, 50.0,
            (vec4 *)&state.fps.view_proj);

  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);

  for (unsigned i = 0; i < game->entity_count; i++) {
    unsigned signature = game->entities[i];

    if (signature & agent_signature) {
      G_UpdateAgents(game, i);
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
  unsigned texture_count = 33;
  texture_t *textures = malloc(sizeof(texture_t) * texture_count);
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
  textures[13] = G_LoadSingleTexture("../base/walls/Brick_Wall_01.png");

  textures[14] = G_LoadSingleTexture("../base/furnitures/bed_east.png");
  textures[15] = G_LoadSingleTexture("../base/furnitures/bed_north.png");
  textures[16] = G_LoadSingleTexture("../base/furnitures/bed_south.png");

  for (unsigned t = 0; t < 16; t++) {
    char name[256];
    sprintf(name, "../base/walls/Brick_Wall_%02d.png", t);
    printf("loading %s walllll -> %d\n", name, 17 + t);
    textures[17 + t] = G_LoadSingleTexture(name);
  }

  for (unsigned i = 0; i < 10; i++) {
    unsigned texture = 0;

    struct Sprite sprite = {
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
                [0] = ((float)(rand() % 8) * 8.0f) + 0.0f + 60.0f,
                [1] = ((float)(rand() % 8) * 8.0f) + 2.0f + 60.0f,
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

  VK_UploadTextures(CL_GetRend(client), textures, texture_count);

  struct Tile tiles[256][256];

  for (unsigned x = 0; x < 256; x++) {
    for (unsigned y = 0; y < 256; y++) {
      tiles[x][y].texture = 12;
      tiles[x][y].wall = 0;
    }
  }

  if (CORRIDOR) {
    unsigned room_1_count = 0;
    unsigned *room_1 = malloc(sizeof(unsigned) * 512);

    G_Rectangle((ivec2){10, 10}, (ivec2){60, 20}, room_1, &room_1_count);

    for (unsigned i = 0; i < room_1_count; i++) {
      unsigned row = room_1[i] / 256;
      unsigned col = room_1[i] % 256;
      jps_set_obstacle(game->map, col, row, true);
      tiles[col][row].wall = 1;
      tiles[col][row].texture = 13;
    }

    unsigned row = room_1[9] / 256;
    unsigned col = room_1[9] % 256;
    jps_set_obstacle(game->map, col, row, false);
    tiles[col][row].wall = 0;
    tiles[col][row].texture = 12;

    G_Rectangle((ivec2){10, 20}, (ivec2){60, 30}, room_1, &room_1_count);

    for (unsigned i = 0; i < room_1_count; i++) {
      unsigned row = room_1[i] / 256;
      unsigned col = room_1[i] % 256;
      jps_set_obstacle(game->map, col, row, true);
      tiles[col][row].wall = 1;
      tiles[col][row].texture = 13;
    }

    row = room_1[40] / 256;
    col = room_1[40] % 256;
    jps_set_obstacle(game->map, col, row, false);
    tiles[col][row].wall = 0;
    tiles[col][row].texture = 12;

    G_Rectangle((ivec2){10, 30}, (ivec2){60, 40}, room_1, &room_1_count);

    for (unsigned i = 0; i < room_1_count; i++) {
      unsigned row = room_1[i] / 256;
      unsigned col = room_1[i] % 256;
      jps_set_obstacle(game->map, col, row, true);
      tiles[col][row].wall = 1;
      tiles[col][row].texture = 13;
    }

    row = room_1[9] / 256;
    col = room_1[9] % 256;
    jps_set_obstacle(game->map, col, row, false);
    tiles[col][row].wall = 0;
    tiles[col][row].texture = 12;
  } else {
    for (unsigned i = 0; i < 10000; i++) {
      unsigned row = rand() % 256;
      unsigned col = rand() % 256;

      if (row != 0 && col != 0) {
        jps_set_obstacle(game->map, col, row, true);
        tiles[col][row].wall = 1;
        tiles[col][row].texture = 13;
      }
    }
  }

  jps_mark_connected(game->map);
  jps_dump_connected(game->map);

  VK_SetMap(CL_GetRend(client), &tiles[0][0], 256, 256);

  struct Immovable bed = {
      .size = {1.0, 2.0},
  };

  G_AddFurniture(client, game,
                 &(struct Transform){
                     .position = {11.0f, 11.0f, 10.0f, 1.0f},
                     .rotation = {0.0f, 0.0f, 0.0f},
                     .scale = {1.0, 1.0, 1.0, 1.0},
                 },
                 &(struct Sprite){
                     .texture_east = 14,
                     .texture_north = 15,
                     .texture_south = 16,
                     .current = 15,
                 },
                 &bed);

  bed = (struct Immovable){
      .size = {2.0, 1.0},
  };
  G_AddFurniture(client, game,
                 &(struct Transform){
                     .position = {12.0f, 11.0f, 10.0f, 1.0f},
                     .rotation = {0.0f, 0.0f, 0.0f},
                     .scale = {1.0, 1.0, 1.0, 1.0},
                 },
                 &(struct Sprite){
                     .texture_east = 14,
                     .texture_north = 15,
                     .texture_south = 16,
                     .current = 14,
                 },
                 &bed);

  // Get the mapped data from the renderer
  game->gpu_agents = VK_GetAgents(CL_GetRend(client));
  game->transforms = VK_GetTransforms(CL_GetRend(client));
  game->gpu_tiles = VK_GetMap(CL_GetRend(client));
  game->entities = VK_GetEntities(CL_GetRend(client));

  return true;
}

void G_AddFurniture(client_t *client, game_t *game, struct Transform *transform,
                    struct Sprite *sprite, struct Immovable *immovable) {
  game->entity_count += 1;
  vk_rend_t *rend = CL_GetRend(client);
  unsigned entity =
      VK_Add_Entity(rend, transform_signature | model_transform_signature |
                              sprite_signature | immovable_signature);

  VK_Add_Transform(rend, entity, transform);
  VK_Add_Model_Transform(rend, entity, NULL);
  VK_Add_Sprite(rend, entity, sprite);
  VK_Add_Immovable(rend, entity, immovable);
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
  };

  cpu_agent_t cpu_agent = {
      .state = AGENT_PATH_FINDING,
      .speed = 0.2,
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
