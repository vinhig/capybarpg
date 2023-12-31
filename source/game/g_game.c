#include "game/g_game.h"
#include "cglm/mat4.h"
#include "cglm/util.h"
#include "freetype/freetype.h"
#include "intlist.h"
#include "qcvm.h"
#include "vk/vk_vulkan.h"
#include <client/cl_input.h>
#include <float.h>
#include <game/g_private.h>
#include <math.h>
#include <stbi_image.h>

#include <jps.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

void G_WorkerRegisterThread(void *data);
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

  zpl_mutex_init(&game->thread_ids_mutex);
  zpl_mutex_lock(&game->thread_ids_mutex);

  for (unsigned i = 0; i < game->worker_count; i++) {
    zpl_jobs_enqueue(&game->job_sys, G_WorkerRegisterThread, game);
  }

  zpl_jobs_process(&game->job_sys);

  zpl_mutex_unlock(&game->thread_ids_mutex);

  zpl_affinity_destroy(&af);

  zpl_mutex_init(&game->map_texture_mutex);
  zpl_mutex_init(&game->font_mutex);

  G_Materials_init(&game->material_bank, zpl_heap_allocator());
  G_ImmediateTextures_init(&game->immediate_texture_bank, zpl_heap_allocator());
  G_Walls_init(&game->wall_bank, zpl_heap_allocator());
  G_Terrains_init(&game->terrain_bank, zpl_heap_allocator());
  G_Pawns_init(&game->pawn_bank, zpl_heap_allocator());

  // Two freetype library because font loading may happen on different threads
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

  for (unsigned i = 0; i < 16; i++) {
    qcvm_free(game->qcvms[i]);
  }

  for (unsigned i = 0; i < game->map_count; i++) {
    free(game->maps[i].cpu_tiles);
    for (unsigned j = 0; j < 16; j++) {
      jps_destroy(game->maps[i].jps_maps[j]);
    }
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

void G_WorkerRegisterThread(void *data) {
  game_t *game = data;
  unsigned os_id = zpl_thread_current_id();

  for (unsigned i = 0; i < 16; i++) {
    if (game->thread_ids[i].os_id == os_id) {
      printf("Already registered thread %d. Let's pray together our Lord and Savior.\n", i);
      abort();
      return;
    }
  }

  int my_idx = atomic_fetch_add(&game->registered_thread_idx, 1);
  game->thread_ids[my_idx].os_id = os_id;

  zpl_mutex_lock(&game->thread_ids_mutex);
  zpl_mutex_unlock(&game->thread_ids_mutex);
}

unsigned G_GetThreadId(game_t *game, unsigned os_id) {
  for (unsigned i = 0; i < 16; i++) {
    if (game->thread_ids[i].os_id == os_id) {
      return i;
    }
  }

  printf("G_GetThreadId was called from an unregistred thread. Something went super wrong...\n");
  printf("Let's abort and pray together.\n");
  abort();
}

void G_WorkerThinkAgent(void *data) {
  think_job_t *job = data;
  game_t *game = job->game;
  unsigned agent = job->agent;

  unsigned thread_id = G_GetThreadId(game, zpl_thread_current_id());

  qcvm_t *qcvm = game->qcvms[thread_id];

  for (unsigned t = 0; t < game->current_scene->agent_think_listener_count; t++) {
    qcvm_set_parm_int(qcvm, 0, game->current_scene->current_map);
    qcvm_set_parm_int(qcvm, 1, agent);
    qcvm_set_parm_float(qcvm, 2, game->cpu_agents[agent].type);
    qcvm_set_parm_vector(qcvm, 3, game->transforms[agent].position[0], game->transforms[agent].position[1], 0.0f);
    qcvm_set_parm_float(qcvm, 4, game->cpu_agents[agent].state);

    qcvm_run(qcvm, game->current_scene->agent_think_listeners[t].qcvm_func);
  }
}

void G_WorkerUpdateAgents(void *data) {
  path_finding_job_t *the_job = data;
  game_t *game = the_job->game;
  unsigned agent = the_job->agent;
  map_t *the_map = &game->maps[the_job->map];

  if (game->cpu_agents[agent].state == AGENT_PATH_FINDING) {
    // Find a jps_map that is available hehe
    // That's called lock free stuffy stuff

    unsigned thread_id = G_GetThreadId(game, zpl_thread_current_id());

    struct map *jps_map = the_map->jps_maps[thread_id];

    jps_set_start(jps_map, game->transforms[agent].position[0],
                  game->transforms[agent].position[1]);
    jps_set_end(jps_map, game->cpu_agents[agent].target[0],
                game->cpu_agents[agent].target[1]);

    IntList *list = il_create(2);
    jps_path_finding(jps_map, 2, list);

    unsigned size = il_size(list);

    if (size == 0) {
      game->cpu_agents[agent].state = AGENT_NOTHING;
      il_destroy(list);
      return;
    }

    game->cpu_agents[agent].computed_path.count = size;
    game->cpu_agents[agent].computed_path.current = 0;
    if (game->cpu_agents[agent].computed_path.points) {
      free(game->cpu_agents[agent].computed_path.points);
    }
    game->cpu_agents[agent].computed_path.points = calloc(size, sizeof(vec2));
    for (unsigned p = 0; p < size; p++) {
      int x = il_get(list, (size - p - 1), 0);
      int y = il_get(list, (size - p - 1), 1);
      game->cpu_agents[agent].computed_path.points[p][0] = x;
      game->cpu_agents[agent].computed_path.points[p][1] = y;
    }

    game->cpu_agents[agent].state = AGENT_MOVING;

    il_destroy(list);
  } else if (game->cpu_agents[agent].state == AGENT_MOVING) {
    unsigned c = game->cpu_agents[agent].computed_path.current;
    if (c < game->cpu_agents[agent].computed_path.count) {
      // Compute the direction to take
      vec2 next_pos;
      glm_vec2(game->cpu_agents[agent].computed_path.points[c], next_pos);
      vec2 d;
      glm_vec2_sub(next_pos, game->transforms[agent].position, d);
      vec2 s;
      glm_vec2_sign(d, s);
      d[0] = glm_min(fabs(d[0]), game->cpu_agents[agent].speed) * s[0];
      d[1] = glm_min(fabs(d[1]), game->cpu_agents[agent].speed) * s[1];

      // Reflect the direction on the related GPU agent
      // Visual and Animation is supposed to change
      vec2 supposed_d = {
          1.0f * s[0],
          1.0f * s[1],
      };

      if (supposed_d[0] != 0.0f || supposed_d[1] != 0.0f) {
        game->gpu_agents[agent].direction[0] = supposed_d[0];
        game->gpu_agents[agent].direction[1] = supposed_d[1];
      }

      // Apply the movement
      glm_vec2_add(game->transforms[agent].position, d,
                   game->transforms[agent].position);
      if (game->transforms[agent].position[0] == next_pos[0] &&
          game->transforms[agent].position[1] == next_pos[1]) {
        game->cpu_agents[agent].computed_path.current++;
      }
    } else {
      game->cpu_agents[agent].state = AGENT_NOTHING;
      game->gpu_agents[agent].direction[0] = 0.0f;
      game->gpu_agents[agent].direction[1] = 0.0f;
    }
  }
}

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

  game->delta_time = zpl_time_rel() - game->last_time;

  game->last_time = zpl_time_rel();

  if (!game->current_scene) {
    printf("[WARNING] No current scene hehe...\n");
  } else {
    for (unsigned i = 0; i < game->current_scene->update_listener_count; i++) {
      qcvm_set_parm_float(game->qcvms[0], 0, game->delta_time);
      qcvm_run(game->qcvms[0],
               game->current_scene->update_listeners[i].qcvm_func);
    }

    for (unsigned i = 0; i < game->current_scene->camera_update_listener_count;
         i++) {
      qcvm_set_parm_float(game->qcvms[0], 0, game->delta_time);
      qcvm_run(game->qcvms[0],
               game->current_scene->camera_update_listeners[i].qcvm_func);
    }

    // TODO: should allocate a memory arena or something
    unsigned agent_count = 0;
    for (unsigned i = 0; i < game->entity_count; i++) {
      unsigned signature = game->entities[i];

      if (signature & agent_signature) {
        agent_count++;
      }
    }

    if (agent_count != 0) {
      think_job_t *think_jobs = calloc(agent_count, sizeof(think_job_t));
      unsigned agent_idx = 0;

      for (unsigned i = 0; i < game->entity_count; i++) {
        unsigned signature = game->entities[i];

        if (signature & agent_signature) {
          think_jobs[i] = (think_job_t){
              .agent = agent_idx,
              .game = game,
          };
          zpl_jobs_enqueue(&game->job_sys, G_WorkerThinkAgent, &think_jobs[agent_idx]);
          agent_idx++;
        }
      }

      while (!zpl_jobs_done(&game->job_sys)) {
        zpl_jobs_process(&game->job_sys);
      };

      free(think_jobs);

      path_finding_job_t *path_finding_jobs = calloc(agent_count, sizeof(path_finding_job_t));
      agent_idx = 0;
      for (unsigned i = 0; i < game->entity_count; i++) {
        unsigned signature = game->entities[i];

        if (signature & agent_signature) {
          path_finding_jobs[i] = (path_finding_job_t){
              .agent = i,
              .game = game,
              .map = game->current_scene->current_map,
          };
          zpl_jobs_enqueue(&game->job_sys, G_WorkerUpdateAgents, &path_finding_jobs[agent_idx]);
          agent_idx++;
        }
      }

      while (!zpl_jobs_done(&game->job_sys)) {
        zpl_jobs_process(&game->job_sys);
      };

      free(path_finding_jobs);
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
  for (unsigned i = 0; i < 16; i++) {
    the_map->jps_maps[i] = jps_create(w, h);
  }
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
  game_t *game = qcvm_get_user_data(qcvm);

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
  game_t *game = qcvm_get_user_data(qcvm);

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
  // * pawns
  // We extract them here in a specific order, cause there are some dependencies
  // (furnitures and walls are built with materials for example)
  zpl_json_object *terrains = zpl_adt_find(&recipes, "terrains", false);
  zpl_json_object *materials = zpl_adt_find(&recipes, "materials", false);
  zpl_json_object *walls = zpl_adt_find(&recipes, "walls", false);
  zpl_json_object *furnitures = zpl_adt_find(&recipes, "furnitures", false);
  zpl_json_object *pawns = zpl_adt_find(&recipes, "pawns", false);

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

  if (pawns && pawns->type == ZPL_ADT_TYPE_ARRAY) {
    unsigned size = zpl_array_count(pawns->nodes);

    for (unsigned i = 0; i < size; i++) {
      zpl_adt_node *element = &pawns->nodes[i];
      zpl_adt_node *id_node = zpl_adt_find(element, "id", false);

      if (!id_node || id_node->type != ZPL_ADT_TYPE_STRING) {
        printf("[WARNING] The element %d in the 'pawns' list doesn't have "
               "an ID (of type string).\n",
               i);
        continue;
      } else {
        zpl_u64 key = zpl_fnv64(id_node->string, strlen(id_node->string));
        // Does the bank contains this element? If not create an empty one.
        pawn_t *pawn = G_Pawns_get(&game->pawn_bank, key);

        if (!pawn) {
          G_Pawns_set(&game->pawn_bank, key, (pawn_t){.revision = 1});
          pawn = G_Pawns_get(&game->pawn_bank, key);
        } else {
          pawn->revision += 1;
        }

        zpl_adt_node *name_node = zpl_adt_find(element, "name", false);
        zpl_adt_node *sprites_node = zpl_adt_find(element, "sprites", false);

        zpl_adt_node *north_node = NULL;
        zpl_adt_node *south_node = NULL;
        zpl_adt_node *east_node = NULL;

        // Some field may be NULL if not present, we don't care
        if (name_node && name_node->type == ZPL_ADT_TYPE_STRING) {
          if (pawn->name) {
            free(pawn->name);
          }
          pawn->name =
              memcpy(malloc(strlen(name_node->string) + 1), name_node->string,
                     strlen(name_node->string) + 1);
        }

        if (sprites_node && sprites_node->type == ZPL_ADT_TYPE_OBJECT) {
          north_node = zpl_adt_find(sprites_node, "north", false);
          south_node = zpl_adt_find(sprites_node, "south", false);
          east_node = zpl_adt_find(sprites_node, "east", false);

          if (north_node && north_node->type == ZPL_ADT_TYPE_STRING) {
            if (pawn->north_path) {
              free(pawn->north_path);
            }
            pawn->north_path = memcpy(
                malloc(strlen(north_node->string) + 1),
                north_node->string, strlen(north_node->string) + 1);
          }
          if (south_node && south_node->type == ZPL_ADT_TYPE_STRING) {
            if (pawn->south_path) {
              free(pawn->south_path);
            }
            pawn->south_path = memcpy(
                malloc(strlen(south_node->string) + 1),
                south_node->string, strlen(south_node->string) + 1);
          }
          if (east_node && east_node->type == ZPL_ADT_TYPE_STRING) {
            if (pawn->east_path) {
              free(pawn->east_path);
            }
            pawn->east_path = memcpy(
                malloc(strlen(east_node->string) + 1),
                east_node->string, strlen(east_node->string) + 1);
          }
        }
      }
    }
  }

  free(content);
  zpl_json_free(&recipes);
  return true;
}

void G_Add_Recipes_QC(qcvm_t *qcvm) {
  game_t *game = qcvm_get_user_data(qcvm);

  const char *path = qcvm_get_parm_string(qcvm, 0);
  bool required = qcvm_get_parm_int(qcvm, 1);

  qcvm_return_int(qcvm, (int)G_Add_Recipes(game, path, required));
}

unsigned G_Item_AddAmount(game_t *game, unsigned map, unsigned x, unsigned y,
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
      unsigned can_place =
          (max_stack_size - the_cpu_tile->stack_amounts[s_idx]);

      if (can_place >= remaining) {
        the_cpu_tile->stack_amounts[s_idx] += remaining;
        remaining = 0;
      } else {
        the_cpu_tile->stack_amounts[s_idx] += can_place;
        remaining -= can_place;
      }
    }

    s_idx++;
  }

  return remaining;
}

