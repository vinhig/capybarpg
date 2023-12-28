#include "game/g_game.h"
#include "cglm/mat4.h"
#include "freetype/freetype.h"
#include "vk/vk_vulkan.h"
#include <client/cl_input.h>
#include <game/g_private.h>
#include <stbi_image.h>

#include <jps.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

void G_WorkerSetupTileText(void *data);
void G_WorkerLoadTexture(void *data);
void G_WorkerLoadFont(void *data);

extern const unsigned char no_image[];
extern const unsigned no_image_size;

game_t *G_CreateGame(client_t *client, char *base) {
  game_t *game = calloc(1, sizeof(game_t));

  game->cpu_agents = calloc(3000, sizeof(cpu_agent_t));

  game->map_textures = calloc(32, sizeof(texture_t));
  game->map_texture_capacity = 32;
  game->map_texture_count = 0;

  game->font_textures = calloc(1024, sizeof(texture_t));
  game->font_texture_capacity = 1024;
  game->font_texture_count = 0;

  game->state.texts = calloc(4096, sizeof(game_text_draw_t));
  game->state.text_capacity = 4096;
  atomic_store(&game->state.text_count, 0);

  game->state.fps.zoom = 0.178f;
  game->state.fps.pos[0] = 4.806f;
  game->state.fps.pos[1] = 4.633f;

  game->base = base;

  game->client = client;
  game->rend = CL_GetRend(client);

  zpl_affinity af;
  zpl_affinity_init(&af);
  // The workers[0] is actually the main thread ok?
  game->worker_count = (af.thread_count / 2) + 1;

  zpl_jobs_init_with_limit(&game->job_sys, zpl_heap_allocator(),
                           game->worker_count, 10000);

  for (unsigned w = 0; w < game->worker_count; w++) {
    game->workers[w] = (worker_t){
        .game = game,
        .id = w,
    };
  }

  zpl_affinity_destroy(&af);

  zpl_mutex_init(&game->map_texture_mutex);
  zpl_mutex_init(&game->font_mutex);

  G_Materials_init(&game->material_bank, zpl_heap_allocator());
  G_ImmediateTextures_init(&game->immediate_texture_bank, zpl_heap_allocator());
  G_Walls_init(&game->wall_bank, zpl_heap_allocator());
  G_Terrains_init(&game->terrain_bank, zpl_heap_allocator());

  if (FT_Init_FreeType(&game->console_ft)) {
    printf("[ERROR] Couldn't init freetype for the game.\n");
    return false;
  }
  if (FT_Init_FreeType(&game->game_ft)) {
    printf("[ERROR] Couldn't init freetype for the console.\n");
    return false;
  }
  game->white_space = calloc(128 * 128, sizeof(char));

  CL_Characters_init(&game->console_character_bank, zpl_heap_allocator());
  CL_Characters_init(&game->game_character_bank, zpl_heap_allocator());

  // Add "model" system at the end, just compute the model matrix for each
  // transform
  VK_AddSystem_Transform(game->rend, "model_matrix system",
                         "model_matrix_frustum.comp.spv");

  return game;
}

void G_DestroyGame(game_t *game) {
  for (unsigned i = 0; i < zpl_array_count(game->material_bank.entries); i++) {
    material_bank_tEntry *entry = &game->material_bank.entries[i];
    free(entry->value.full_stack_path);
    free(entry->value.half_stack_path);
    free(entry->value.low_stack_path);
    free(entry->value.name);
  }

  for (unsigned i = 0;
       i < zpl_array_count(game->immediate_texture_bank.entries); i++) {
    texture_bank_tEntry *entry = &game->immediate_texture_bank.entries[i];
    free(entry->value.label);
    free(entry->value.data);
  }

  for (unsigned i = 0; i < zpl_array_count(game->wall_bank.entries); i++) {
    wall_bank_tEntry *entry = &game->wall_bank.entries[i];
    free(entry->value.only_left_path);
    free(entry->value.only_right_path);
    free(entry->value.only_top_path);
    free(entry->value.only_bottom_path);
    free(entry->value.all_path);
    free(entry->value.left_right_bottom_path);
    free(entry->value.left_right_path);
    free(entry->value.left_right_top_path);
    free(entry->value.top_bottom_path);
    free(entry->value.right_top_path);
    free(entry->value.left_top_path);
    free(entry->value.right_bottom_path);
    free(entry->value.left_bottom_path);
    free(entry->value.left_top_bottom_path);
    free(entry->value.right_top_bottom_path);
    free(entry->value.nothing_path);
    free(entry->value.name);
  }

  for (unsigned i = 0; i < zpl_array_count(game->terrain_bank.entries); i++) {
    terrain_bank_tEntry *entry = &game->terrain_bank.entries[i];
    free(entry->value.variant1_path);
    free(entry->value.variant2_path);
    free(entry->value.variant3_path);
    free(entry->value.name);
  }

  for (unsigned i = 0; i < game->scene_count; i++) {
    free(game->scenes[i].name);
  }

  for (unsigned i = 0; i < game->map_texture_count; i++) {
    texture_t *tex = &game->map_textures[i];
    free(tex->data);
    free(tex->label);
  }

  free(game->map_textures);

  for (unsigned i = 0; i < game->font_texture_count; i++) {
    texture_t *tex = &game->font_textures[i];
    free(tex->data);
    // free(tex->label);
  }

  free(game->font_textures);

  G_Materials_destroy(&game->material_bank);
  G_ImmediateTextures_destroy(&game->immediate_texture_bank);
  G_Walls_destroy(&game->wall_bank);
  G_Terrains_destroy(&game->terrain_bank);

  for (unsigned i = 0; i < game->worker_count; i++) {
    // jps_destroy(game->workers[i].)
    qcvm_free(game->workers[i].qcvm);
  }

  for (unsigned i = 0; i < game->map_count; i++) {
    free(game->maps[i].cpu_tiles);
    jps_destroy(game->maps[i].jps_map);
  }

  zpl_jobs_free(&game->job_sys);

  FT_Done_Face(game->console_face);
  FT_Done_Face(game->game_face);
  FT_Done_FreeType(game->console_ft);
  FT_Done_FreeType(game->game_ft);

  free(game->cpu_agents);
  free(game->scenes);
  free(game);
}

// void G_UpdateAgents(game_t *game, unsigned i) {
//   if (game->cpu_agents[i].state == AGENT_PATH_FINDING) {
//     // G_PathFinding(&game->workers[0], i);
//     jps_set_start(game->map, game->transforms[i].position[0],
//                   game->transforms[i].position[1]);
//     jps_set_end(game->map, game->cpu_agents[i].target[0],
//                 game->cpu_agents[i].target[1]);

//     IntList *list = il_create(2);
//     jps_path_finding(game->map, 2, list);

//     unsigned size = il_size(list);

//     game->cpu_agents[i].computed_path.count = size;
//     game->cpu_agents[i].computed_path.current = 0;
//     game->cpu_agents[i].computed_path.points = calloc(size, sizeof(vec2));
//     for (unsigned p = 0; p < size; p++) {
//       int x = il_get(list, (size - p - 1), 0);
//       int y = il_get(list, (size - p - 1), 1);
//       game->cpu_agents[i].computed_path.points[p][0] = x;
//       game->cpu_agents[i].computed_path.points[p][1] = y;
//     }

//     game->cpu_agents[i].state = AGENT_MOVING;
//   } else if (game->cpu_agents[i].state == AGENT_MOVING) {
//     unsigned c = game->cpu_agents[i].computed_path.current;
//     if (c < game->cpu_agents[i].computed_path.count) {
//       // Compute the direction to take
//       vec2 next_pos;
//       glm_vec2(game->cpu_agents[i].computed_path.points[c], next_pos);
//       vec2 d;
//       glm_vec2_sub(next_pos, game->transforms[i].position, d);
//       vec2 s;
//       glm_vec2_sign(d, s);
//       d[0] = glm_min(fabs(d[0]), game->cpu_agents[i].speed) * s[0];
//       d[1] = glm_min(fabs(d[1]), game->cpu_agents[i].speed) * s[1];

//       // Reflect the direction on the related GPU agent
//       // Visual and Animation is supposed to change
//       vec2 supposed_d = {
//           1.0f * s[0],
//           1.0f * s[1],
//       };
//       if (supposed_d[0] != 0.0f || supposed_d[1] != 0.0f) {
//         game->gpu_agents[i].direction[0] = supposed_d[0];
//         game->gpu_agents[i].direction[1] = supposed_d[1];
//       }

