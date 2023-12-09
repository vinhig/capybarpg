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
#include <qcvm/qcvm.h>
#include <zpl/zpl.h>
//
#include <qclib/qclib.h>

#define CORRIDOR 1

ZPL_TABLE_DECLARE(extern, material_bank_t, G_Materials_, material_t)
ZPL_TABLE_DECLARE(extern, texture_bank_t, G_Textures_, texture_t)

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
  // Hey hey, I heard QCVM wasn't thread-safe. So i just init a QCVM for each
  // worker thread. Hope you don't mind!
  qcvm_t *qcvm;

  unsigned id;
  game_t *game;
} worker_t;

typedef enum listener_type_t {
  G_SCENE_START,
  G_SCENE_END,
  G_SCENE_UPDATE,
  G_LISTENER_TYPE_COUNT,
} listener_type_t;

typedef struct listener_t {
  int qcvm_func;
} listener_t;

typedef struct scene_t {
  const char *name;

  listener_t start_listeners[16];
  unsigned start_listener_count;

  listener_t update_listeners[16];
  unsigned update_listener_count;

  listener_t end_listeners[16];
  unsigned end_listener_count;
} scene_t;

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

  unsigned worker_count;
  worker_t workers[8];

  char *base;

  scene_t *scenes;
  unsigned scene_count;
  unsigned scene_capacity;

  scene_t *current_scene;

  // Asset banks
  material_bank_t material_bank;
  texture_bank_t texture_bank;

  // Game state, reset each frame
  // The renderer use this to draw the frame
  game_state_t state;

  // Hello
  vk_rend_t *rend;
  client_t *client;
};