void G_Item_AddAmount_QC(qcvm_t *qcvm) {
  game_t *game = qcvm_get_user_data(qcvm);

  int map = qcvm_get_parm_int(qcvm, 0);
  float x = qcvm_get_parm_float(qcvm, 1);
  float y = qcvm_get_parm_float(qcvm, 2);
  const char *recipe = qcvm_get_parm_string(qcvm, 3);
  float amount = qcvm_get_parm_float(qcvm, 4);

  if (map < 0 || map >= (int)game->map_count) {
    printf(
        "[ERROR] Assertion G_Item_AddAmount_QC(map >= 0 || map < game->map_count) "
        "[map = %d, map_count = %d] should be "
        "verified.\n",
        map, game->map_count);
    return;
  }

  map_t *the_map = &game->maps[map];

  if (x < 0.0f || x >= the_map->w) {
    printf("[ERROR] Assertion G_Item_AddAmount_QC(x >= 0 || x < "
           "map->w) "
           "[x = %d, map->w = %d] should be "
           "verified.\n",
           (unsigned)x, the_map->w);
    return;
  }

  if (y < 0.0f || y >= the_map->h) {
    printf("[ERROR] Assertion G_Item_AddAmount_QC(y >= 0 || y < "
           "map->h) "
           "[y = %d, map->h = %d] should be "
           "verified.\n",
           (unsigned)x, the_map->h);
    return;
  }

  zpl_u64 key = zpl_fnv64(recipe, strlen(recipe));
  material_t *the_item = G_Materials_get(&game->material_bank, key);

  if (!the_item) {
    printf("[ERROR] Assertion G_Item_AddAmount_QC(recipe exists) [recipe = \"%s\"] "
           "should be verified.\n",
           recipe);
    return;
  }

  if (amount <= 0.0f) {
    printf("[ERROR] Assertion G_Item_AddAmount_QC(amount is positive) [amount = "
           "%.03f] "
           "should be verified.\n",
           amount);
    return;
  }

  float placed = G_Item_AddAmount(game, map, x, y, the_item, (unsigned)amount);

  qcvm_return_float(qcvm, placed);
}

