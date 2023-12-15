#pragma once

#include "client/cl_client.h"
#include "game/g_game.h"
#include "stbi_image.h"
#include "vk/vk_system.h"
#include "vk/vk_vulkan.h"

#include <cglm/cam.h>
#include <cglm/cglm.h>
#include <qcvm/qcvm.h>
#include <zpl/zpl.h>
//
#include <qclib/qclib.h>

struct map;

ZPL_TABLE_DECLARE(extern, material_bank_t, G_Materials_, material_t)
ZPL_TABLE_DECLARE(extern, texture_bank_t, G_ImmediateTextures_, texture_t)
ZPL_TABLE_DECLARE(extern, wall_bank_t, G_Walls_, wall_t)
ZPL_TABLE_DECLARE(extern, terrain_bank_t, G_Terrains_, terrain_t)

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

typedef struct texture_job_t {
  const char *path;
  unsigned *dest_text;

  game_t *game;
} texture_job_t;

typedef struct map_t {
  struct map *jps_map;
  zpl_mutex mutex;

  unsigned w;
  unsigned h;

  struct Tile *gpu_tiles;
} map_t;

typedef struct worker_t {
  // Hey hey, I heard QCVM wasn't thread-safe. So i just init a QCVM for each
  // worker thread. Hope you don't mind!
  qcvm_t *qcvm;

  unsigned id;
  game_t *game;

  map_t maps[16];
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

  zpl_mutex scene_mutex;
  int current_map;
} scene_t;

struct game_t {
  vk_system_t *model_matrix_sys;
  vk_system_t *path_finding_sys;

  cpu_agent_t *cpu_agents;
  struct Agent *gpu_agents;
  struct Transform *transforms;

  unsigned entity_count;
  unsigned *entities;

  unsigned worker_count;
  worker_t workers[32];
  zpl_jobs_system job_sys;

  char *base;

  scene_t *scenes;
  unsigned scene_count;
  unsigned scene_capacity;

  char *next_scene;
  scene_t *current_scene;

  texture_t *textures;
  unsigned texture_count;
  unsigned texture_capacity;
  zpl_mutex texture_mutex;

  zpl_mutex global_map_mutex;
  unsigned map_count;

  // Asset banks
  material_bank_t material_bank;
  wall_bank_t wall_bank;
  texture_bank_t immediate_texture_bank;
  terrain_bank_t terrain_bank;

  // Game state, reset each frame
  // The renderer use this to draw the frame
  game_state_t state;

  float last_time;
  float delta_time;

  // Hello
  vk_rend_t *rend;
  client_t *client;
};