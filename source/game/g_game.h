#pragma once

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <wchar.h>

#include "cglm/types.h"

typedef struct client_t client_t;

typedef struct game_t game_t;

struct Transform;
struct ModelTransform;
struct Sprite;
struct Agent;
struct Immovable;

typedef enum agent_type_t {
  AGENT_ANIMAL = 1 << 0,
  AGENT_PLAYER = 1 << 1,
  AGENT_FACTION0 = 1 << 2,
  AGENT_FACTION1 = 1 << 3,
  AGENT_FACTION2 = 1 << 4,
  AGENT_FACTION3 = 1 << 5,
  AGENT_FACTION4 = 1 << 6,
  AGENT_FACTION5 = 1 << 7,
  AGENT_FACTION6 = 1 << 8,
} agent_type_t;

typedef struct short_string_t {
  char str[64];
} short_string_t;

typedef struct game_immediate_draw_t {
  float x, y, z;
  float w, h;
  unsigned handle;
} game_immediate_draw_t;

typedef struct game_text_draw_t {
  vec4 color;
  vec4 pos;
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
  game_text_draw_t *texts;
  atomic_int text_count;
  unsigned text_capacity;

} game_state_t;

typedef struct character_t {
  unsigned texture_idx;

  vec2 size;
  vec2 bearing;
  unsigned advance;
} character_t;

typedef struct material_t {
  char *name;
  uint64_t key;

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

typedef struct pawn_t {
  char *name;

  float scale;
  unsigned revision;

  unsigned north_tex;
  char *north_path;

  unsigned south_tex;
  char *south_path;

  unsigned east_tex;
  char *east_path;
} pawn_t;

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
void G_AddPawn(game_t *game, struct Transform *transform,
               struct Sprite *sprite, agent_type_t agent_type);

/// @brief Helper function to add a Furniture to the world (an entity with a
/// Transform, Model Transform, and Sprite components). Register it to the list
/// of usable material if applicable.
void G_AddFurniture(client_t *client, game_t *game, struct Transform *transform,
                    struct Sprite *sprite, struct Immovable *immovable);

game_state_t *G_TickGame(client_t *client, game_t *game);

void G_DestroyGame(game_t *game);

bool G_Load(client_t *client, game_t *game);

void G_Rectangle(ivec2 start, ivec2 end, int *indices, int *count);

character_t *G_GetCharacter(game_t *game, const char *family, wchar_t c);