game_t *G_CreateGame(client_t *client, char *base) {
  // At the moment, randomly spawn 3000 pawns. And make them wander.
  game_t *game = calloc(1, sizeof(game_t));

  game->cpu_agents = calloc(3000, sizeof(cpu_agent_t));

  game->map = jps_create(256, 256);
  game->base = base;

  game->client = client;
  game->rend = CL_GetRend(client);

  game->workers[0] = (worker_t){
      .game = game,
  };

  G_Materials_init(&game->material_bank, zpl_heap_allocator());
  G_Textures_init(&game->texture_bank, zpl_heap_allocator());

  // Add "model" system at the end, just compute the model matrix for each
  // transform
  VK_AddSystem_Transform(game->rend, "model_matrix system",
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

void G_ResetGameState(game_t *game) { game->state = (game_state_t){}; }

game_state_t G_TickGame(client_t *client, game_t *game) {
  G_ResetGameState(game);

  unsigned w, h;
  CL_GetViewDim(client, &w, &h);

  float ratio = (float)w / (float)h;

  glm_mat4_identity(game->state.fps.view);
  float zoom = 0.05f;
  float offset_x = 85.0f / 3.0f;
  float offset_y = 85.0f / 3.0f;
  glm_ortho(-1.0 * ratio / zoom + offset_x, 1.0 * ratio / zoom + offset_x,
            -1.0f / zoom + offset_y, 1.0f / zoom + offset_y, 0.01, 50.0,
            (vec4 *)&game->state.fps.view_proj);

  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);

  if (!game->current_scene) {
    printf("[WARNING] No current scene hehe...\n");
  } else {
    for (unsigned i = 0; i < game->current_scene->update_listener_count; i++) {
      qcvm_run(game->workers[0].qcvm,
               game->current_scene->update_listeners[i].qcvm_func);
    }

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
  }

  VK_TickSystems(game->rend);

  return game->state;
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

int G_Create_Map(game_t *game, unsigned w, unsigned h) { return -1; }

void G_Create_Map_QC(qcvm_t *qcvm) {
  worker_t *worker = (worker_t *)qcvm_get_user_data(qcvm);
  game_t *game = worker->game;

  if (worker->id != 0) {
    printf(
        "`G_Create_Map_QC` can only be called from thread 0 (main thread).\n");
    return;
  }

  float w = qcvm_get_parm_float(qcvm, 0);
  float h = qcvm_get_parm_float(qcvm, 1);

  if (w <= 0.0f) {
    printf("[ERROR] Can't initialize a map with a negative or null width (%f "
           "was specified).\n",
           w);
    qcvm_return_int(qcvm, -1);
    return;
  }

  if (h <= 0.0f) {
    printf("[ERROR] Can't initialize a map with a negative or null height (%f "
           "was specified).\n",
           w);
    qcvm_return_int(qcvm, -1);
    return;
  }

  qcvm_return_int(qcvm, G_Create_Map(game, w, h));
}

bool G_Add_Recipes(game_t *game, const char *path, bool required) {
  char base_path[256];
  sprintf(&base_path[0], "%s/%s", game->base, path);
  FILE *f = fopen(base_path, "r");

  if (!f) {
    printf("[WARNING] The recipes file `%s`|`%s` doesn't seem to exist (skill "
           "issue).\n",
           path, base_path);
    return 0;
  }

  fseek(f, 0, SEEK_END);
  size_t size = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *content = malloc(size);
  fread(content, 1, size, f);
  zpl_json_object recipes = {0};
  zpl_u8 err;
  err = zpl_json_parse(&recipes, content, zpl_heap_allocator());

  if (err != ZPL_JSON_ERROR_NONE) {
    printf("[ERROR] The recipes file `%s` isn't a valid JSON.\n", path);
    return 0;
  }

  // It's a valid JSON
  // Let's parse it, yeah!

  // A recipe can have these sections:
  // * walls
  // * furnitures
  // * items
  // * materials
  // * animals
  // We extract them here in a specific order, cause there are some dependencies
  // (furnitures and walls are built with materials for example)
  zpl_json_object *materials = zpl_adt_find(&recipes, "materials", false);
  zpl_json_object *walls = zpl_adt_find(&recipes, "walls", false);
  zpl_json_object *furnitures = zpl_adt_find(&recipes, "furnitures", false);
  zpl_json_object *items = zpl_adt_find(&recipes, "items", false);

  // The info extraction is a bit weird as mod can overwrite what was previously
  // specified. So if there isn't a element of a specific type in the asset bank
  // with that name, we create it. Otherwise, we simply update its properties
  // (for example a mod "Stronger Walls" doesn't have to re-specify every single
  // sprites, just the health). So the identifier is the only mandatory field.
  // Yes I know something like Rust or Go would have been smarter than all this
  // manual bullshit.

  if (materials && materials->type == ZPL_ADT_TYPE_ARRAY) {
    unsigned size = zpl_array_count(materials->nodes);
    for (unsigned i = 0; i < size; i++) {
      zpl_adt_node *element = &materials->nodes[i];
      zpl_adt_node *id_node = zpl_adt_find(element, "id", false);

      if (!id_node || id_node->type != ZPL_ADT_TYPE_STRING) {
        printf("[WARNING] The element %d in the 'materials' list doesn't have "
               "an ID (of type string).\n",
               i);
        continue;
      } else {
        zpl_u64 key = zpl_fnv64(id_node->string, strlen(id_node->string));
        // Does the bank contains this element? If not create an empty one.
        material_t *material = G_Materials_get(&game->material_bank, key);

        if (!material) {
          G_Materials_set(&game->material_bank, key, (material_t){});
          material_t *material = G_Materials_get(&game->material_bank, key);
        }

        // Extract all relevant info from the JSON field
        zpl_adt_node *name_node = zpl_adt_find(element, "name", false);
        zpl_adt_node *stack_size_node = zpl_adt_find(element, "name", false);
        zpl_adt_node *sprites_node = zpl_adt_find(element, "sprites", false);

        zpl_adt_node *low_stack_node = NULL;
        zpl_adt_node *half_stack_node = NULL;
        zpl_adt_node *full_stack_node = NULL;

        // Some field may be NULL if not present, we don't care
        if (name_node && name_node->type == ZPL_ADT_TYPE_STRING) {
        }

        if (stack_size_node && stack_size_node->type == ZPL_ADT_TYPE_INTEGER) {
        }

        if (sprites_node && sprites_node->type == ZPL_ADT_TYPE_OBJECT) {
          low_stack_node = zpl_adt_find(sprites_node, "low_stack", false);
          half_stack_node = zpl_adt_find(sprites_node, "half_stack", false);
          full_stack_node = zpl_adt_find(sprites_node, "full_stack", false);
        }
      }
    }
  } else {
    printf("[DEBUG] No 'materials' specified in `%s`.\n", path);
  }

  zpl_json_free(&recipes);
  return true;
}

void G_Add_Recipes_QC(qcvm_t *qcvm) {
  worker_t *worker = (worker_t *)qcvm_get_user_data(qcvm);
  game_t *game = worker->game;

  if (worker->id != 0) {
    printf(
        "`G_Add_Recipes_QC` can only be called from thread 0 (main thread).\n");
    return;
  }

  const char *path = qcvm_get_parm_string(qcvm, 0);
  bool required = qcvm_get_parm_int(qcvm, 1);

  qcvm_return_int(qcvm, (int)G_Add_Recipes(game, path, required));
}

void G_Draw_Image_Relative(game_t *game, const char *path, float w, float h,
                           float x, float y, float z) {
  // Is the image loaded?
  zpl_u64 key = zpl_fnv64(path, strlen(path));
  texture_t *texture = G_Textures_get(&game->texture_bank, key);

  if (!texture) {
    char complete_path[256];
    sprintf(&complete_path[0], "%s/%s", game->base, path);
    printf("[VERBOSE] G_Draw_Image_Relative(\"%s\");\n", &complete_path[0]);
    texture_t t = G_LoadSingleTexture(complete_path);

    G_Textures_set(&game->texture_bank, key, t);
    texture = G_Textures_get(&game->texture_bank, key);
    if (!t.data) {
      printf("[ERROR] Couldn't load texture referenced in "
             "`G_Draw_Image_Relative`, namely \"%s\".\n",
             path);

      return;
    } else {
      VK_UploadSingleTexture(game->rend, texture);
    }
  }

  // Now it's loaded for sure, update the state to add a draw call referencing
  // this image
  game->state.draws[game->state.draw_count].x = x;
  game->state.draws[game->state.draw_count].y = y;
  game->state.draws[game->state.draw_count].z = z;
  game->state.draws[game->state.draw_count].w = w;
  game->state.draws[game->state.draw_count].h = h;
  game->state.draws[game->state.draw_count].handle = texture->handle;

  game->state.draw_count++;
}

void G_Draw_Image_Relative_QC(qcvm_t *qcvm) {
  const char *path = qcvm_get_parm_string(qcvm, 0);
  float w = qcvm_get_parm_float(qcvm, 1);
  float h = qcvm_get_parm_float(qcvm, 2);
  float x = qcvm_get_parm_float(qcvm, 3);
  float y = qcvm_get_parm_float(qcvm, 4);
  float z = qcvm_get_parm_float(qcvm, 5);

  game_t *game = ((worker_t *)qcvm_get_user_data(qcvm))->game;

  G_Draw_Image_Relative(game, path, w, h, x, y, z);
}

void G_Run_Scene(worker_t *worker, const char *scene_name) {
  printf("G_Run_Scene(\"%s\");\n", scene_name);
  game_t *game = worker->game;

  // Fetch the scene with this name
  scene_t *scene;
  for (unsigned i = 0; i < game->scene_count; i++) {
    if (strcmp(game->scenes[i].name, scene_name) == 0) {
      scene = &game->scenes[i];
      break;
    }
  }

  if (scene == NULL) {
    printf("[ERROR] Current game doesn't have a scene with that name `%s`.\n",
           scene_name);
    return;
  }

  // First, call all the end listeners in order for the previous scene
  // The invokation of listeners happens on the same thread that called
  // G_Run_Scene. I don't know if it's a good idea...
  if (game->current_scene) {
    for (unsigned j = 0; j < game->current_scene->end_listener_count; j++) {
      qcvm_run(worker->qcvm, game->current_scene->end_listeners[j].qcvm_func);
    }
  }

  // The specified scene become the current scene
  game->current_scene = scene;

  // Then call all start listeners in order
  for (unsigned j = 0; j < game->current_scene->start_listener_count; j++) {
    qcvm_run(worker->qcvm, game->current_scene->start_listeners[j].qcvm_func);
  }
}

void G_Run_Scene_QC(qcvm_t *qcvm) {
  worker_t *worker = (worker_t *)qcvm_get_user_data(qcvm);

  const char *scene_name = qcvm_get_parm_string(qcvm, 0);

  G_Run_Scene(worker, scene_name);
}

void G_Load_Game_QC(qcvm_t *qcvm) {
  worker_t *worker = (worker_t *)qcvm_get_user_data(qcvm);
  game_t *game = worker->game;

  const char *loading_scene_name = qcvm_get_parm_string(qcvm, 0);
  const char *next_scene = qcvm_get_parm_string(qcvm, 0);

  if (worker->id != 0) {
    printf(
        "`G_Load_Game_QC` can only be called from thread 0 (main thread).\n");
    return;
  }

  G_Run_Scene(worker, loading_scene_name);
}

const char *G_Get_Last_Asset_Loaded(game_t *game) {
  return "not yet implemented!";
}

void G_Get_Last_Asset_Loaded_QC(qcvm_t *qcvm) {
  game_t *game = ((worker_t *)qcvm_get_user_data(qcvm))->game;

  qcvm_return_string(qcvm, G_Get_Last_Asset_Loaded(game));
}

void G_Add_Wall(game_t *game, int x, int y, const char *recipe) {}

void G_Add_Wall_QC(qcvm_t *qcvm) {}

void G_Add_Scene(game_t *game, const char *name) {
  if (game->scene_capacity == 0) {
    game->scenes = calloc(16, sizeof(scene_t));
    game->scene_count = 0;
    game->scene_capacity = 16;
  }
  if (game->scene_count == game->scene_capacity) {
    game->scenes =
        realloc(game->scenes, game->scene_capacity * 2 * sizeof(scene_t));
    game->scene_capacity *= 2;
  }

  game->scenes[game->scene_count].name =
      memcpy(malloc(strlen(name) + 1), name, strlen(name) + 1);
  game->scene_count++;
}

void G_Add_Scene_QC(qcvm_t *qcvm) {
  const char *scene_name = qcvm_get_parm_string(qcvm, 0);

  worker_t *worker = (worker_t *)qcvm_get_user_data(qcvm);
  game_t *game = worker->game;

  if (worker->id != 0) {
    printf(
        "`G_Add_Scene_QC` can only be called from thread 0 (main thread).\n");
    return;
  }

  G_Add_Scene(game, scene_name);
}

scene_t *G_Get_Scene_By_Name(game_t *game, const char *name) {
  for (unsigned i = 0; i < game->scene_count; i++) {
    scene_t *the_scene = &game->scenes[i];
    if (strcmp(name, the_scene->name) == 0) {
      return the_scene;
    }
  }

  return NULL;
}

void G_Add_Listener(game_t *game, listener_type_t type, const char *attachment,
                    int func) {
  switch (type) {
  case G_SCENE_START: {
    scene_t *the_scene = G_Get_Scene_By_Name(game, attachment);

    if (the_scene) {
      if (the_scene->start_listener_count == 16) {
        printf("[ERROR] Reached max number of `G_SCENE_START` for the scene "
               "`%s`.\n",
               attachment);

        return;
      }
      the_scene->start_listeners[the_scene->start_listener_count].qcvm_func =
          func;
      the_scene->start_listener_count++;
    } else {
      printf(
          "[ERROR] QuakeC code specified a non-existent scene `%s` when trying "
          "to attach a `G_SCENE_START` listener.\n",
          attachment);
    }

    break;
  }
  case G_SCENE_END: {
    scene_t *the_scene = G_Get_Scene_By_Name(game, attachment);

    if (the_scene) {
      if (the_scene->end_listener_count == 16) {
        printf("[ERROR] Reached max number of `G_SCENE_END` for the scene "
               "`%s`.\n",
               attachment);

        return;
      }
      the_scene->end_listeners[the_scene->end_listener_count].qcvm_func = func;
      the_scene->end_listener_count++;
    } else {
      printf(
          "[ERROR] QuakeC code specified a non-existent scene `%s` when trying "
          "to attach a `G_SCENE_END` listener.\n",
          attachment);
    }

    break;
  }
  case G_SCENE_UPDATE: {
    scene_t *the_scene = G_Get_Scene_By_Name(game, attachment);

    if (the_scene) {
      if (the_scene->update_listener_count == 16) {
        printf("[ERROR] Reached max number of `G_SCENE_UPDATE` for the scene "
               "`%s`.\n",
               attachment);

        return;
      }
      the_scene->update_listeners[the_scene->update_listener_count].qcvm_func =
          func;
      the_scene->update_listener_count++;
    } else {
      printf(
          "[ERROR] QuakeC code specified a non-existent scene `%s` when trying "
          "to attach a `G_SCENE_UPDATE` listener.\n",
          attachment);
    }

    break;
  }
  default:
    break;
  }
}

void G_Add_Listener_QC(qcvm_t *qcvm) {
  int listener_type = qcvm_get_parm_int(qcvm, 0);
  const char *attachment = qcvm_get_parm_string(qcvm, 1);
  const char *func = qcvm_get_parm_string(qcvm, 2);

  if (listener_type >= G_LISTENER_TYPE_COUNT) {
    printf("[ERROR] QuakeC code specified an invalid listener type.\n");
    return;
  }

  int func_id = qcvm_find_function(qcvm, func);
  if (func_id < 1) {
    printf("[ERROR] QuakeC code specified an invalid function for the "
           "listener callback.\n");
    return;
  }

  G_Add_Listener(((worker_t *)qcvm_get_user_data(qcvm))->game, listener_type,
                 attachment, func_id);
}

void G_Install_QCVM(worker_t *worker) {
  qcvm_export_t export_G_Add_Recipes = {
      .func = G_Add_Recipes_QC,
      .name = "G_Add_Recipes",
      .argc = 2,
      .args[0] = {.name = "path", .type = QCVM_STRING},
      .args[1] = {.name = "required", .type = QCVM_INT},
  };

  qcvm_export_t export_G_Load_Game = {
      .func = G_Load_Game_QC,
      .name = "G_Load_Game",
      .argc = 1,
      .args[0] = {.name = "scene_name", .type = QCVM_STRING},
  };

  qcvm_export_t export_G_Get_Last_Asset_Loaded = {
      .func = G_Get_Last_Asset_Loaded_QC,
      .name = "G_Get_Last_Asset_Loaded",
      .argc = 0,
  };

  qcvm_export_t export_G_Add_Scene = {
      .func = G_Add_Scene_QC,
      .name = "G_Add_Scene",
      .argc = 1,
      .args[0] = {.name = "s", .type = QCVM_STRING},
  };

  qcvm_export_t export_G_Run_Scene = {
      .func = G_Run_Scene_QC,
      .name = "G_Run_Scene",
      .argc = 1,
      .args[0] = {.name = "s", .type = QCVM_STRING},
  };

  qcvm_export_t export_G_Create_Map = {
      .func = G_Create_Map_QC,
      .name = "G_Create_Map",
      .argc = 2,
      .args[0] = {.name = "w", .type = QCVM_FLOAT},
      .args[1] = {.name = "h", .type = QCVM_FLOAT},
      .type = QCVM_INT,
  };

  qcvm_export_t export_G_Add_Wall = {
      .func = G_Add_Wall_QC,
      .name = "G_Add_Wall",
      .argc = 5,
      .args[0] = {.name = "recipe", .type = QCVM_STRING},
      .args[1] = {.name = "x", .type = QCVM_FLOAT},
      .args[2] = {.name = "y", .type = QCVM_FLOAT},
      .args[3] = {.name = "health", .type = QCVM_FLOAT},
      .args[4] = {.name = "to_build", .type = QCVM_INT},
  };

  qcvm_export_t export_G_Add_Listener = {
      .func = G_Add_Listener_QC,
      .name = "G_Add_Listener",
      .argc = 3,
      .args[0] = {.name = "type", .type = QCVM_INT},
      .args[1] = {.name = "attachment", .type = QCVM_STRING},
      .args[2] = {.name = "func", .type = QCVM_STRING},
  };

  qcvm_export_t export_G_Draw_Image_Relative = {
      .func = G_Draw_Image_Relative_QC,
      .name = "G_Draw_Image_Relative",
      .argc = 6,
      .args[0] = {.name = "path", .type = QCVM_STRING},
      .args[1] = {.name = "w", .type = QCVM_FLOAT},
      .args[2] = {.name = "h", .type = QCVM_FLOAT},
      .args[3] = {.name = "x", .type = QCVM_FLOAT},
      .args[4] = {.name = "y", .type = QCVM_FLOAT},
      .args[5] = {.name = "z", .type = QCVM_FLOAT},
  };

  qcvm_add_export(worker->qcvm, &export_G_Add_Recipes);
  qcvm_add_export(worker->qcvm, &export_G_Load_Game);
  qcvm_add_export(worker->qcvm, &export_G_Get_Last_Asset_Loaded);
  qcvm_add_export(worker->qcvm, &export_G_Add_Scene);
  qcvm_add_export(worker->qcvm, &export_G_Run_Scene);
  qcvm_add_export(worker->qcvm, &export_G_Create_Map);
  qcvm_add_export(worker->qcvm, &export_G_Add_Wall);
  qcvm_add_export(worker->qcvm, &export_G_Add_Listener);
  qcvm_add_export(worker->qcvm, &export_G_Draw_Image_Relative);
}

bool G_Load(client_t *client, game_t *game) {
  // TODO: should find a way to correctly guess the number of workers
  game->worker_count = 8;

  game->cpu_agents = calloc(3000, sizeof(cpu_agent_t));
  time_t seed = time(NULL);
  srand(seed);
  // Try to fetch the progs.dat of the specified game
  char progs_dat[256];
  sprintf(&progs_dat[0], "%s/progs.dat", game->base);

  FILE *f = fopen(&progs_dat[0], "rb");
  if (!f) {
    printf("`%s` file doesn't seem to exist.\n", progs_dat);
    return NULL;
  }

  fseek(f, 0, SEEK_END);
  size_t size = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *bytes = malloc(size);
  fread(bytes, size, 1, f);
  fclose(f);

  for (unsigned i = 0; i < game->worker_count; i++) {
    game->workers[i].qcvm = qcvm_from_memory(bytes, size);
    if (!game->workers[i].qcvm) {
      printf("Couldn't load `%s`. Aborting, the game isn't playable.\n",
             progs_dat);
      return false;
    }

    qclib_install(game->workers[i].qcvm);
    G_Install_QCVM(&game->workers[i]);

    game->workers[i].id = i;
    qcvm_set_user_data(game->workers[i].qcvm, &game->workers[i]);
  }

  // Run the QuakeC main function on the first worker
  int main_func = qcvm_find_function(game->workers[0].qcvm, "main");
  if (main_func < 1) {
    printf("[ERROR] No main function in `progs.dat`.\n");
    return false;
  }
  qcvm_run(game->workers[0].qcvm, main_func);

  // Get the mapped data from the renderer
  game->gpu_agents = VK_GetAgents(game->rend);
  game->transforms = VK_GetTransforms(game->rend);
  game->gpu_tiles = VK_GetMap(game->rend);
  game->entities = VK_GetEntities(game->rend);

  return true;
}

void G_AddFurniture(client_t *client, game_t *game, struct Transform *transform,
                    struct Sprite *sprite, struct Immovable *immovable) {
  game->entity_count += 1;
  vk_rend_t *rend = game->rend;
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
  vk_rend_t *rend = game->rend;
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
