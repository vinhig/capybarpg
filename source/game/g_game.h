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

typedef struct game_state_t {
  // First person camera.
  // Yes, we want to speed gameplay
  struct {
    mat4 view;
    mat4 proj;
    mat4 view_proj;
    // vec3 pos;
  } fps;

} game_state_t;

typedef struct material_t {
  const char *name;
} material_t;

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
                    struct Sprite *sprite, struct Immovable* immovable);

game_state_t G_TickGame(client_t *client, game_t *game);

void G_DestroyGame(game_t *game);

bool G_Load(client_t *client, game_t *game);

void G_Rectangle(ivec2 start, ivec2 end, unsigned *indices, unsigned *count);
