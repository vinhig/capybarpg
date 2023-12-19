#pragma once

#include <stdbool.h>

#include "cglm/types.h"

typedef struct client_t client_t;

typedef struct game_t game_t;

struct Transform;
struct ModelTransform;
struct Sprite;
struct Agent;
struct Immovable;

typedef struct game_immediate_draw_t {
  float x, y, z;
  float w, h;
  unsigned handle;
} game_immediate_draw_t;

typedef struct game_text_draw_t {
  vec4 color;
  vec2 pos;
  vec2 size;
  unsigned tex;
} game_text_draw_t;

typedef struct game_state_t {
  // First person camera.
  // Yes, we want to speed gameplay
  struct {
    mat4 view;
    mat4 proj;
    mat4 view_proj;
    vec3 pos;
    float zoom;
  } fps;

  game_immediate_draw_t draws[64];
  unsigned draw_count;

  // This is updated by the console
  // but stored in game state
  game_text_draw_t texts[1024];
  unsigned text_count;

} game_state_t;

typedef struct material_t {
  char *name;

  unsigned revision;

  unsigned stack_size;

  unsigned low_stack_tex;
  char *low_stack_path;
  unsigned half_stack_tex;
  char *half_stack_path;
  unsigned full_stack_tex;
  char *full_stack_path;
} material_t;

typedef struct terrain_t {
  char *name;

  unsigned revision;

  unsigned variant1_tex;
  char *variant1_path;

  unsigned variant2_tex;
  char *variant2_path;

  unsigned variant3_tex;
  char *variant3_path;
} terrain_t;

typedef struct wall_t {
  char *name;

  unsigned revision;

  unsigned health;

  unsigned only_left_tex;
  char *only_left_path;
  unsigned only_right_tex;
  char *only_right_path;
  unsigned only_top_tex;
  char *only_top_path;
  unsigned only_bottom_tex;
  char *only_bottom_path;
  unsigned all_tex;
  char *all_path;
  unsigned left_right_bottom_tex;
  char *left_right_bottom_path;
  unsigned left_right_tex;
  char *left_right_path;
  unsigned left_right_top_tex;
  char *left_right_top_path;
  unsigned top_bottom_tex;
  char *top_bottom_path;
  unsigned right_top_tex;
  char *right_top_path;
  unsigned left_top_tex;
  char *left_top_path;
  unsigned right_bottom_tex;
  char *right_bottom_path;
  unsigned left_bottom_tex;
  char *left_bottom_path;
  unsigned left_top_bottom_tex;
  char *left_top_bottom_path;
  unsigned right_top_bottom_tex;
  char *right_top_bottom_path;
  unsigned nothing_tex;
  char *nothing_path;
} wall_t;

/// @brief Create a new game, allocating the memory for it. Read `main.toml`
/// from the given base folder, and set it as the current scene. No assets
/// loading occurs.
/// @param base Base folder to fetch all assets from.
/// @return
game_t *G_CreateGame(client_t *client, char *base);

bool G_LoadCurrentWorld(client_t *client, game_t *game);

/// @brief Helper function to add a Pawn to the world (an entity with a
/// Transform, Model Transform, Sprite and Agent components).
void G_AddPawn(client_t *client, game_t *game, struct Transform *transform,
               struct Sprite *sprite);

/// @brief Helper function to add a Furniture to the world (an entity with a
/// Transform, Model Transform, and Sprite components). Register it to the list
/// of usable material if applicable.
void G_AddFurniture(client_t *client, game_t *game, struct Transform *transform,
                    struct Sprite *sprite, struct Immovable *immovable);

game_state_t G_TickGame(client_t *client, game_t *game);

void G_DestroyGame(game_t *game);

bool G_Load(client_t *client, game_t *game);

void G_Rectangle(ivec2 start, ivec2 end, int *indices, int *count);