//       // Apply the movement
//       glm_vec2_add(game->transforms[i].position, d,
//                    game->transforms[i].position);
//       if (game->transforms[i].position[0] == next_pos[0] &&
//           game->transforms[i].position[1] == next_pos[1]) {
//         game->cpu_agents[i].computed_path.current++;
//       }
//     } else {
//       game->cpu_agents[i].state = AGENT_NOTHING;
//       game->gpu_agents[i].direction[0] = 0.0f;
//       game->gpu_agents[i].direction[1] = 0.0f;
//     }
//   }
// }

void G_ResetGameState(game_t *game) {
  game_text_draw_t *draws = game->state.texts;
  int count = atomic_load(&game->state.text_count);
  memset(draws, 0, sizeof(game_text_draw_t) * count);

  game->state.draw_count = 0;
  memset(&game->state.draws, 0, sizeof(game->state.draws));
  game->state.texts = draws;
  atomic_store(&game->state.text_count, 0);
}

game_state_t *G_TickGame(client_t *client, game_t *game) {
  G_ResetGameState(game);

  unsigned w, h;
  CL_GetViewDim(client, &w, &h);

  float ratio = (float)w / (float)h;

  glm_mat4_identity(game->state.fps.view);
  float zoom = game->state.fps.zoom;
  float offset_x = game->state.fps.pos[0];
  float offset_y = game->state.fps.pos[1];
  glm_ortho(-1.0 * ratio / zoom + offset_x, 1.0 * ratio / zoom + offset_x,
            -1.0f / zoom + offset_y, 1.0f / zoom + offset_y, 0.01, 50.0,
            (vec4 *)&game->state.fps.view_proj);

  printf("zoom = %.03f, offset_x = %.03f, offset_y = %.03f\n", zoom, offset_x, offset_y);

  game->delta_time = zpl_time_rel() - game->last_time;

  game->last_time = zpl_time_rel();

  if (!game->current_scene) {
    printf("[WARNING] No current scene hehe...\n");
  } else {
    for (unsigned i = 0; i < game->current_scene->update_listener_count; i++) {
      qcvm_set_parm_float(game->workers[0].qcvm, 0, game->delta_time);
      qcvm_run(game->workers[0].qcvm,
               game->current_scene->update_listeners[i].qcvm_func);
    }

    for (unsigned i = 0; i < game->current_scene->camera_update_listener_count;
         i++) {
      qcvm_set_parm_float(game->workers[0].qcvm, 0, game->delta_time);
      qcvm_run(game->workers[0].qcvm,
               game->current_scene->camera_update_listeners[i].qcvm_func);
    }

    for (unsigned i = 0; i < game->entity_count; i++) {
      unsigned signature = game->entities[i];

      if (signature & agent_signature) {
        // G_UpdateAgents(game, i);
      }
    }

    if (game->current_scene->current_map != -1) {
      map_t *the_map = &game->maps[game->current_scene->current_map];

      item_text_job_t *jobs = calloc(the_map->h, sizeof(item_text_job_t));

      for (unsigned row = 0; row < the_map->h; row++) {
        jobs[row] = (item_text_job_t){
            .row = row,
            .game = game,
        };

        zpl_jobs_enqueue(&game->job_sys, G_WorkerSetupTileText, &jobs[row]);
      }

      while (!zpl_jobs_done(&game->job_sys)) {
        zpl_jobs_process(&game->job_sys);
      };

      free(jobs);
    }

    zpl_f64 delta = (zpl_time_rel() - game->last_time) /
                    1000.0f; // Delta time in milliseconds
    if (delta > 10.0f) {
      printf("[WARNING] Anormaly long update time... %fms\n", delta);
    }
  }

  VK_TickSystems(game->rend);

  return &game->state;
}

texture_t G_LoadSingleTextureFromMemory(const unsigned char *pdata,
                                        unsigned len, char *label) {
  stbi_set_flip_vertically_on_load(true);
  int w, h, c;
  unsigned char *data = stbi_load_from_memory(pdata, len, &w, &h, &c, 4);

  if (!data) {
    printf("omgggg.\n");
  }

  return (texture_t){
      .c = c,
      .data = data,
      .height = h,
      .width = w,
      .label = label,
  };
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
      .label = memcpy(malloc(strlen(path) + 1), path, strlen(path) + 1),
  };
}

int G_Create_Map(game_t *game, unsigned w, unsigned h) {
  if (game->map_count == 16) {
    printf(
        "[ERROR] Max number of map reached (16), who needs that much map???\n");
    return -1;
  }

  VK_CreateMap(game->rend, w, h, game->map_count);

  // Init the same map accross all workers
  // for (unsigned i = 0; i < game->worker_count; i++) {
  // TODO: the jps map should be per worker
  map_t *the_map = &game->maps[game->map_count];
  zpl_mutex_lock(&the_map->mutex);
  the_map->jps_map = jps_create(w, h);
  the_map->w = w;
  the_map->h = h;
  the_map->gpu_tiles = VK_GetMap(game->rend, game->map_count);
  the_map->cpu_tiles = calloc(sizeof(cpu_tile_t), w * h);

  // Set every single tile to be the default PINK texture (texture == 0)
  // And not sure if the mapped data from the renderer is actually zeroed
  memset(the_map->gpu_tiles, 0, w * h * sizeof(struct Tile));
  zpl_mutex_unlock(&the_map->mutex);
  // }

  if (game->current_scene->current_map == -1) {
    game->current_scene->current_map = game->map_count;
  }

  int map = game->map_count;
  game->map_count++;

  return map;
}

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