void G_UpdateTile(game_t *game, map_t *map, unsigned idx) {
  cpu_tile_t *cpu_tile = &map->cpu_tiles[idx];
  struct Tile *gpu_tile = &map->gpu_tiles[idx];

  unsigned actual_stack_count = 0;
  for (unsigned i = 0; i < 3; i++) {
    if (cpu_tile->stack_amounts[i] == 0) {
      cpu_tile->stack_recipes[i] = NULL;
    } else {
      actual_stack_count++;
    }
  }

  cpu_tile->stack_count = actual_stack_count;

  unsigned current_stack = 0;
  for (unsigned i = 0; i < 3; i++) {
    if (cpu_tile->stack_recipes[i]) {
      material_t *tmp_mat = cpu_tile->stack_recipes[i];
      unsigned tmp_amount = cpu_tile->stack_amounts[i];
      cpu_tile->stack_recipes[i] = NULL;
      cpu_tile->stack_amounts[i] = 0;
      cpu_tile->stack_recipes[current_stack] = tmp_mat;
      cpu_tile->stack_amounts[current_stack] = tmp_amount;
      current_stack++;
    }
  }

  gpu_tile->stack_count = actual_stack_count;
  gpu_tile->stack_textures[0] = (cpu_tile->stack_recipes[0] ? cpu_tile->stack_recipes[0]->full_stack_tex : 0);
  gpu_tile->stack_textures[1] = (cpu_tile->stack_recipes[1] ? cpu_tile->stack_recipes[1]->full_stack_tex : 0);
  gpu_tile->stack_textures[2] = (cpu_tile->stack_recipes[2] ? cpu_tile->stack_recipes[2]->full_stack_tex : 0);
}

void G_Item_RemoveAmount_QC(qcvm_t *qcvm) {
  game_t *game = qcvm_get_user_data(qcvm);

  int map = qcvm_get_parm_int(qcvm, 0);
  float x = qcvm_get_parm_float(qcvm, 1);
  float y = qcvm_get_parm_float(qcvm, 2);
  const char *recipe = qcvm_get_parm_string(qcvm, 3);
  float amount = qcvm_get_parm_float(qcvm, 4);

  if (map < 0 || map >= (int)game->map_count) {
    printf(
        "[ERROR] Assertion G_Item_RemoveAmount_QC(map >= 0 || map < game->map_count) "
        "[map = %d, map_count = %d] should be "
        "verified.\n",
        map, game->map_count);
    return;
  }

  map_t *the_map = &game->maps[map];

  if (x < 0.0f || x >= the_map->w) {
    printf("[ERROR] Assertion G_Item_RemoveAmount_QC(x >= 0 || x < "
           "map->w) "
           "[x = %d, map->w = %d] should be "
           "verified.\n",
           (unsigned)x, the_map->w);
    return;
  }

  if (y < 0.0f || y >= the_map->h) {
    printf("[ERROR] Assertion G_Item_RemoveAmount_QC(y >= 0 || y < "
           "map->h) "
           "[y = %d, map->h = %d] should be "
           "verified.\n",
           (unsigned)x, the_map->h);
    return;
  }

  zpl_u64 key = zpl_fnv64(recipe, strlen(recipe));
  material_t *the_item = G_Materials_get(&game->material_bank, key);

  if (!the_item) {
    printf("[ERROR] Assertion G_Item_RemoveAmount_QC(recipe exists) [recipe = \"%s\"] "
           "should be verified.\n",
           recipe);
    return;
  }

  if (amount <= 0.0f) {
    printf("[ERROR] Assertion G_Item_RemoveAmount_QC(amount is positive) [amount = "
           "%.03f] "
           "should be verified.\n",
           amount);
    return;
  }

  unsigned idx = y * the_map->w + x;

  cpu_tile_t *the_tile = &the_map->cpu_tiles[idx];

  for (unsigned i = 0; i < 3; i++) {
    if (!the_tile->stack_recipes[i]) {
      continue;
    }
    if (the_tile->stack_recipes[i]->key == the_item->key) {
      if (the_tile->stack_amounts[i] >= amount) {
        the_tile->stack_amounts[i] -= amount;
        break;
      } else {
        amount -= the_tile->stack_amounts[i];
        the_tile->stack_amounts[i] = 0.0f;
      }
    }
  }

  G_UpdateTile(game, the_map, idx);
}

