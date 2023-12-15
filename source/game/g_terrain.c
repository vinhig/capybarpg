#include <game/g_private.h>

#include <string.h>

void G_Rectangle(ivec2 start, ivec2 end, int *indices, int *count) {
  // start
  //  ----------------
  // |               |
  // |               |
  // |               |
  // -----------------
  //               end

  *count = 0;

  for (int i = start[0]; i < end[0]; i++) {
    ivec2 coord = {
        [0] = i,
        [1] = start[1],
    };
    indices[*count] = coord[1] * 256 + coord[0];
    (*count)++;
  }

  for (int i = start[0]; i < end[0]; i++) {
    ivec2 coord = {
        [0] = i,
        [1] = end[1],
    };
    indices[*count] = coord[1] * 256 + coord[0];
    (*count)++;
  }

  for (int i = start[1]; i < end[1]; i++) {
    ivec2 coord = {
        [0] = start[0],
        [1] = i,
    };
    indices[*count] = coord[1] * 256 + coord[0];
    (*count)++;
  }

  for (int i = start[1]; i < end[1] + 1; i++) {
    ivec2 coord = {
        [0] = end[0],
        [1] = i,
    };
    indices[*count] = coord[1] * 256 + coord[0];
    (*count)++;
  }
}

void G_Map_Set_Terrain_Type(qcvm_t *qcvm) {
  worker_t *worker = qcvm_get_user_data(qcvm);
  game_t *game = worker->game;

  int map = qcvm_get_parm_int(qcvm, 0);
  float x = qcvm_get_parm_float(qcvm, 1);
  float y = qcvm_get_parm_float(qcvm, 2);
  const char *recipe = qcvm_get_parm_string(qcvm, 3);

  if (map < 0 || map >= (int)game->map_count) {
    printf("[ERROR] Assertion G_Map_Set_Terrain_Type(map >= 0 || map < "
           "game->map_count) "
           "[map = %d, map_count = %d] should be "
           "verified.\n",
           map, game->map_count);
    return;
  }

  map_t *the_map = &worker->maps[map];

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
           "map->y) "
           "[y = %d, map->h = %d] should be "
           "verified.\n",
           (unsigned)y, the_map->h);
    return;
  }

  zpl_u64 key = zpl_fnv64(recipe, strlen(recipe));
  terrain_t *the_recipe = G_Terrains_get(&game->terrain_bank, key);

  if (!the_recipe) {
    printf("[ERROR] Assertion G_Map_Set_Terrain_Type(recipe exists) [recipe = "
           "\"%s\"] "
           "should be verified.\n",
           recipe);
    return;
  }

  unsigned idx = (unsigned)y * the_map->w + (unsigned)x;

  the_map->gpu_tiles[idx].texture = the_recipe->terrain_tex;
}

void G_Map_Get_Terrain_Type(qcvm_t *qcvm) {
}

void G_TerrainInstall(qcvm_t *qcvm) {}