void G_Set_Current_Map_QC(qcvm_t *qcvm) {
  worker_t *worker = qcvm_get_user_data(qcvm);
  game_t *game = worker->game;

  const char *scene_name = qcvm_get_parm_string(qcvm, 0);
  int map = qcvm_get_parm_int(qcvm, 1);

  if (map < 0 || map >= (int)game->map_count) {
    printf("[ERROR] Assertion G_Set_Current_Map_QC(map >= 0 || map < "
           "game->map_count) "
           "[map = %d, map_count = %d] should be "
           "verified.\n",
           map, game->map_count);
    return;
  }

  scene_t *scene = NULL;

  for (unsigned i = 0; i < game->scene_count; i++) {
    scene_t *the_scene = &game->scenes[i];
    if (strcmp(scene_name, the_scene->name) == 0) {
      scene = the_scene;
    }
  }

  if (!scene) {
    printf("[ERROR] Assertion G_Set_Current_Map_QC(map exists) [map = \"%s\"] "
           "should be verified.\n",
           scene_name);
    return;
  }

  zpl_mutex_lock(&game->global_map_mutex);
  zpl_mutex_lock(&scene->scene_mutex);
  scene->current_map = map;
  VK_SetCurrentMap(game->rend, map);
  zpl_mutex_unlock(&scene->scene_mutex);
  zpl_mutex_unlock(&game->global_map_mutex);
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
  zpl_json_object *terrains = zpl_adt_find(&recipes, "terrains", false);
  zpl_json_object *materials = zpl_adt_find(&recipes, "materials", false);
  zpl_json_object *walls = zpl_adt_find(&recipes, "walls", false);
  zpl_json_object *furnitures = zpl_adt_find(&recipes, "furnitures", false);

  zpl_unused(furnitures);

  // The info extraction is a bit weird as mods can overwrite what was
  // previously specified. So if there isn't a element of a specific type in the
  // asset bank with that name, we create it. Otherwise, we simply update its
  // properties (for example a mod "Stronger Walls" doesn't have to re-specify
  // every single sprites, just the health). So the identifier is the only
  // mandatory field. Yes I know something like Rust or Go would have been
  // smarter than all this manual marshalling bullshit.

  if (terrains && terrains->type == ZPL_ADT_TYPE_ARRAY) {
    unsigned size = zpl_array_count(terrains->nodes);

    for (unsigned i = 0; i < size; i++) {
      zpl_adt_node *element = &terrains->nodes[i];
      zpl_adt_node *id_node = zpl_adt_find(element, "id", false);

      if (!id_node || id_node->type != ZPL_ADT_TYPE_STRING) {
        printf("[WARNING] The element %d in the 'terrains' list doesn't have "
               "an ID (of type string).\n",
               i);
        continue;
      } else {
        zpl_u64 key = zpl_fnv64(id_node->string, strlen(id_node->string));
        // Does the bank contains this element? If not create an empty one.
        terrain_t *terrain = G_Terrains_get(&game->terrain_bank, key);

        if (!terrain) {
          G_Terrains_set(&game->terrain_bank, key, (terrain_t){.revision = 1});
          terrain = G_Terrains_get(&game->terrain_bank, key);
        } else {
          terrain->revision += 1;
        }

        zpl_adt_node *name_node = zpl_adt_find(element, "name", false);
        zpl_adt_node *sprites_node = zpl_adt_find(element, "sprites", false);

        zpl_adt_node *variant1_node = NULL;
        zpl_adt_node *variant2_node = NULL;
        zpl_adt_node *variant3_node = NULL;

        // Some field may be NULL if not present, we don't care
        if (name_node && name_node->type == ZPL_ADT_TYPE_STRING) {
          if (terrain->name) {
            free(terrain->name);
          }
          terrain->name =
              memcpy(malloc(strlen(name_node->string) + 1), name_node->string,
                     strlen(name_node->string) + 1);
        }

        if (sprites_node && sprites_node->type == ZPL_ADT_TYPE_OBJECT) {
          variant1_node = zpl_adt_find(sprites_node, "variant0", false);
          variant2_node = zpl_adt_find(sprites_node, "variant1", false);
          variant3_node = zpl_adt_find(sprites_node, "variant2", false);

          if (variant1_node && variant1_node->type == ZPL_ADT_TYPE_STRING) {
            if (terrain->variant1_path) {
              free(terrain->variant1_path);
            }
            terrain->variant1_path = memcpy(
                malloc(strlen(variant1_node->string) + 1),
                variant1_node->string, strlen(variant1_node->string) + 1);
          }
          if (variant2_node && variant2_node->type == ZPL_ADT_TYPE_STRING) {
            if (terrain->variant2_path) {
              free(terrain->variant2_path);
            }
            terrain->variant2_path = memcpy(
                malloc(strlen(variant2_node->string) + 1),
                variant2_node->string, strlen(variant2_node->string) + 1);
          }
          if (variant3_node && variant3_node->type == ZPL_ADT_TYPE_STRING) {
            if (terrain->variant3_path) {
              free(terrain->variant3_path);
            }
            terrain->variant3_path = memcpy(
                malloc(strlen(variant3_node->string) + 1),
                variant3_node->string, strlen(variant3_node->string) + 1);
          }
        }
      }
    }
  }

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
          G_Materials_set(&game->material_bank, key,
                          (material_t){.revision = 1, .key = key});
          material = G_Materials_get(&game->material_bank, key);
        } else {
          material->revision += 1;
        }

        // Extract all relevant info from the JSON field
        zpl_adt_node *name_node = zpl_adt_find(element, "name", false);
        zpl_adt_node *stack_size_node =
            zpl_adt_find(element, "stack_size", false);
        zpl_adt_node *sprites_node = zpl_adt_find(element, "sprites", false);

        zpl_adt_node *low_stack_node = NULL;
        zpl_adt_node *half_stack_node = NULL;
        zpl_adt_node *full_stack_node = NULL;

        // Some field may be NULL if not present, we don't care
        if (name_node && name_node->type == ZPL_ADT_TYPE_STRING) {
          if (material->name) {
            free(material->name);
          }
          material->name =
              memcpy(malloc(strlen(name_node->string) + 1), name_node->string,
                     strlen(name_node->string) + 1);
        }

        if (stack_size_node && stack_size_node->type == ZPL_ADT_TYPE_INTEGER) {
          material->stack_size = stack_size_node->integer;
        }

        if (sprites_node && sprites_node->type == ZPL_ADT_TYPE_OBJECT) {
          low_stack_node = zpl_adt_find(sprites_node, "low_stack", false);
          half_stack_node = zpl_adt_find(sprites_node, "half_stack", false);
          full_stack_node = zpl_adt_find(sprites_node, "full_stack", false);

          if (low_stack_node && low_stack_node->type == ZPL_ADT_TYPE_STRING) {
            if (material->low_stack_path) {
              free(material->low_stack_path);
            }
            material->low_stack_path = memcpy(
                malloc(strlen(low_stack_node->string) + 1),
                low_stack_node->string, strlen(low_stack_node->string) + 1);
          }
          if (half_stack_node && half_stack_node->type == ZPL_ADT_TYPE_STRING) {
            if (material->half_stack_path) {
              free(material->half_stack_path);
            }
            material->half_stack_path = memcpy(
                malloc(strlen(half_stack_node->string) + 1),
                half_stack_node->string, strlen(half_stack_node->string) + 1);
          }
          if (full_stack_node && full_stack_node->type == ZPL_ADT_TYPE_STRING) {
            if (material->full_stack_path) {
              free(material->full_stack_path);
            }
            material->full_stack_path = memcpy(
                malloc(strlen(full_stack_node->string) + 1),
                full_stack_node->string, strlen(full_stack_node->string) + 1);
          }
        }
      }
    }
  } else {
    printf("[DEBUG] No 'materials' specified in `%s`.\n", path);
  }

  if (walls && walls->type == ZPL_ADT_TYPE_ARRAY) {
    unsigned size = zpl_array_count(walls->nodes);
    for (unsigned i = 0; i < size; i++) {
      zpl_adt_node *element = &walls->nodes[i];
      zpl_adt_node *id_node = zpl_adt_find(element, "id", false);

      if (!id_node || id_node->type != ZPL_ADT_TYPE_STRING) {
        printf("[WARNING] The element %d in the 'walls' list doesn't have "
               "an ID (of type string).\n",
               i);
        continue;
      } else {
        zpl_u64 key = zpl_fnv64(id_node->string, strlen(id_node->string));
        // Does the bank contains this element? If not create an empty one.
        wall_t *wall = G_Walls_get(&game->wall_bank, key);

        if (!wall) {
          G_Walls_set(&game->wall_bank, key, (wall_t){.revision = 1});
          wall = G_Walls_get(&game->wall_bank, key);
        } else {
          wall->revision += 1;
        }

        // Extract all relevant info from the JSON field
        zpl_adt_node *name_node = zpl_adt_find(element, "name", false);
        zpl_adt_node *health_node = zpl_adt_find(element, "health", false);
        zpl_adt_node *sprites_node = zpl_adt_find(element, "sprites", false);

        // Some field may be NULL if not present, we don't care
        if (name_node && name_node->type == ZPL_ADT_TYPE_STRING) {
          if (wall->name) {
            free(wall->name);
          }
          wall->name = memcpy(malloc(strlen(name_node->string) + 1),
                              name_node->string, strlen(name_node->string) + 1);
        }

        if (health_node && health_node->type == ZPL_ADT_TYPE_INTEGER) {
          wall->health = health_node->integer;
        }

        zpl_adt_node *only_left_node = NULL;
        zpl_adt_node *only_right_node = NULL;
        zpl_adt_node *only_top_node = NULL;
        zpl_adt_node *only_bottom_node = NULL;
        zpl_adt_node *all_node = NULL;
        zpl_adt_node *left_right_bottom_node = NULL;
        zpl_adt_node *left_right_node = NULL;
        zpl_adt_node *left_right_top_node = NULL;
        zpl_adt_node *top_bottom_node = NULL;
        zpl_adt_node *right_top_node = NULL;
        zpl_adt_node *left_top_node = NULL;
        zpl_adt_node *right_bottom_node = NULL;
        zpl_adt_node *left_bottom_node = NULL;
        zpl_adt_node *left_top_bottom_node = NULL;
        zpl_adt_node *right_top_bottom_node = NULL;
        zpl_adt_node *nothing_node = NULL;

        if (sprites_node && sprites_node->type == ZPL_ADT_TYPE_OBJECT) {
          only_left_node = zpl_adt_find(sprites_node, "only_left", false);
          only_right_node = zpl_adt_find(sprites_node, "only_right", false);
          only_top_node = zpl_adt_find(sprites_node, "only_top", false);
          only_bottom_node = zpl_adt_find(sprites_node, "only_bottom", false);
          all_node = zpl_adt_find(sprites_node, "all", false);
          left_right_bottom_node =
              zpl_adt_find(sprites_node, "left_right_bottom", false);
          left_right_node = zpl_adt_find(sprites_node, "left_right", false);
          left_right_top_node =
              zpl_adt_find(sprites_node, "left_right_top", false);
          top_bottom_node = zpl_adt_find(sprites_node, "top_bottom", false);
          right_top_node = zpl_adt_find(sprites_node, "right_top", false);
          left_top_node = zpl_adt_find(sprites_node, "left_top", false);
          right_bottom_node = zpl_adt_find(sprites_node, "right_bottom", false);
          left_bottom_node = zpl_adt_find(sprites_node, "left_bottom", false);
          left_top_bottom_node =
              zpl_adt_find(sprites_node, "left_top_bottom", false);
          right_top_bottom_node =
              zpl_adt_find(sprites_node, "right_top_bottom", false);
          nothing_node = zpl_adt_find(sprites_node, "nothing", false);

          if (only_left_node && only_left_node->type == ZPL_ADT_TYPE_STRING) {
            if (wall->only_left_path) {
              free(wall->only_left_path);
            }
            wall->only_left_path = memcpy(
                malloc(strlen(only_left_node->string) + 1),
                only_left_node->string, strlen(only_left_node->string) + 1);
          }
          if (only_right_node && only_right_node->type == ZPL_ADT_TYPE_STRING) {
            if (wall->only_right_path) {
              free(wall->only_right_path);
            }
            wall->only_right_path = memcpy(
                malloc(strlen(only_right_node->string) + 1),
                only_right_node->string, strlen(only_right_node->string) + 1);
          }
          if (only_top_node && only_top_node->type == ZPL_ADT_TYPE_STRING) {
            if (wall->only_top_path) {
              free(wall->only_top_path);
            }
            wall->only_top_path = memcpy(
                malloc(strlen(only_top_node->string) + 1),
                only_top_node->string, strlen(only_top_node->string) + 1);
          }
          if (only_bottom_node &&
              only_bottom_node->type == ZPL_ADT_TYPE_STRING) {
            if (wall->only_bottom_path) {
              free(wall->only_bottom_path);
            }
            wall->only_bottom_path = memcpy(
                malloc(strlen(only_bottom_node->string) + 1),
                only_bottom_node->string, strlen(only_bottom_node->string) + 1);
          }
          if (all_node && all_node->type == ZPL_ADT_TYPE_STRING) {
            if (wall->all_path) {
              free(wall->all_path);
            }
            wall->all_path =
                memcpy(malloc(strlen(all_node->string) + 1), all_node->string,
                       strlen(all_node->string) + 1);
          }
          if (left_right_bottom_node &&
              left_right_bottom_node->type == ZPL_ADT_TYPE_STRING) {
            if (wall->left_right_bottom_path) {
              free(wall->left_right_bottom_path);
            }
            wall->left_right_bottom_path =
                memcpy(malloc(strlen(left_right_bottom_node->string) + 1),
                       left_right_bottom_node->string,
                       strlen(left_right_bottom_node->string) + 1);
          }
          if (left_right_node && left_right_node->type == ZPL_ADT_TYPE_STRING) {
            if (wall->left_right_path) {
              free(wall->left_right_path);
            }
            wall->left_right_path = memcpy(
                malloc(strlen(left_right_node->string) + 1),
                left_right_node->string, strlen(left_right_node->string) + 1);
          }
          if (left_right_top_node &&
              left_right_top_node->type == ZPL_ADT_TYPE_STRING) {
            if (wall->left_right_top_path) {
              free(wall->left_right_top_path);
            }
            wall->left_right_top_path =
                memcpy(malloc(strlen(left_right_top_node->string) + 1),
                       left_right_top_node->string,
                       strlen(left_right_top_node->string) + 1);
          }
          if (top_bottom_node && top_bottom_node->type == ZPL_ADT_TYPE_STRING) {
            if (wall->top_bottom_path) {
              free(wall->top_bottom_path);
            }
            wall->top_bottom_path = memcpy(
                malloc(strlen(top_bottom_node->string) + 1),
                top_bottom_node->string, strlen(top_bottom_node->string) + 1);
          }
          if (right_top_node && right_top_node->type == ZPL_ADT_TYPE_STRING) {
            if (wall->right_top_path) {
              free(wall->right_top_path);
            }
            wall->right_top_path = memcpy(
                malloc(strlen(right_top_node->string) + 1),
                right_top_node->string, strlen(right_top_node->string) + 1);
          }
          if (left_top_node && left_top_node->type == ZPL_ADT_TYPE_STRING) {
            if (wall->left_top_path) {
              free(wall->left_top_path);
            }
            wall->left_top_path = memcpy(
                malloc(strlen(left_top_node->string) + 1),
                left_top_node->string, strlen(left_top_node->string) + 1);
          }
          if (right_bottom_node &&
              right_bottom_node->type == ZPL_ADT_TYPE_STRING) {
            if (wall->right_bottom_path) {
              free(wall->right_bottom_path);
            }
            wall->right_bottom_path =
                memcpy(malloc(strlen(right_bottom_node->string) + 1),
                       right_bottom_node->string,
                       strlen(right_bottom_node->string) + 1);
          }
          if (left_bottom_node &&
              left_bottom_node->type == ZPL_ADT_TYPE_STRING) {
            if (wall->left_bottom_path) {
              free(wall->left_bottom_path);
            }
            wall->left_bottom_path = memcpy(
                malloc(strlen(left_bottom_node->string) + 1),
                left_bottom_node->string, strlen(left_bottom_node->string) + 1);
          }
          if (left_top_bottom_node &&
              left_top_bottom_node->type == ZPL_ADT_TYPE_STRING) {
            if (wall->left_top_bottom_path) {
              free(wall->left_top_bottom_path);
            }
            wall->left_top_bottom_path =
                memcpy(malloc(strlen(left_top_bottom_node->string) + 1),
                       left_top_bottom_node->string,
                       strlen(left_top_bottom_node->string) + 1);
          }
          if (right_top_bottom_node &&
              right_top_bottom_node->type == ZPL_ADT_TYPE_STRING) {
            if (wall->right_top_bottom_path) {
              free(wall->right_top_bottom_path);
            }
            wall->right_top_bottom_path =
                memcpy(malloc(strlen(right_top_bottom_node->string) + 1),
                       right_top_bottom_node->string,
                       strlen(right_top_bottom_node->string) + 1);
          }
          if (nothing_node && nothing_node->type == ZPL_ADT_TYPE_STRING) {
            if (wall->nothing_path) {
              free(wall->nothing_path);
            }
            wall->nothing_path =
                memcpy(malloc(strlen(nothing_node->string) + 1),
                       nothing_node->string, strlen(nothing_node->string) + 1);
          }
        }
      }
    }
  } else {
    printf("[DEBUG] No 'walls' specified in `%s`.\n", path);
  }

  free(content);
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

