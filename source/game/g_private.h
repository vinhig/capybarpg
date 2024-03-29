#pragma once

#include "common/c_job.h"
#include <client/cl_client.h>
#include <game/g_game.h>
#include <vk/vk_system.h>
#include <vk/vk_vulkan.h>

#include <cglm/cam.h>
#include <cglm/cglm.h>
#include <qcvm/qcvm.h>
#include <stdatomic.h>
#include <zpl/zpl.h>
//
#include <qclib/qclib.h>

#include <ft2build.h>
#include FT_FREETYPE_H

struct map;

ZPL_TABLE_DECLARE(extern, material_bank_t, G_Materials_, material_t)
ZPL_TABLE_DECLARE(extern, texture_bank_t, G_ImmediateTextures_, texture_t)
ZPL_TABLE_DECLARE(extern, wall_bank_t, G_Walls_, wall_t)
ZPL_TABLE_DECLARE(extern, image_bank_t, G_Images_, image_ui_t)
ZPL_TABLE_DECLARE(extern, terrain_bank_t, G_Terrains_, terrain_t)
ZPL_TABLE_DECLARE(extern, character_bank_t, CL_Characters_, character_t)
ZPL_TABLE_DECLARE(extern, pawn_bank_t, G_Pawns_, pawn_t)
ZPL_TABLE_DECLARE(extern, facility_bank_t, G_Facilities_, facility_t)
ZPL_TABLE_DECLARE(extern, inventory_t, G_Inventory_, float)

typedef struct cpu_path_t {
  vec2 *points;
  unsigned count;
  unsigned current;
} cpu_path_t;

typedef struct cpu_agent_t {
  vec2 target;
  float speed;
  enum agent_state_e {
    AGENT_NOTHING,
    AGENT_PATH_FINDING,
    AGENT_MOVING,
    AGENT_DRAFTED,
  } state;
  agent_type_t type;
  cpu_path_t computed_path;

  inventory_t inventory;
  bool inventory_initialized;
} __attribute__((aligned(16))) cpu_agent_t;

typedef struct cpu_tile_t {
  float health;

  wall_t *related_wall_recipe;       // may be null
  terrain_t *related_terrain_recipe; // may be null
  material_t *stack_recipes[3];      // may be null
  unsigned stack_amounts[3];
  unsigned stack_count;
} cpu_tile_t;

typedef struct node_t node_t;

#define THINK_JOB_BATCH_AGENT_SIZE 64
typedef struct think_job_t {
  unsigned agents[THINK_JOB_BATCH_AGENT_SIZE]; // a think job handles more than 1 agent to reduce contention in the job system
  unsigned agent_count;

  game_t *game;
} think_job_t;

typedef struct path_finding_job_t {
  unsigned agent;
  unsigned map;
  float delta;
  game_t *game;
} path_finding_job_t;

typedef struct item_text_job_t {
  unsigned row;

  game_t *game;
} item_text_job_t;

typedef struct font_job_t {
  const char *path;

  unsigned size;

  FT_Face *face;
  FT_Library ft;
  character_bank_t *character_bank;

  game_t *game;
} font_job_t;

typedef struct texture_job_t {
  union {
    unsigned *dest_id;
    char* label;
  };

  enum {
    __to_map_or_font,
    __to_ui,
  } type;

  game_t *game;
  const char *path;
} texture_job_t;

typedef struct map_t {
  struct map *jps_maps[16];
  zpl_mutex mutex;

  unsigned w;
  unsigned h;

  struct Tile *gpu_tiles;
  cpu_tile_t *cpu_tiles;
} map_t;

typedef enum listener_type_t {
  G_SCENE_START,
  G_SCENE_END,
  G_SCENE_UPDATE,
  G_CAMERA_UPDATE,
  G_THINK_UPDATE,
  G_LISTENER_TYPE_COUNT,
} listener_type_t;

typedef struct listener_t {
  int qcvm_func;
} listener_t;

typedef struct scene_t {
  char *name;
  unsigned id;

  zpl_mutex global_map_mutex;
  unsigned map_count;
  map_t maps[16];
  int current_map;

  listener_t start_listeners[16];
  unsigned start_listener_count;

  listener_t update_listeners[16];
  unsigned update_listener_count;

  listener_t end_listeners[16];
  unsigned end_listener_count;

  listener_t camera_update_listeners[16];
  unsigned camera_update_listener_count;

  listener_t agent_think_listeners[16];
  unsigned agent_think_listener_count;

  zpl_mutex scene_mutex;
} scene_t;

typedef struct localization_t {
  char ***translations;
  unsigned current_language;
  unsigned language_count;
  unsigned entry_count;

  zpl_arena memory;
} localization_t;

struct game_t {
  vk_system_t *model_matrix_sys;
  vk_system_t *path_finding_sys;

  cpu_agent_t *cpu_agents;
  struct Agent *gpu_agents;
  struct Transform *transforms;

  unsigned entity_count;
  unsigned *entities;

  unsigned worker_count;
  job_system_t *job_sys2;

  qcvm_t *qcvms[16];

  char *base;

  scene_t *scenes;
  unsigned scene_count;
  unsigned scene_capacity;

  // Scene change happens at the end of a frame
  char *next_scene;
  scene_t *current_scene;

  texture_t *map_textures;
  unsigned map_texture_count;
  unsigned map_texture_capacity;
  zpl_mutex map_texture_mutex;

  texture_t *ui_textures;
  unsigned ui_texture_count;
  unsigned ui_texture_capacity;
  zpl_mutex ui_texture_mutex;

  texture_t *font_textures;
  unsigned font_texture_count;
  unsigned font_texture_capacity;
  zpl_mutex font_mutex; // protecc font_textures and character_bank

  FT_Library console_ft; // two different libs, because multithreading
  FT_Library game_ft;
  FT_Face console_face;
  FT_Face game_face;

  character_bank_t console_character_bank;
  character_bank_t game_character_bank;
  unsigned char *white_space;

  // Asset banks
  material_bank_t material_bank;
  wall_bank_t wall_bank;
  texture_bank_t immediate_texture_bank;
  terrain_bank_t terrain_bank;
  pawn_bank_t pawn_bank;
  facility_bank_t facility_bank;
  image_bank_t image_bank;

  // Game state, reset each frame
  // The renderer use this to draw the frame
  game_state_t state;

  float last_time;
  float delta_time;

  // Hello
  vk_rend_t *rend;
  client_t *client;

  char current_window_id[64];

  localization_t *localization;
};

void G_CommonInstall(qcvm_t *qcvm);
void G_TerrainInstall(qcvm_t *qcvm);
void G_Add_Wall(game_t *game, int map, int x, int y, float health,
                wall_t *wall_recipe);
void G_UIInstall(qcvm_t *qcvm);