void G_Item_GetAmount_QC(qcvm_t *qcvm) {
  game_t *game = qcvm_get_user_data(qcvm);

  int map = qcvm_get_parm_int(qcvm, 0);
  float x = qcvm_get_parm_float(qcvm, 1);
  float y = qcvm_get_parm_float(qcvm, 2);
  const char *recipe = qcvm_get_parm_string(qcvm, 3);

  if (map < 0 || map >= (int)game->map_count) {
    printf(
        "[ERROR] Assertion G_Item_GetAmount_QC(map >= 0 || map < game->map_count) "
        "[map = %d, map_count = %d] should be "
        "verified.\n",
        map, game->map_count);
    return;
  }

  map_t *the_map = &game->maps[map];

  if (x < 0.0f || x >= the_map->w) {
    printf("[ERROR] Assertion G_Item_GetAmount_QC(x >= 0 || x < "
           "map->w) "
           "[x = %d, map->w = %d] should be "
           "verified.\n",
           (unsigned)x, the_map->w);
    return;
  }

  if (y < 0.0f || y >= the_map->h) {
    printf("[ERROR] Assertion G_Item_GetAmount_QC(y >= 0 || y < "
           "map->h) "
           "[y = %d, map->h = %d] should be "
           "verified.\n",
           (unsigned)x, the_map->h);
    return;
  }

  zpl_u64 key = zpl_fnv64(recipe, strlen(recipe));
  material_t *the_item = G_Materials_get(&game->material_bank, key);

  if (!the_item) {
    printf("[ERROR] Assertion G_Item_GetAmount_QC(recipe exists) [recipe = \"%s\"] "
           "should be verified.\n",
           recipe);
    return;
  }

  unsigned idx = y * the_map->w;

  cpu_tile_t *the_tile = &the_map->cpu_tiles[idx];

  for (unsigned i = 0; i < 3; i++) {
    if (!the_tile->stack_recipes[i]) {
      continue;
    }
    if (the_tile->stack_recipes[i]->key == the_item->key) {
      qcvm_return_float(qcvm, the_tile->stack_amounts[i]);
      break;
    }
  }

  printf("no stuff on this tile mate %s\n", recipe);
  qcvm_return_float(qcvm, 0.0f);
}

bool G_Item_FindNearest(unsigned map, unsigned x, unsigned y,
                        const char *recipe, unsigned start_search) {
  return false;
}

int G_SortFoundStack(const void *a, const void *b) {
  typedef struct found_stack_t {
    float distance;
    vec2 pos;
    float amount;
  } found_stack_t;

  const found_stack_t *tmp_a = a;
  const found_stack_t *tmp_b = b;

  if (tmp_a->distance > tmp_b->distance) {
    return 1;
  }
  return -1;
}

void G_Item_FindNearest_QC(qcvm_t *qcvm) {
  game_t *game = qcvm_get_user_data(qcvm);

  int map = qcvm_get_parm_int(qcvm, 0);
  float x = qcvm_get_parm_float(qcvm, 1);
  float y = qcvm_get_parm_float(qcvm, 2);
  const char *recipe = qcvm_get_parm_string(qcvm, 3);
  int start_search = (int)qcvm_get_parm_float(qcvm, 4);

  if (map < 0 || map >= (int)game->map_count) {
    printf("[ERROR] Assertion G_Item_FindNearest_QC(map >= 0 || map < "
           "game->map_count) "
           "[map = %d, map_count = %d] should be "
           "verified.\n",
           map, game->map_count);
    return;
  }

  map_t *the_map = &game->maps[map];

  if (x < 0.0f || x >= the_map->w) {
    printf("[ERROR] Assertion G_Item_FindNearest_QC(x >= 0 || x < "
           "map->w) "
           "[x = %d, map->w = %d] should be "
           "verified.\n",
           (unsigned)x, the_map->w);
    return;
  }

  if (y < 0.0f || y >= the_map->h) {
    printf("[ERROR] Assertion G_Item_FindNearest_QC(y >= 0 || y < "
           "map->y) "
           "[y = %d, map->h = %d] should be "
           "verified.\n",
           (unsigned)y, the_map->h);
    return;
  }

  zpl_u64 key = zpl_fnv64(recipe, strlen(recipe));
  material_t *the_material = G_Materials_get(&game->material_bank, key);

  if (!the_material) {
    printf("[ERROR] Assertion G_Item_FindNearest_QC(recipe exists) [recipe = "
           "\"%s\"] "
           "should be verified.\n",
           recipe);
    return;
  }

  // TODO: this part needs a major rework, but it's unlikely that
  // a player can have more than 64 stacks of the same material
  typedef struct found_stack_t {
    float distance;
    vec2 pos;
    float amount;
  } found_stack_t;
  found_stack_t found_stacks[64];
  unsigned found_stack_count = 0;

  for (unsigned xx = 0; xx < the_map->w; xx++) {
    for (unsigned yy = 0; yy < the_map->h; yy++) {
      unsigned idx = yy * the_map->w + xx;
      for (unsigned ii = 0; ii < 3; ii++) {
        if (!the_map->cpu_tiles[idx].stack_recipes[ii]) {
          continue;
        }
        if (the_map->cpu_tiles[idx].stack_recipes[ii]->key == the_material->key) {
          float distance = sqrtf(
              ((float)xx - x) * ((float)xx - x) + ((float)yy - y) * ((float)yy - y));

          found_stacks[found_stack_count].amount = the_map->cpu_tiles[idx].stack_amounts[ii];
          found_stacks[found_stack_count].distance = distance;
          found_stacks[found_stack_count].pos[0] = xx;
          found_stacks[found_stack_count].pos[1] = yy;

          found_stack_count++;
        }
      }
    }
  }

  qsort(&found_stacks, found_stack_count, sizeof(found_stack_t), G_SortFoundStack);

  if (found_stack_count == 0) {
    qcvm_return_vector(qcvm, 0.0f, 0.0f, -1.0f);
  } else {
    qcvm_return_vector(qcvm, found_stacks[start_search].pos[0], found_stacks[start_search].pos[1], found_stacks[start_search].amount);
  }
}