unsigned G_Add_Items(game_t *game, unsigned map, unsigned x, unsigned y,
                     material_t *the_item, unsigned amount) {
  map_t *the_map = &game->maps[map];

  unsigned idx = y * the_map->w + x;

  cpu_tile_t *the_cpu_tile = &the_map->cpu_tiles[idx];
  struct Tile *the_gpu_tile = &the_map->gpu_tiles[idx];

  unsigned max_stack_size = the_item->stack_size;
  unsigned remaining = amount;
  unsigned s_idx = 0;

  while (remaining != 0 && s_idx < 3) {
    // Check if this stack is of the same type
    if (the_cpu_tile->stack_amounts[s_idx] == 0) {
      if (remaining >= max_stack_size) {
        the_cpu_tile->stack_amounts[s_idx] = max_stack_size;
        remaining -= max_stack_size;
      } else {
        the_cpu_tile->stack_amounts[s_idx] = remaining;
        remaining = 0;
      }
      the_cpu_tile->stack_recipes[s_idx] = the_item;
      the_cpu_tile->stack_count = s_idx + 1;
      the_gpu_tile->stack_textures[s_idx] = the_item->full_stack_tex;
      the_gpu_tile->stack_count = s_idx + 1;
    } else if (the_cpu_tile->stack_recipes[s_idx] &&
               the_item->key == the_cpu_tile->stack_recipes[s_idx]->key) {
      printf("found a stack of same type for %s\n", the_item->name);
      unsigned can_place =
          (max_stack_size - the_cpu_tile->stack_amounts[s_idx]);

      if (can_place >= remaining) {
        the_cpu_tile->stack_amounts[s_idx] += remaining;
        remaining = 0;
      } else {
        the_cpu_tile->stack_amounts[s_idx] += can_place;
        remaining -= can_place;
        printf("the stack is full, remaining: %d\n", remaining);
      }
    }

    s_idx++;
  }

  printf("after -> %d\n", the_cpu_tile->stack_count);

  return remaining;
}

