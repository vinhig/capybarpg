#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "client/cl_client.h"
#include "game/g_game.h"
#include "stbi_image.h"
#include "vk/vk_system.h"
#include "vk/vk_vulkan.h"
#include <cglm/cam.h>

struct game_t {
  vk_system_t *model_matrix_sys;
  vk_system_t *path_finding_sys;
};

game_t *G_CreateGame(client_t *client, char *base) {
  // At the moment, randomly spawn 3000 pawns. And make them wander.
  game_t *game = calloc(1, sizeof(game_t));

  // Add "path finding" system
  VK_AddSystem_Agent_Transform(CL_GetRend(client), "path_finding system",
                               "path_finding.comp.spv");

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
  // glm_ortho(-1.0 * ratio, 1.0* ratio, -1.0f, 1.0f , 0.01, 50.0,
  //           &state.fps.proj);
  float zoom = 0.2;
  glm_ortho(-1.0 * ratio / zoom, 1.0 * ratio / zoom, -1.0f / zoom, 1.0f / zoom,
            0.01, 50.0, (vec4 *)&state.fps.view_proj);
  // glm_ortho_default(ratio, &state.fps.view_proj);

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
  srand(time(NULL));
  texture_t *textures = malloc(sizeof(texture_t) * 12);
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

  for (unsigned i = 0; i < 18; i++) {
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
                [0] = ((float)(rand() % 20) - 10.0f) / 5.0f,
                [1] = ((float)(rand() % 20) - 10.0f) / 5.0f,
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

  VK_UploadTextures(CL_GetRend(client), textures, 12);

  return true;
}

void G_AddPawn(client_t *client, game_t *game, struct Transform *transform,
               struct Sprite *sprite) {
  vk_rend_t *rend = CL_GetRend(client);
  unsigned entity =
      VK_Add_Entity(rend, transform_signature | model_transform_signature |
                              agent_signature | sprite_signature);

  struct Agent agent = {
      .target =
          {
              [0] = 0.0f,
              [1] = 0.0f,
              [3] = 1.0f,
          },
      .speed = 1.0f,
  };

  VK_Add_Transform(rend, entity, transform);
  VK_Add_Model_Transform(rend, entity, NULL);
  VK_Add_Agent(rend, entity, &agent);
  VK_Add_Sprite(rend, entity, sprite);
}