void G_NeutralAnimal_Add_QC(qcvm_t *qcvm) {
  game_t *game = qcvm_get_user_data(qcvm);

  int map = qcvm_get_parm_int(qcvm, 0);
  float x = qcvm_get_parm_float(qcvm, 1);
  float y = qcvm_get_parm_float(qcvm, 2);
  const char *recipe = qcvm_get_parm_string(qcvm, 3);

  if (map < 0 || map >= (int)game->map_count) {
    printf("[ERROR] Assertion G_NeutralAnimal_Add(map >= 0 || map < "
           "game->map_count) "
           "[map = %d, map_count = %d] should be "
           "verified.\n",
           map, game->map_count);
    return;
  }

  map_t *the_map = &game->maps[map];

  if (x < 0.0f || x >= the_map->w) {
    printf("[ERROR] Assertion G_NeutralAnimal_Add(x >= 0 || x < "
           "map->w) "
           "[x = %d, map->w = %d] should be "
           "verified.\n",
           (unsigned)x, the_map->w);
    return;
  }

  if (y < 0.0f || y >= the_map->h) {
    printf("[ERROR] Assertion G_NeutralAnimal_Add(y >= 0 || y < "
           "map->y) "
           "[y = %d, map->h = %d] should be "
           "verified.\n",
           (unsigned)y, the_map->h);
    return;
  }

  zpl_u64 key = zpl_fnv64(recipe, strlen(recipe));
  pawn_t *the_pawn = G_Pawns_get(&game->pawn_bank, key);

  if (!the_pawn) {
    printf("[ERROR] Assertion G_NeutralAnimal_Add(recipe exists) [recipe = "
           "\"%s\"] "
           "should be verified.\n",
           recipe);
    return;
  }

  struct Transform transform = {
      .position = {x, y},
      .scale = {1.0f, 1.0f},
  };

  struct Sprite sprite = {
      .current = the_pawn->east_tex,
      .texture_east = the_pawn->east_tex,
      .texture_south = the_pawn->south_tex,
      .texture_north = the_pawn->north_tex,
  };

  G_AddPawn(game, &transform, &sprite, AGENT_ANIMAL);
}

void G_Colonist_Add_QC(qcvm_t *qcvm) {
  game_t *game = qcvm_get_user_data(qcvm);

  int map = qcvm_get_parm_int(qcvm, 0);
  float x = qcvm_get_parm_float(qcvm, 1);
  float y = qcvm_get_parm_float(qcvm, 2);
  int faction = qcvm_get_parm_int(qcvm, 3);
  const char *recipe = qcvm_get_parm_string(qcvm, 4);

  if (map < 0 || map >= (int)game->map_count) {
    printf("[ERROR] Assertion G_Colonist_Add_QC(map >= 0 || map < "
           "game->map_count) "
           "[map = %d, map_count = %d] should be "
           "verified.\n",
           map, game->map_count);
    return;
  }

  map_t *the_map = &game->maps[map];

  if (x < 0.0f || x >= the_map->w) {
    printf("[ERROR] Assertion G_Colonist_Add_QC(x >= 0 || x < "
           "map->w) "
           "[x = %d, map->w = %d] should be "
           "verified.\n",
           (unsigned)x, the_map->w);
    return;
  }

  if (y < 0.0f || y >= the_map->h) {
    printf("[ERROR] Assertion G_Colonist_Add_QC(y >= 0 || y < "
           "map->y) "
           "[y = %d, map->h = %d] should be "
           "verified.\n",
           (unsigned)y, the_map->h);
    return;
  }

  zpl_u64 key = zpl_fnv64(recipe, strlen(recipe));
  pawn_t *the_pawn = G_Pawns_get(&game->pawn_bank, key);

  if (!the_pawn) {
    printf("[ERROR] Assertion G_Colonist_Add_QC(recipe exists) [recipe = "
           "\"%s\"] "
           "should be verified.\n",
           recipe);
    return;
  }

  struct Transform transform = {
      .position = {x, y},
      .scale = {1.0f, 1.0f},
  };

  struct Sprite sprite = {
      .current = the_pawn->east_tex,
      .texture_east = the_pawn->east_tex,
      .texture_south = the_pawn->south_tex,
      .texture_north = the_pawn->north_tex,
  };

  G_AddPawn(game, &transform, &sprite, faction);
}

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

void G_Entity_Goto_QC(qcvm_t *qcvm) {
  game_t *game = qcvm_get_user_data(qcvm);
  int entity = qcvm_get_parm_int(qcvm, 0);

  float x = qcvm_get_parm_float(qcvm, 1);
  float y = qcvm_get_parm_float(qcvm, 2);

  game->cpu_agents[entity].state = AGENT_PATH_FINDING;
  game->cpu_agents[entity].target[0] = roundf(x);
  game->cpu_agents[entity].target[1] = roundf(y);
}

void G_Draw_Image_Relative_QC(qcvm_t *qcvm) {
  const char *path = qcvm_get_parm_string(qcvm, 0);
  float w = qcvm_get_parm_float(qcvm, 1);
  float h = qcvm_get_parm_float(qcvm, 2);
  float x = qcvm_get_parm_float(qcvm, 3);
  float y = qcvm_get_parm_float(qcvm, 4);
  float z = qcvm_get_parm_float(qcvm, 5);

  game_t *game = qcvm_get_user_data(qcvm);

  G_Draw_Image_Relative(game, path, w, h, x, y, z);
}

// TODO: G_Prepare_Scene have to be called before calling G_Run_Scene.
// G_Run_Scene only set the current_scene to be the specified scene. It allows
// calling an running the start listener while the previous scene still run (for
// example, to let the loading animation be performed).
void G_Run_Scene(game_t *game, const char *scene_name) {
  printf("G_Run_Scene(\"%s\") TODO: should make sure this is call from the main thread;\n", scene_name);

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
      qcvm_run(game->qcvms[0], game->current_scene->end_listeners[j].qcvm_func);
    }
  }

  // The specified scene become the current scene
  game->current_scene = scene;

  // Then call all start listeners in order
  for (unsigned j = 0; j < game->current_scene->start_listener_count; j++) {
    qcvm_run(game->qcvms[0], game->current_scene->start_listeners[j].qcvm_func);
  }
}