void G_Add_Items_QC(qcvm_t *qcvm) {
  worker_t *worker = qcvm_get_user_data(qcvm);
  game_t *game = worker->game;

  int map = qcvm_get_parm_int(qcvm, 0);
  float x = qcvm_get_parm_float(qcvm, 1);
  float y = qcvm_get_parm_float(qcvm, 2);
  const char *recipe = qcvm_get_parm_string(qcvm, 3);
  float amount = qcvm_get_parm_float(qcvm, 4);

  if (map < 0 || map >= (int)game->map_count) {
    printf(
        "[ERROR] Assertion G_Add_Items_QC(map >= 0 || map < game->map_count) "
        "[map = %d, map_count = %d] should be "
        "verified.\n",
        map, game->map_count);
    return;
  }

  map_t *the_map = &game->maps[map];

  if (x < 0.0f || x >= the_map->w) {
    printf("[ERROR] Assertion G_Add_Items_QC(x >= 0 || x < "
           "map->w) "
           "[x = %d, map->w = %d] should be "
           "verified.\n",
           (unsigned)x, the_map->w);
    return;
  }

  if (y < 0.0f || y >= the_map->h) {
    printf("[ERROR] Assertion G_Add_Items_QC(y >= 0 || y < "
           "map->h) "
           "[y = %d, map->h = %d] should be "
           "verified.\n",
           (unsigned)x, the_map->h);
    return;
  }

  zpl_u64 key = zpl_fnv64(recipe, strlen(recipe));
  material_t *the_item = G_Materials_get(&game->material_bank, key);

  if (!the_item) {
    printf("[ERROR] Assertion G_Add_Items_QC(recipe exists) [recipe = \"%s\"] "
           "should be verified.\n",
           recipe);
    return;
  }

  if (amount <= 0.0f) {
    printf("[ERROR] Assertion G_Add_Items_QC(amount is positive) [amount = "
           "%.03f] "
           "should be verified.\n",
           amount);
    return;
  }

  float placed = G_Add_Items(game, map, x, y, the_item, (unsigned)amount);

  qcvm_return_float(qcvm, placed);
}

bool G_Find_Nearest_Items(unsigned map, unsigned x, unsigned y,
                          const char *recipe, unsigned start_search) {
  return false;
}

void G_Find_Nearest_Items_QC(qcvm_t *qcvm) {}

void G_Draw_Image_Relative(game_t *game, const char *path, float w, float h,
                           float x, float y, float z) {
  // Is the image loaded?
  zpl_u64 key = zpl_fnv64(path, strlen(path));
  texture_t *texture =
      G_ImmediateTextures_get(&game->immediate_texture_bank, key);

  if (!texture) {
    char complete_path[256];
    sprintf(&complete_path[0], "%s/%s", game->base, path);

    texture_t t = G_LoadSingleTexture(complete_path);

    if (!t.data) {
      printf("[ERROR] Couldn't load texture referenced in "
             "`G_Draw_Image_Relative`, namely \"%s\".\n",
             path);

      return;
    } else {
      G_ImmediateTextures_set(&game->immediate_texture_bank, key, t);
      texture = G_ImmediateTextures_get(&game->immediate_texture_bank, key);

      VK_UploadSingleTexture(game->rend, texture);
    }
  }

  // Now it's loaded for sure, update the state to add a draw call referencing
  // this image
  if (texture) {
    game->state.draws[game->state.draw_count].x = x;
    game->state.draws[game->state.draw_count].y = y;
    game->state.draws[game->state.draw_count].z = z;
    game->state.draws[game->state.draw_count].w = w;
    game->state.draws[game->state.draw_count].h = h;
    game->state.draws[game->state.draw_count].handle = texture->handle;

    game->state.draw_count++;
  }
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

// TODO: G_Prepare_Scene have to be called before calling G_Run_Scene.
// G_Run_Scene only set the current_scene to be the specified scene. It allows
// calling an running the start listener while the previous scene still run (for
// example, to let the loading animation be performed).
void G_Run_Scene(worker_t *worker, const char *scene_name) {
  printf("G_Run_Scene(\"%s\");\n", scene_name);
  game_t *game = worker->game;

  // Fetch the scene with this name
  scene_t *scene = NULL;
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

const vec2 offsets_1[3] = {
    {0.5, 1.0}, // stack_idx==0 / middle of the tile
    {0.0, 0.0},
    {0.0, 0.0},
};
const vec2 offsets_2[3] = {
    {0.05, 1.2f}, // stack_idx==0 / bottom left
    {0.60, 0.7},  // stack_idx==1 / top right
    {0.0},
};
const vec2 offsets_3[3] = {
    {-0.0, 1.37}, // bottom right
    {-0.0, 0.50},
    {0.80, 0.76},
};

void G_WorkerSetupTileText(void *data) {
  unsigned screen_width, screen_height;

  item_text_job_t *the_job = data;

  // worker_t *worker = the_job->worker;
  game_t *game = the_job->game;
  map_t *the_map = &game->maps[game->current_scene->current_map];
  CL_GetViewDim(game->client, &screen_width, &screen_height);

  char amount_str[256];

  for (unsigned col = 0; col < the_map->w; col++) {
    unsigned idx = the_job->row * the_map->w + col;

    cpu_tile_t *the_tile = &the_map->cpu_tiles[idx];

    vec4 tile_pos = {(float)col, (float)the_job->row, 0.0f, 1.0f};
    vec4 screen_space;

    const vec2 *offsets = NULL;
    float scale = 1.9f;

    if (the_tile->stack_count == 1) {
      offsets = offsets_1;
      scale = 1.2;
    } else if (the_tile->stack_count == 2) {
      offsets = offsets_2;
      scale = 1.5;
    } else {
      offsets = offsets_3;
    }

    for (unsigned i = 0; i < 3; i++) {
      if (the_tile->stack_amounts[i] != 0) {
        sprintf(&amount_str[0], "%d", the_tile->stack_amounts[i]);

        vec4 label_pos = {tile_pos[0] + (offsets[i][0] - 0.5f) / scale,
                          tile_pos[1] + (offsets[i][1] - 0.5f) / scale, 0.0f,
                          1.0f};

        glm_mat4_mulv(game->state.fps.view_proj, label_pos, screen_space);

        if (screen_space[0] > 1.0f || screen_space[0] < -1.0f ||
            screen_space[1] > 1.0f || screen_space[1] < -1.0f) {
          continue;
        }

        unsigned len = strlen(amount_str);

        // Naive approach to center the digits, it assumes all char are 20px
        // width
        float current_pos = -((float)len) * 20.0f / 2.0f;
        float start_pos = current_pos - 5.0f;
        float max_height = 0.0f;
        int background_idx = atomic_fetch_add_explicit(&game->state.text_count,
                                                       1, memory_order_relaxed);

        // Digits
        for (unsigned cc = 0; cc < len; cc++) {
          int idx = atomic_fetch_add_explicit(&game->state.text_count, 1,
                                              memory_order_relaxed);

          character_t *character =
              CL_Characters_get(&game->game_character_bank, amount_str[cc]);
          if (!character) {
            continue;
          }

          game->state.texts[idx].color[0] = 1.0f;
          game->state.texts[idx].color[1] = 1.0f;
          game->state.texts[idx].color[2] = 1.0f;
          game->state.texts[idx].color[3] = 1.0f;
          game->state.texts[idx].tex = character->texture_idx;
          game->state.texts[idx].pos[0] = screen_space[0] + (current_pos + character->bearing[0]) / (float)screen_width;
          game->state.texts[idx].pos[1] = screen_space[1] + ((-character->bearing[1]) / (float)screen_height);
          game->state.texts[idx].pos[2] = 0.1;
          game->state.texts[idx].size[0] = character->size[0] / (float)screen_width;
          game->state.texts[idx].size[1] = character->size[1] / (float)screen_height;
          if (character->size[1] > max_height) {
            max_height = character->size[1];
          }

          current_pos += (float)(character->advance >> 6);
        }

        character_t *character =
            CL_Characters_get(&game->game_character_bank, 9999);

        // Adding a background
        game->state.texts[background_idx].color[0] = 0.0f;
        game->state.texts[background_idx].color[1] = 0.0f;
        game->state.texts[background_idx].color[2] = 0.0f;
        game->state.texts[background_idx].color[3] = 0.4f;
        game->state.texts[background_idx].tex = character->texture_idx;
        game->state.texts[background_idx].pos[0] = screen_space[0] + start_pos / (float)screen_width;
        game->state.texts[background_idx].pos[1] = screen_space[1] - ((max_height + 5.0f) / (float)screen_height);
        game->state.texts[background_idx].pos[2] = 0.9;
        game->state.texts[background_idx].size[0] = ((current_pos + 5.0f) - start_pos) / (float)screen_width;
        game->state.texts[background_idx].size[1] = (max_height + 10.0f) / (float)screen_height;
      }
    }
  }
}

void G_WorkerLoadFont(void *data) {
  font_job_t *the_job = data;
  game_t *game = the_job->game;

  FT_Library ft = the_job->ft;

  if (FT_New_Face(ft, the_job->path, 0, the_job->face)) {
    printf("[ERROR] Couldn't load font family "
           "(\"%s\") for the console.\n",
           the_job->path);
    return;
  }

  FT_Set_Pixel_Sizes(*the_job->face, 0, the_job->size);

  wchar_t alphabet[] = L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ()"
                       L"{}:/+-_*012345789おはよう!?èé&#<>.,\\\"ùàç []";

  for (unsigned i = 0; i < sizeof(alphabet) / sizeof(wchar_t); i++) {
    if (FT_Load_Char(*(the_job->face), alphabet[i],
                     FT_LOAD_RENDER | FT_LOAD_COLOR)) {
      printf("[WARNING] Ooopsie doopsie, the char `%lc` isn't supported...\n",
             alphabet[i]);
      continue;
    }

    FT_GlyphSlot the_glyph = (*the_job->face)->glyph;
    unsigned char *pixel_data = the_glyph->bitmap.buffer;
    // See console note
    if (the_glyph->bitmap.width == 0 || the_glyph->bitmap.rows == 0) {
      FT_Load_Char(*the_job->face, L'_', FT_LOAD_RENDER | FT_LOAD_COLOR);
      the_glyph = (*the_job->face)->glyph;
      pixel_data = game->white_space;
    }

    unsigned data_size = the_glyph->bitmap.width * the_glyph->bitmap.rows;
    pixel_data = memcpy(malloc(data_size), pixel_data, data_size);

    zpl_mutex_lock(&game->font_mutex);
    game->font_textures[game->font_texture_count] = (texture_t){
        .c = 1,
        .width = the_glyph->bitmap.width,
        .height = the_glyph->bitmap.rows,
        .data = pixel_data,
        .label = "character",
    };

    CL_Characters_set(the_job->character_bank, alphabet[i],
                      (character_t){
                          game->font_texture_count,
                          {
                              the_glyph->bitmap.width,
                              the_glyph->bitmap.rows,
                          },
                          {
                              the_glyph->bitmap_left,
                              the_glyph->bitmap_top,
                          },
                          the_glyph->advance.x,
                      });
    game->font_texture_count++;
    zpl_mutex_unlock(&game->font_mutex);
  }

  // Adding a blank character used to draw background
  zpl_mutex_lock(&game->font_mutex);
  unsigned char *blank = malloc(64 * 64);
  for (unsigned i = 0; i < 64 * 64; i++) {
    blank[i] = 255;
  }
  game->font_textures[game->font_texture_count] = (texture_t){
      .c = 1,
      .width = 64,
      .height = 64,
      .data = blank,
      .label = "character",
  };

  CL_Characters_set(the_job->character_bank, 9999,
                    (character_t){
                        game->font_texture_count,
                        {
                            0,
                            0,
                        },
                        {
                            0,
                            0,
                        },
                        0,
                    });
  game->font_texture_count++;
  zpl_mutex_unlock(&game->font_mutex);
}

void G_WorkerLoadTexture(void *data) {
  texture_job_t *the_job = data;
  game_t *game = the_job->game;

  char the_path[256];
  sprintf(&the_path[0], "%s/%s", game->base, the_job->path);

  texture_t tex = G_LoadSingleTexture(the_path);

  if (!tex.data) {
    printf("[ERROR] Couldn't load texture `%s` | `%s` (in a worker thread).\n",
           the_job->path, the_path);
    return;
  }

  zpl_mutex_lock(&game->map_texture_mutex);
  if (game->map_texture_count == game->map_texture_capacity) {
    game->map_texture_capacity *= 2;
    game->map_textures = realloc(
        game->map_textures, game->map_texture_capacity * sizeof(texture_t));
  }
  game->map_textures[game->map_texture_count] = tex;
  *the_job->dest_text = game->map_texture_count;
  game->map_texture_count++;

  zpl_mutex_unlock(&game->map_texture_mutex);
}

void G_Load_Game(worker_t *worker) {
  game_t *game = worker->game;

  zpl_f64 now = zpl_time_rel();

  // Load the default texture, that'll have a null index (index == 0)
  // So, everytime there is an error, it'll be displayed
  // WARNING: kinda weeb stuff
  game->map_textures[0] = G_LoadSingleTextureFromMemory(
      &no_image[0], no_image_size,
      memcpy(malloc(23), "resources/no_image.png", 23));
  game->map_texture_count = 1;

  font_job_t font_job_console = {
      .game = game,
      .ft = game->console_ft,
      .character_bank = &game->console_character_bank,
      .face = &game->console_face,
      .size = 48,
      .path = "../source/resources/JF-Dot-Paw16.ttf",
  };

  font_job_t font_job_game = {
      .game = game,
      .ft = game->game_ft,
      .character_bank = &game->game_character_bank,
      .face = &game->game_face,
      .size = 24,
      .path = "../source/resources/InterDisplay-ExtraBold.ttf",
  };

  zpl_jobs_enqueue(&game->job_sys, G_WorkerLoadFont, &font_job_console);
  zpl_jobs_enqueue(&game->job_sys, G_WorkerLoadFont, &font_job_game);

  texture_job_t *texture_jobs =
      calloc(zpl_array_count(game->material_bank.entries) * 3 +
                 zpl_array_count(game->wall_bank.entries) * 16 +
                 zpl_array_count(game->terrain_bank.entries) * 3,
             sizeof(texture_job_t));

  for (unsigned i = 0; i < zpl_array_count(game->material_bank.entries); i++) {
    material_bank_tEntry *entry = &game->material_bank.entries[i];
    material_t *mat = &entry->value;

    texture_job_t *low_stack_job = &texture_jobs[i * 3 + 0];
    *low_stack_job = (texture_job_t){
        .game = game,
        .dest_text = &mat->low_stack_tex,
        .path = mat->low_stack_path,
    };
    texture_job_t *half_stack_job = &texture_jobs[i * 3 + 1];
    *half_stack_job = (texture_job_t){
        .game = game,
        .dest_text = &mat->half_stack_tex,
        .path = mat->half_stack_path,
    };
    texture_job_t *full_stack_job = &texture_jobs[i * 3 + 2];
    *full_stack_job = (texture_job_t){
        .game = game,
        .dest_text = &mat->full_stack_tex,
        .path = mat->full_stack_path,
    };

    zpl_jobs_enqueue(&game->job_sys, G_WorkerLoadTexture, low_stack_job);
    zpl_jobs_enqueue(&game->job_sys, G_WorkerLoadTexture, half_stack_job);
    zpl_jobs_enqueue(&game->job_sys, G_WorkerLoadTexture, full_stack_job);
  }

  unsigned offset = zpl_array_count(game->material_bank.entries) * 3;

  for (unsigned i = 0; i < zpl_array_count(game->wall_bank.entries); i++) {
    wall_bank_tEntry *entry = &game->wall_bank.entries[i];
    wall_t *wall = &entry->value;

    texture_job_t *only_left_job = &texture_jobs[offset + i * 16 + 0];
    *only_left_job = (texture_job_t){
        .game = game,
        .dest_text = &wall->only_left_tex,
        .path = wall->only_left_path,
    };
    texture_job_t *only_right_job = &texture_jobs[offset + i * 16 + 1];
    *only_right_job = (texture_job_t){
        .game = game,
        .dest_text = &wall->only_right_tex,
        .path = wall->only_right_path,
    };
    texture_job_t *only_top_job = &texture_jobs[offset + i * 16 + 2];
    *only_top_job = (texture_job_t){
        .game = game,
        .dest_text = &wall->only_top_tex,
        .path = wall->only_top_path,
    };
    texture_job_t *only_bottom_job = &texture_jobs[offset + i * 16 + 3];
    *only_bottom_job = (texture_job_t){
        .game = game,
        .dest_text = &wall->only_bottom_tex,
        .path = wall->only_bottom_path,
    };
    texture_job_t *all_job = &texture_jobs[offset + i * 16 + 4];
    *all_job = (texture_job_t){
        .game = game,
        .dest_text = &wall->all_tex,
        .path = wall->all_path,
    };
    texture_job_t *left_right_bottom_job = &texture_jobs[offset + i * 16 + 5];
    *left_right_bottom_job = (texture_job_t){
        .game = game,
        .dest_text = &wall->left_right_bottom_tex,
        .path = wall->left_right_bottom_path,
    };
    texture_job_t *left_right_job = &texture_jobs[offset + i * 16 + 6];
    *left_right_job = (texture_job_t){
        .game = game,
        .dest_text = &wall->left_right_tex,
        .path = wall->left_right_path,
    };
    texture_job_t *left_right_top_job = &texture_jobs[offset + i * 16 + 7];
    *left_right_top_job = (texture_job_t){
        .game = game,
        .dest_text = &wall->left_right_top_tex,
        .path = wall->left_right_top_path,
    };
    texture_job_t *top_bottom_job = &texture_jobs[offset + i * 16 + 8];
    *top_bottom_job = (texture_job_t){
        .game = game,
        .dest_text = &wall->top_bottom_tex,
        .path = wall->top_bottom_path,
    };
    texture_job_t *right_top_job = &texture_jobs[offset + i * 16 + 9];
    *right_top_job = (texture_job_t){
        .game = game,
        .dest_text = &wall->right_top_tex,
        .path = wall->right_top_path,
    };
    texture_job_t *left_top_job = &texture_jobs[offset + i * 16 + 10];
    *left_top_job = (texture_job_t){
        .game = game,
        .dest_text = &wall->left_top_tex,
        .path = wall->left_top_path,
    };
    texture_job_t *right_bottom_job = &texture_jobs[offset + i * 16 + 11];
    *right_bottom_job = (texture_job_t){
        .game = game,
        .dest_text = &wall->right_bottom_tex,
        .path = wall->right_bottom_path,
    };
    texture_job_t *left_bottom_job = &texture_jobs[offset + i * 16 + 12];
    *left_bottom_job = (texture_job_t){
        .game = game,
        .dest_text = &wall->left_bottom_tex,
        .path = wall->left_bottom_path,
    };
    texture_job_t *left_top_bottom_job = &texture_jobs[offset + i * 16 + 13];
    *left_top_bottom_job = (texture_job_t){
        .game = game,
        .dest_text = &wall->left_top_bottom_tex,
        .path = wall->left_top_bottom_path,
    };
    texture_job_t *right_top_bottom_job = &texture_jobs[offset + i * 16 + 14];
    *right_top_bottom_job = (texture_job_t){
        .game = game,
        .dest_text = &wall->right_top_bottom_tex,
        .path = wall->right_top_bottom_path,
    };
    texture_job_t *nothing_job = &texture_jobs[offset + i * 16 + 15];
    *nothing_job = (texture_job_t){
        .game = game,
        .dest_text = &wall->nothing_tex,
        .path = wall->nothing_path,
    };

    zpl_jobs_enqueue(&game->job_sys, G_WorkerLoadTexture, only_left_job);
    zpl_jobs_enqueue(&game->job_sys, G_WorkerLoadTexture, only_right_job);
    zpl_jobs_enqueue(&game->job_sys, G_WorkerLoadTexture, only_top_job);
    zpl_jobs_enqueue(&game->job_sys, G_WorkerLoadTexture, only_bottom_job);
    zpl_jobs_enqueue(&game->job_sys, G_WorkerLoadTexture, all_job);
    zpl_jobs_enqueue(&game->job_sys, G_WorkerLoadTexture,
                     left_right_bottom_job);
    zpl_jobs_enqueue(&game->job_sys, G_WorkerLoadTexture, left_right_job);
    zpl_jobs_enqueue(&game->job_sys, G_WorkerLoadTexture, left_right_top_job);
    zpl_jobs_enqueue(&game->job_sys, G_WorkerLoadTexture, top_bottom_job);
    zpl_jobs_enqueue(&game->job_sys, G_WorkerLoadTexture, right_top_job);
    zpl_jobs_enqueue(&game->job_sys, G_WorkerLoadTexture, left_top_job);
    zpl_jobs_enqueue(&game->job_sys, G_WorkerLoadTexture, right_bottom_job);
    zpl_jobs_enqueue(&game->job_sys, G_WorkerLoadTexture, left_bottom_job);
    zpl_jobs_enqueue(&game->job_sys, G_WorkerLoadTexture, left_top_bottom_job);
    zpl_jobs_enqueue(&game->job_sys, G_WorkerLoadTexture, right_top_bottom_job);
    zpl_jobs_enqueue(&game->job_sys, G_WorkerLoadTexture, nothing_job);
  }

  offset += zpl_array_count(game->wall_bank.entries) * 3;

  for (unsigned i = 0; i < zpl_array_count(game->terrain_bank.entries); i++) {
    terrain_bank_tEntry *entry = &game->terrain_bank.entries[i];
    terrain_t *terrain = &entry->value;

    texture_job_t *variant1_job = &texture_jobs[offset + i * 3 + 0];
    *variant1_job = (texture_job_t){
        .game = game,
        .dest_text = &terrain->variant1_tex,
        .path = terrain->variant1_path,
    };
    texture_job_t *variant2_job = &texture_jobs[offset + i * 3 + 1];
    *variant2_job = (texture_job_t){
        .game = game,
        .dest_text = &terrain->variant2_tex,
        .path = terrain->variant2_path,
    };
    texture_job_t *variant3_job = &texture_jobs[offset + i * 3 + 2];
    *variant3_job = (texture_job_t){
        .game = game,
        .dest_text = &terrain->variant3_tex,
        .path = terrain->variant3_path,
    };

    zpl_jobs_enqueue(&game->job_sys, G_WorkerLoadTexture, variant1_job);
    zpl_jobs_enqueue(&game->job_sys, G_WorkerLoadTexture, variant2_job);
    zpl_jobs_enqueue(&game->job_sys, G_WorkerLoadTexture, variant3_job);
  }

  while (!zpl_jobs_done(&game->job_sys)) {
    zpl_jobs_process(&game->job_sys);

    game_state_t *state = G_TickGame(game->client, game);

    CL_DrawClient(game->client, game, state);
  }

  printf("[VERBOSE] Loading game took `%f` ms\n",
         (float)(zpl_time_rel() - now));

  VK_UploadMapTextures(game->rend, game->map_textures, game->map_texture_count);
  VK_UploadFontTextures(game->rend, game->font_textures,
                        game->font_texture_count);

  G_Run_Scene(worker, game->next_scene);

  free(game->next_scene);
  game->next_scene = NULL;

  free(texture_jobs);
}

void G_Load_Game_QC(qcvm_t *qcvm) {
  worker_t *worker = (worker_t *)qcvm_get_user_data(qcvm);
  game_t *game = worker->game;

  const char *loading_scene_name = qcvm_get_parm_string(qcvm, 0);
  const char *next_scene = qcvm_get_parm_string(qcvm, 1);

  if (worker->id != 0) {
    printf(
        "`G_Load_Game_QC` can only be called from thread 0 (main thread).\n");
    return;
  }

  game->next_scene = memcpy(malloc(strlen(next_scene) + 1), next_scene,
                            strlen(next_scene) + 1);

  G_Run_Scene(worker, loading_scene_name);

  G_Load_Game(worker);
}

const char *G_Get_Last_Asset_Loaded(game_t *game) {
  return "not yet implemented!";
}

void G_Get_Last_Asset_Loaded_QC(qcvm_t *qcvm) {
  game_t *game = ((worker_t *)qcvm_get_user_data(qcvm))->game;

  qcvm_return_string(qcvm, G_Get_Last_Asset_Loaded(game));
}

void G_Add_Wall_QC(qcvm_t *qcvm) {
  worker_t *worker = qcvm_get_user_data(qcvm);
  game_t *game = (game_t *)worker->game;
  const char *recipe = qcvm_get_parm_string(qcvm, 0);
  float x = qcvm_get_parm_float(qcvm, 1);
  float y = qcvm_get_parm_float(qcvm, 2);

  float health = qcvm_get_parm_float(qcvm, 3);
  bool to_build = qcvm_get_parm_int(qcvm, 4); // Ignored for the time being
  int map = qcvm_get_parm_int(qcvm, 4);
  zpl_unused(to_build);

  if (health <= 0.0f) {
    printf("[ERROR] Assertion G_Add_Wall_QC(health > 0.0) [health = %f] should "
           "be verified.\n",
           health);
    return;
  }

  if (map < 0 || map >= (int)game->map_count) {
    printf("[ERROR] Assertion G_Add_Wall_QC(map >= 0 || map < game->map_count) "
           "[map = %d, map_count = %d] should be "
           "verified.\n",
           map, game->map_count);
    return;
  }

  map_t *the_map = &game->maps[map];

  zpl_u64 key = zpl_fnv64(recipe, strlen(recipe));
  wall_t *the_wall = G_Walls_get(&game->wall_bank, key);

  if (!the_wall) {
    printf("[ERROR] Assertion G_Add_Wall_QC(recipe exists) [recipe = \"%s\"] "
           "should be verified.\n",
           recipe);
    return;
  }

  if (x < 0.0f || x >= the_map->w) {
    printf("[ERROR] Assertion G_Map_Set_Terrain_Type(x >= 0 || x < "
           "map->w) "
           "[x = %d, map->w = %d] should be "
           "verified.\n",
           (unsigned)x, the_map->w);
    return;
  }

  if (y < 0.0f || y >= the_map->h) {
    printf("[ERROR] Assertion G_Map_Set_Terrain_Type(y >= 0 || y < "
           "map->h) "
           "[y = %d, map->h = %d] should be "
           "verified.\n",
           (unsigned)x, the_map->h);
    return;
  }

  G_Add_Wall(game, map, x, y, health, the_wall);
}

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
  game->scenes[game->scene_count].current_map = -1;
  zpl_mutex_init(&game->scenes[game->scene_count].scene_mutex);
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
      printf("[ERROR] QuakeC code specified a non-existent scene `%s` when "
             "trying "
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
      printf("[ERROR] QuakeC code specified a non-existent scene `%s` when "
             "trying "
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
      printf("[ERROR] QuakeC code specified a non-existent scene `%s` when "
             "trying "
             "to attach a `G_SCENE_UPDATE` listener.\n",
             attachment);
    }

    break;
  }
  case G_CAMERA_UPDATE: {
    scene_t *the_scene = G_Get_Scene_By_Name(game, attachment);

    if (the_scene) {
      if (the_scene->camera_update_listener_count == 16) {
        printf("[ERROR] Reached max number of `G_CAMERA_UPDATE` for the scene "
               "`%s`.\n",
               attachment);

        return;
      }
      the_scene
          ->camera_update_listeners[the_scene->camera_update_listener_count]
          .qcvm_func = func;
      the_scene->camera_update_listener_count++;
    } else {
      printf("[ERROR] QuakeC code specified a non-existent scene `%s` when "
             "trying "
             "to attach a `G_CAMERA_UPDATE` listener.\n",
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

void G_Get_Camera_Position_QC(qcvm_t *qcvm) {
  game_t *game = ((worker_t *)qcvm_get_user_data(qcvm))->game;
  qcvm_return_vector(qcvm, game->state.fps.pos[0], game->state.fps.pos[1],
                     game->state.fps.pos[2]);
}

void G_Set_Camera_Position_QC(qcvm_t *qcvm) {
  game_t *game = ((worker_t *)qcvm_get_user_data(qcvm))->game;

  qcvm_vec3_t vec = qcvm_get_parm_vector(qcvm, 0);

  game->state.fps.pos[0] = vec.x;
  game->state.fps.pos[1] = vec.y;
  game->state.fps.pos[2] = vec.z;
}

void G_Get_Camera_Zoom_QC(qcvm_t *qcvm) {
  game_t *game = ((worker_t *)qcvm_get_user_data(qcvm))->game;

  qcvm_return_float(qcvm, game->state.fps.zoom);
}

void G_Set_Camera_Zoom_QC(qcvm_t *qcvm) {
  game_t *game = ((worker_t *)qcvm_get_user_data(qcvm))->game;

  game->state.fps.zoom = qcvm_get_parm_float(qcvm, 0);

  // printf("game->state.fps.zoom = %f\n", game->state.fps.zoom);
}

void G_Get_Axis_QC(qcvm_t *qcvm) {
  game_t *game = ((worker_t *)qcvm_get_user_data(qcvm))->game;
  const char *axis = qcvm_get_parm_string(qcvm, 0);

  if (strcmp(axis, "vertical") == 0) {
    qcvm_return_float(qcvm, CL_GetInput(game->client)->movement.x_axis);
  } else if (strcmp(axis, "horizontal") == 0) {
    qcvm_return_float(qcvm, CL_GetInput(game->client)->movement.y_axis);
  } else if (strcmp(axis, "mouse_wheel") == 0) {
    qcvm_return_float(qcvm, CL_GetInput(game->client)->wheel);
  }
}

void G_Get_Mouse_Position_QC(qcvm_t *qcvm) {
  game_t *game = ((worker_t *)qcvm_get_user_data(qcvm))->game;
  qcvm_return_vector(qcvm, CL_GetInput(game->client)->mouse_x,
                     CL_GetInput(game->client)->mouse_y, 0.0f);
}

void G_Get_Screen_Size_QC(qcvm_t *qcvm) {
  game_t *game = ((worker_t *)qcvm_get_user_data(qcvm))->game;
  unsigned w, h;
  CL_GetViewDim(game->client, &w, &h);
  qcvm_return_vector(qcvm, (float)w, (float)h, 0.0f);
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
      .argc = 6,
      .args[0] = {.name = "recipe", .type = QCVM_STRING},
      .args[1] = {.name = "x", .type = QCVM_FLOAT},
      .args[2] = {.name = "y", .type = QCVM_FLOAT},
      .args[3] = {.name = "health", .type = QCVM_FLOAT},
      .args[4] = {.name = "to_build", .type = QCVM_INT},
      .args[5] = {.name = "map", .type = QCVM_INT},
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

  qcvm_export_t export_G_Set_Current_Map = {
      .func = G_Set_Current_Map_QC,
      .name = "G_Set_Current_Map",
      .argc = 2,
      .args[0] = {.name = "scene", .type = QCVM_STRING},
      .args[1] = {.name = "map", .type = QCVM_INT},
  };

  qcvm_export_t export_G_Get_Camera_Position = {
      .func = G_Get_Camera_Position_QC,
      .name = "G_Get_Camera_Position",
      .argc = 0,
      .type = QCVM_VECTOR,
  };

  qcvm_export_t export_G_Set_Camera_Position = {
      .func = G_Set_Camera_Position_QC,
      .name = "G_Set_Camera_Position",
      .argc = 1,
      .args[0] = {.name = "value", .type = QCVM_VECTOR},
  };

  qcvm_export_t export_G_Get_Camera_Zoom = {
      .func = G_Get_Camera_Zoom_QC,
      .name = "G_Get_Camera_Zoom",
      .argc = 1,
      .type = QCVM_FLOAT,
  };

  qcvm_export_t export_G_Set_Camera_Zoom = {
      .func = G_Set_Camera_Zoom_QC,
      .name = "G_Set_Camera_Zoom",
      .argc = 1,
      .args[0] = {.name = "value", .type = QCVM_FLOAT},
  };

  qcvm_export_t export_G_Get_Axis = {
      .func = G_Get_Axis_QC,
      .name = "G_Get_Axis",
      .argc = 1,
      .args[0] = {.name = "axis", .type = QCVM_STRING},
      .type = QCVM_FLOAT,
  };

  qcvm_export_t export_G_Get_Mouse_Position = {
      .func = G_Get_Mouse_Position_QC,
      .name = "G_Get_Mouse_Position",
      .argc = 0,
      .type = QCVM_VECTOR,
  };

  qcvm_export_t export_G_Get_Screen_Size = {
      .func = G_Get_Screen_Size_QC,
      .name = "G_Get_Screen_Size",
      .argc = 0,
      .type = QCVM_VECTOR,
  };

  qcvm_export_t export_G_Add_Items = {
      .func = G_Add_Items_QC,
      .name = "G_Add_Items",
      .argc = 5,
      .args[0] = {.name = "map", .type = QCVM_INT},
      .args[1] = {.name = "x", .type = QCVM_FLOAT},
      .args[2] = {.name = "y", .type = QCVM_FLOAT},
      .args[3] = {.name = "recipe", .type = QCVM_STRING},
      .args[4] = {.name = "amount", .type = QCVM_FLOAT},
      .type = QCVM_VOID,
  };

  qcvm_export_t export_G_Find_Nearest_Items = {
      .func = G_Find_Nearest_Items_QC,
      .name = "G_Find_Nearest_Items",
      .argc = 5,
      .args[0] = {.name = "map", .type = QCVM_INT},
      .args[1] = {.name = "org_x", .type = QCVM_FLOAT},
      .args[2] = {.name = "org_y", .type = QCVM_FLOAT},
      .args[3] = {.name = "recipe", .type = QCVM_STRING},
      .args[4] = {.name = "start_search", .type = QCVM_INT},
      .type = QCVM_VECTOR,
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
  qcvm_add_export(worker->qcvm, &export_G_Set_Current_Map);
  qcvm_add_export(worker->qcvm, &export_G_Get_Camera_Position);
  qcvm_add_export(worker->qcvm, &export_G_Set_Camera_Position);
  qcvm_add_export(worker->qcvm, &export_G_Get_Camera_Zoom);
  qcvm_add_export(worker->qcvm, &export_G_Set_Camera_Zoom);
  qcvm_add_export(worker->qcvm, &export_G_Get_Axis);
  qcvm_add_export(worker->qcvm, &export_G_Get_Mouse_Position);
  qcvm_add_export(worker->qcvm, &export_G_Get_Screen_Size);
  qcvm_add_export(worker->qcvm, &export_G_Add_Items);
  qcvm_add_export(worker->qcvm, &export_G_Find_Nearest_Items);
}

bool G_Load(client_t *client, game_t *game) {
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
    G_TerrainInstall(game->workers[i].qcvm);

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

  game->entities = VK_GetEntities(game->rend);

  free(bytes);

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

character_t *G_GetCharacter(game_t *game, const char *family, wchar_t c) {
  // TODO: we only compare the first letter, cause we only have two font family
  // available, sorry for that
  if (family[0] == 'c') { // "console"
    return CL_Characters_get(&game->console_character_bank, c);
  } else if (family[0] == 'g') { // "game"
    return CL_Characters_get(&game->game_character_bank, c);
  }

  return NULL;
}