void G_Run_Scene_QC(qcvm_t *qcvm) {
  game_t *game = qcvm_get_user_data(qcvm);

  const char *scene_name = qcvm_get_parm_string(qcvm, 0);

  G_Run_Scene(game, scene_name);
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

    if (the_tile->stack_count == 0) {
      continue;
    } else if (the_tile->stack_count == 1) {
      offsets = offsets_1;
      scale = 1.2;
    } else if (the_tile->stack_count == 2) {
      offsets = offsets_2;
      scale = 1.5;
    } else if (the_tile->stack_count == 3) {
      offsets = offsets_3;
    } else {
      printf("[ERROR] G_WorkerSetupTileText assumes there is at most 3 stacks per tile. Stack count is %d on (%d,%d).\n", the_tile->stack_count, col, the_job->row);
    }

    for (unsigned i = 0; i < the_tile->stack_count; i++) {
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

void G_Load_Game(game_t *game) {
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
                 zpl_array_count(game->terrain_bank.entries) * 3 +
                 zpl_array_count(game->pawn_bank.entries) * 3,
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
    zpl_jobs_enqueue(&game->job_sys, G_WorkerLoadTexture, left_right_bottom_job);
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

  offset += zpl_array_count(game->wall_bank.entries) * 16;

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

  offset += zpl_array_count(game->terrain_bank.entries) * 3;

  for (unsigned i = 0; i < zpl_array_count(game->pawn_bank.entries); i++) {
    pawn_bank_tEntry *entry = &game->pawn_bank.entries[i];
    pawn_t *pawn = &entry->value;

    texture_job_t *north_job = &texture_jobs[offset + i * 3 + 0];
    *north_job = (texture_job_t){
        .game = game,
        .dest_text = &pawn->north_tex,
        .path = pawn->north_path,
    };
    texture_job_t *south_job = &texture_jobs[offset + i * 3 + 1];
    *south_job = (texture_job_t){
        .game = game,
        .dest_text = &pawn->south_tex,
        .path = pawn->south_path,
    };
    texture_job_t *east_job = &texture_jobs[offset + i * 3 + 2];
    *east_job = (texture_job_t){
        .game = game,
        .dest_text = &pawn->east_tex,
        .path = pawn->east_path,
    };

    zpl_jobs_enqueue(&game->job_sys, G_WorkerLoadTexture, north_job);
    zpl_jobs_enqueue(&game->job_sys, G_WorkerLoadTexture, south_job);
    zpl_jobs_enqueue(&game->job_sys, G_WorkerLoadTexture, east_job);
  }

  while (!zpl_jobs_done(&game->job_sys)) {
    zpl_jobs_process(&game->job_sys);

    game_state_t *state = G_TickGame(game->client, game);

    CL_DrawClient(game->client, game, state);
  }

  printf("[VERBOSE] Loading game took `%f` ms\n",
         (float)(zpl_time_rel() - now));

  VK_UploadMapTextures(game->rend, game->map_textures, game->map_texture_count);
  VK_UploadFontTextures(game->rend, game->font_textures, game->font_texture_count);

  G_Run_Scene(game, game->next_scene);

  free(game->next_scene);
  game->next_scene = NULL;

  free(texture_jobs);
}

void G_Load_Game_QC(qcvm_t *qcvm) {
  game_t *game = qcvm_get_user_data(qcvm);

  const char *loading_scene_name = qcvm_get_parm_string(qcvm, 0);
  const char *next_scene = qcvm_get_parm_string(qcvm, 1);

  game->next_scene = memcpy(malloc(strlen(next_scene) + 1), next_scene,
                            strlen(next_scene) + 1);

  G_Run_Scene(game, loading_scene_name);

  G_Load_Game(game);
}

const char *G_Get_Last_Asset_Loaded(game_t *game) {
  return "not yet implemented!";
}

void G_Get_Last_Asset_Loaded_QC(qcvm_t *qcvm) {
  game_t *game = qcvm_get_user_data(qcvm);

  qcvm_return_string(qcvm, G_Get_Last_Asset_Loaded(game));
}

void G_Add_Wall_QC(qcvm_t *qcvm) {
  game_t *game = qcvm_get_user_data(qcvm);
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

  game_t *game = qcvm_get_user_data(qcvm);

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
  case G_THINK_UPDATE: {
    for (unsigned s = 0; s < game->scene_count; s++) {
      scene_t *the_scene = &game->scenes[s];

      if (the_scene->agent_think_listener_count == 16) {
        printf("[ERROR] Reached max number of `G_CAMERA_UPDATE` for the scene "
               "`%s`.\n",
               the_scene->name);

        return;
      }
      the_scene
          ->agent_think_listeners[the_scene->agent_think_listener_count]
          .qcvm_func = func;
      the_scene->agent_think_listener_count++;
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

  G_Add_Listener(qcvm_get_user_data(qcvm), listener_type, attachment, func_id);
}

void G_Get_Camera_Position_QC(qcvm_t *qcvm) {
  game_t *game = qcvm_get_user_data(qcvm);
  qcvm_return_vector(qcvm, game->state.fps.pos[0], game->state.fps.pos[1],
                     game->state.fps.pos[2]);
}

void G_Set_Camera_Position_QC(qcvm_t *qcvm) {
  game_t *game = qcvm_get_user_data(qcvm);

  qcvm_vec3_t vec = qcvm_get_parm_vector(qcvm, 0);

  game->state.fps.pos[0] = vec.x;
  game->state.fps.pos[1] = vec.y;
  game->state.fps.pos[2] = vec.z;
}

void G_Get_Camera_Zoom_QC(qcvm_t *qcvm) {
  game_t *game = qcvm_get_user_data(qcvm);

  qcvm_return_float(qcvm, game->state.fps.zoom);
}

void G_Set_Camera_Zoom_QC(qcvm_t *qcvm) {
  game_t *game = qcvm_get_user_data(qcvm);

  game->state.fps.zoom = qcvm_get_parm_float(qcvm, 0);

  // printf("game->state.fps.zoom = %f\n", game->state.fps.zoom);
}

void G_Get_Axis_QC(qcvm_t *qcvm) {
  game_t *game = qcvm_get_user_data(qcvm);
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
  game_t *game = qcvm_get_user_data(qcvm);
  qcvm_return_vector(qcvm, CL_GetInput(game->client)->mouse_x,
                     CL_GetInput(game->client)->mouse_y, 0.0f);
}

void G_Get_Screen_Size_QC(qcvm_t *qcvm) {
  game_t *game = qcvm_get_user_data(qcvm);
  unsigned w, h;
  CL_GetViewDim(game->client, &w, &h);
  qcvm_return_vector(qcvm, (float)w, (float)h, 0.0f);
}

void G_Entity_GetPosition_QC(qcvm_t *qcvm) {
  game_t *game = qcvm_get_user_data(qcvm);

  unsigned entity = qcvm_get_parm_int(qcvm, 0);

  qcvm_return_vector(qcvm, game->transforms[entity].position[0], game->transforms[entity].position[1], 0.0f);
}

void G_Entity_GetInventoryAmount_QC(qcvm_t *qcvm) {
  game_t *game = qcvm_get_user_data(qcvm);

  unsigned entity = qcvm_get_parm_int(qcvm, 0);
  const char *recipe = qcvm_get_parm_string(qcvm, 1);

  if (game->cpu_agents[entity].inventory_initialized) {
    zpl_u64 key = zpl_fnv64(recipe, strlen(recipe));
    float *amount = G_Inventory_get(&game->cpu_agents[entity].inventory, key);

    if (!amount) {
      qcvm_return_float(qcvm, 0.0f);
    } else {
      qcvm_return_float(qcvm, *amount);
    }
  } else {
    qcvm_return_float(qcvm, 0.0f);
  }
}

void G_Entity_RemoveInventoryAmount_QC(qcvm_t *qcvm) {
  game_t *game = qcvm_get_user_data(qcvm);

  unsigned entity = qcvm_get_parm_int(qcvm, 0);
  const char *recipe = qcvm_get_parm_string(qcvm, 1);
  float amount = qcvm_get_parm_float(qcvm, 2);

  if (game->cpu_agents[entity].inventory_initialized) {
    zpl_u64 key = zpl_fnv64(recipe, strlen(recipe));
    float *current_amount = G_Inventory_get(&game->cpu_agents[entity].inventory, key);

    if (amount) {
      (*current_amount) -= glm_min(*current_amount, amount);
    }
  }
}

void G_Entity_AddInventoryAmount_QC(qcvm_t *qcvm) {
  game_t *game = qcvm_get_user_data(qcvm);

  unsigned entity = qcvm_get_parm_int(qcvm, 0);
  const char *recipe = qcvm_get_parm_string(qcvm, 1);
  float amount = qcvm_get_parm_float(qcvm, 2);

  zpl_u64 key = zpl_fnv64(recipe, strlen(recipe));

  if (!game->cpu_agents[entity].inventory_initialized) {
    G_Inventory_init(&game->cpu_agents[entity].inventory, zpl_heap_allocator());
    game->cpu_agents[entity].inventory_initialized = true;
  }

  float *current_amount = G_Inventory_get(&game->cpu_agents[entity].inventory, key);

  if (!current_amount) {
    G_Inventory_set(&game->cpu_agents[entity].inventory, key, 0.0f);
    current_amount = G_Inventory_get(&game->cpu_agents[entity].inventory, key);
  }

  (*current_amount) += amount;
}

void G_QCVMInstall(qcvm_t *qcvm) {
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

  qcvm_export_t export_G_Item_AddAmount = {
      .func = G_Item_AddAmount_QC,
      .name = "G_Item_AddAmount",
      .argc = 5,
      .args[0] = {.name = "map", .type = QCVM_INT},
      .args[1] = {.name = "x", .type = QCVM_FLOAT},
      .args[2] = {.name = "y", .type = QCVM_FLOAT},
      .args[3] = {.name = "recipe", .type = QCVM_STRING},
      .args[4] = {.name = "amount", .type = QCVM_FLOAT},
      .type = QCVM_VOID,
  };

  qcvm_export_t export_G_Item_RemoveAmount = {
      .func = G_Item_RemoveAmount_QC,
      .name = "G_Item_RemoveAmount",
      .argc = 5,
      .args[0] = {.name = "map", .type = QCVM_INT},
      .args[1] = {.name = "x", .type = QCVM_FLOAT},
      .args[2] = {.name = "y", .type = QCVM_FLOAT},
      .args[3] = {.name = "recipe", .type = QCVM_STRING},
      .args[4] = {.name = "amount", .type = QCVM_FLOAT},
      .type = QCVM_VOID,
  };

  qcvm_export_t export_G_Item_GetAmount = {
      .func = G_Item_GetAmount_QC,
      .name = "G_Item_GetAmount",
      .argc = 4,
      .args[0] = {.name = "map", .type = QCVM_INT},
      .args[1] = {.name = "x", .type = QCVM_FLOAT},
      .args[2] = {.name = "y", .type = QCVM_FLOAT},
      .args[3] = {.name = "recipe", .type = QCVM_STRING},
      .type = QCVM_FLOAT,
  };

  qcvm_export_t export_G_Item_FindNearest = {
      .func = G_Item_FindNearest_QC,
      .name = "G_Item_FindNearest",
      .argc = 5,
      .args[0] = {.name = "map", .type = QCVM_INT},
      .args[1] = {.name = "org_x", .type = QCVM_FLOAT},
      .args[2] = {.name = "org_y", .type = QCVM_FLOAT},
      .args[3] = {.name = "recipe", .type = QCVM_STRING},
      .args[4] = {.name = "start_search", .type = QCVM_INT},
      .type = QCVM_VECTOR,
  };

  qcvm_export_t export_G_NeutralAnimal_Add = {
      .func = G_NeutralAnimal_Add_QC,
      .name = "G_NeutralAnimal_Add",
      .argc = 4,
      .args[0] = {.name = "map", .type = QCVM_INT},
      .args[1] = {.name = "x", .type = QCVM_FLOAT},
      .args[2] = {.name = "y", .type = QCVM_FLOAT},
      .args[3] = {.name = "recipe", .type = QCVM_STRING},
      .type = QCVM_ENTITY,
  };

  qcvm_export_t export_G_Colonist_Add = {
      .func = G_Colonist_Add_QC,
      .name = "G_Colonist_Add",
      .argc = 4,
      .args[0] = {.name = "map", .type = QCVM_INT},
      .args[1] = {.name = "x", .type = QCVM_FLOAT},
      .args[2] = {.name = "y", .type = QCVM_FLOAT},
      .args[3] = {.name = "faction", .type = QCVM_INT},
      .args[4] = {.name = "recipe", .type = QCVM_STRING},
      .type = QCVM_ENTITY,
  };

  qcvm_export_t export_G_Entity_Goto = {
      .func = G_Entity_Goto_QC,
      .name = "G_Entity_Goto",
      .argc = 3,
      .args[0] = {.name = "entity", .type = QCVM_INT},
      .args[1] = {.name = "x", .type = QCVM_FLOAT},
      .args[2] = {.name = "y", .type = QCVM_FLOAT},
  };

  qcvm_export_t export_G_Entity_GetPosition = {
      .func = G_Entity_GetPosition_QC,
      .name = "G_Entity_GetPosition",
      .argc = 1,
      .args[0] = {.name = "entity", .type = QCVM_INT},
      .type = QCVM_VECTOR,
  };

  qcvm_export_t export_G_Entity_GetInventoryAmount = {
      .func = G_Entity_GetInventoryAmount_QC,
      .name = "G_Entity_GetInventoryAmount",
      .argc = 2,
      .args[0] = {.name = "entity", .type = QCVM_INT},
      .args[1] = {.name = "recipe", .type = QCVM_STRING},
      .type = QCVM_FLOAT,
  };

  qcvm_export_t export_G_Entity_RemoveInventoryAmount = {
      .func = G_Entity_RemoveInventoryAmount_QC,
      .name = "G_Entity_RemoveInventoryAmount",
      .argc = 3,
      .args[0] = {.name = "entity", .type = QCVM_INT},
      .args[1] = {.name = "recipe", .type = QCVM_STRING},
      .args[2] = {.name = "amount", .type = QCVM_FLOAT},
  };

  qcvm_export_t export_G_Entity_AddInventoryAmount_QC = {
      .func = G_Entity_AddInventoryAmount_QC,
      .name = "G_Entity_AddInventoryAmount",
      .argc = 3,
      .args[0] = {.name = "entity", .type = QCVM_INT},
      .args[1] = {.name = "recipe", .type = QCVM_STRING},
      .args[2] = {.name = "amount", .type = QCVM_FLOAT},
  };

  qcvm_add_export(qcvm, &export_G_Add_Recipes);
  qcvm_add_export(qcvm, &export_G_Load_Game);
  qcvm_add_export(qcvm, &export_G_Get_Last_Asset_Loaded);
  qcvm_add_export(qcvm, &export_G_Add_Scene);
  qcvm_add_export(qcvm, &export_G_Run_Scene);
  qcvm_add_export(qcvm, &export_G_Create_Map);
  qcvm_add_export(qcvm, &export_G_Add_Wall);
  qcvm_add_export(qcvm, &export_G_Add_Listener);
  qcvm_add_export(qcvm, &export_G_Draw_Image_Relative);
  qcvm_add_export(qcvm, &export_G_Set_Current_Map);
  qcvm_add_export(qcvm, &export_G_Get_Camera_Position);
  qcvm_add_export(qcvm, &export_G_Set_Camera_Position);
  qcvm_add_export(qcvm, &export_G_Get_Camera_Zoom);
  qcvm_add_export(qcvm, &export_G_Set_Camera_Zoom);
  qcvm_add_export(qcvm, &export_G_Get_Axis);
  qcvm_add_export(qcvm, &export_G_Get_Mouse_Position);
  qcvm_add_export(qcvm, &export_G_Get_Screen_Size);
  qcvm_add_export(qcvm, &export_G_Item_AddAmount);
  qcvm_add_export(qcvm, &export_G_Item_RemoveAmount);
  qcvm_add_export(qcvm, &export_G_Item_GetAmount);
  qcvm_add_export(qcvm, &export_G_Item_FindNearest);
  qcvm_add_export(qcvm, &export_G_NeutralAnimal_Add);
  qcvm_add_export(qcvm, &export_G_Colonist_Add);
  qcvm_add_export(qcvm, &export_G_Entity_Goto);
  qcvm_add_export(qcvm, &export_G_Entity_GetPosition);
  qcvm_add_export(qcvm, &export_G_Entity_GetInventoryAmount);
  qcvm_add_export(qcvm, &export_G_Entity_RemoveInventoryAmount);
  qcvm_add_export(qcvm, &export_G_Entity_AddInventoryAmount_QC);
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
    game->qcvms[i] = qcvm_from_memory(bytes, size);
    if (!game->qcvms[i]) {
      printf("Couldn't load `%s`. Aborting, the game isn't playable.\n",
             progs_dat);
      return false;
    }

    qclib_install(game->qcvms[i]);
    G_QCVMInstall(game->qcvms[i]);

    G_CommonInstall(game->qcvms[i]);
    G_TerrainInstall(game->qcvms[i]);

    qcvm_set_user_data(game->qcvms[i], game);
  }

  // Run the QuakeC main function on the first vm
  int main_func = qcvm_find_function(game->qcvms[0], "main");
  if (main_func < 1) {
    printf("[ERROR] No main function in `%s`.\n", progs_dat);
    return false;
  }
  qcvm_run(game->qcvms[0], main_func);

  // Get the mapped data from the renderer
  game->gpu_agents = VK_GetAgents(game->rend);
  game->transforms = VK_GetTransforms(game->rend);

  game->entities = VK_GetEntities(game->rend);

  free(bytes);

  return true;
}

void G_AddFurniture(client_t *client, game_t *game, struct Transform *transform,
                    struct Sprite *sprite, struct Immovable *immovable) {
  vk_rend_t *rend = game->rend;
  int entity =
      VK_Add_Entity(rend, transform_signature | model_transform_signature |
                              sprite_signature | immovable_signature);

  if (entity == -1) {
    return;
  }

  game->entity_count += 1;

  VK_Add_Transform(rend, entity, transform);
  VK_Add_Model_Transform(rend, entity, NULL);
  VK_Add_Sprite(rend, entity, sprite);
  VK_Add_Immovable(rend, entity, immovable);
}

void G_AddPawn(game_t *game, struct Transform *transform,
               struct Sprite *sprite, agent_type_t agent_type) {
  vk_rend_t *rend = game->rend;
  int entity =
      VK_Add_Entity(rend, transform_signature | model_transform_signature |
                              agent_signature | sprite_signature);

  if (entity == -1) {
    return;
  }

  game->entity_count += 1;

  struct Agent agent = {
      .direction =
          {
              [0] = 0.0f,
              [1] = 0.0f,
          },
  };

  cpu_agent_t cpu_agent = {
      .state = AGENT_NOTHING,
      .type = agent_type,
      .speed = 0.05,
      .target = {},
  };

  game->cpu_agents[entity] = cpu_agent;

  VK_Add_Transform(rend, entity, transform);
  VK_Add_Model_Transform(rend, entity, NULL);
  VK_Add_Agent(rend, entity, &agent);
  VK_Add_Sprite(rend, entity, sprite);
}

character_t *G_GetCharacter(game_t *game, const char *family, wchar_t c) {
  // TODO: we only compare the first letter, cause we only have two font families
  // available, sorry for that
  if (family[0] == 'c') { // "console"
    return CL_Characters_get(&game->console_character_bank, c);
  } else if (family[0] == 'g') { // "game"
    return CL_Characters_get(&game->game_character_bank, c);
  }

  return NULL;
}
