#include <game/g_private.h>

#include <jps.h>
#include <string.h>

// TODO: should do the same function for a circle, line, polygon, etc

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

void G_Map_Set_Terrain_Type_QC(qcvm_t *qcvm) {
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

  map_t *the_map = &game->maps[map];

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

  int variant = rand() % 3;

  if (variant == 0) {
    the_map->gpu_tiles[idx].terrain_texture = the_recipe->variant1_tex;
  } else if (variant == 1) {
    the_map->gpu_tiles[idx].terrain_texture = the_recipe->variant2_tex;
  } else {
    the_map->gpu_tiles[idx].terrain_texture = the_recipe->variant3_tex;
  }

  // printf("setting the shitty tiles[%d] to %d\n", idx,
  // the_map->gpu_tiles[idx].texture);
}

void G_Map_Get_Terrain_Type_QC(qcvm_t *qcvm) {
  worker_t *worker = qcvm_get_user_data(qcvm);
  game_t *game = worker->game;

  int map = qcvm_get_parm_int(qcvm, 0);
  float x = qcvm_get_parm_float(qcvm, 1);
  float y = qcvm_get_parm_float(qcvm, 2);

  if (map < 0 || map >= (int)game->map_count) {
    printf("[ERROR] Assertion G_Map_Set_Terrain_Type(map >= 0 || map < "
           "game->map_count) "
           "[map = %d, map_count = %d] should be "
           "verified.\n",
           map, game->map_count);
    return;
  }

  map_t *the_map = &game->maps[map];

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

  unsigned idx = (unsigned)y * the_map->w + (unsigned)x;

  zpl_unused(idx);

  qcvm_return_string(qcvm, "about_to_be_done");
}

void G_TerrainInstall(qcvm_t *qcvm) {
  qcvm_export_t export_G_Map_Set_Terrain_Type = {
      .func = G_Map_Set_Terrain_Type_QC,
      .name = "G_Map_Set_Terrain_Type",
      .argc = 4,
      .args[0] = {.name = "map", .type = QCVM_INT},
      .args[1] = {.name = "x", .type = QCVM_FLOAT},
      .args[2] = {.name = "y", .type = QCVM_FLOAT},
      .args[3] = {.name = "recipe", .type = QCVM_STRING},
  };

  qcvm_export_t export_G_Map_Get_Terrain_Type = {
      .func = G_Map_Get_Terrain_Type_QC,
      .name = "G_Map_Get_Terrain_Type",
      .argc = 3,
      .args[0] = {.name = "map", .type = QCVM_INT},
      .args[1] = {.name = "x", .type = QCVM_FLOAT},
      .args[2] = {.name = "y", .type = QCVM_FLOAT},
  };

  qcvm_add_export(qcvm, &export_G_Map_Set_Terrain_Type);
  qcvm_add_export(qcvm, &export_G_Map_Get_Terrain_Type);
}

static inline int imin(unsigned a, unsigned b) {
  if (a < b) {
    return a;
  } else {
    return b;
  }
}

unsigned G_ComputeWallOrientation(map_t *map, wall_t *wall, int instance_x,
                                  int instance_y) {
  int top, bottom, left, right;

  // Some out-of-bound checks, that's why we work in signed integer in this code
  // part

  if (instance_y - 1 < 0) {
    top = 0;
  } else {
    top = imin(
        1.0,
        map->gpu_tiles[instance_x + (instance_y - 1) * map->w].wall_texture);
  }

  if (instance_y + 1 >= (int)map->h) {
    bottom = 0;
  } else {
    bottom = imin(
        1.0,
        map->gpu_tiles[instance_x + (instance_y + 1) * map->w].wall_texture);
  }

  if (instance_x - 1 < 0) {
    left = 0;
  } else {
    left = imin(
        1.0,
        map->gpu_tiles[(instance_x - 1) + (instance_y)*map->w].wall_texture);
  }

  if (instance_x + 1 >= (int)map->w) {
    right = 0;
  } else {
    right = imin(
        1.0,
        map->gpu_tiles[(instance_x + 1) + (instance_y)*map->w].wall_texture);
  }

  int wall_sig = top << 4 | bottom << 3 | left << 2 | right << 1;

  switch (wall_sig) {
  case 1 << 1:
    return wall->only_right_tex;
  case 0:
    return wall->nothing_tex;
  case 1 << 2:
    return wall->only_left_tex;
  case 1 << 4:
    return wall->only_top_tex;
  case 1 << 3:
    return wall->only_bottom_tex;
  case 1 << 4 | 1 << 3 | 1 << 2 | 1 << 1:
    return wall->all_tex;
  case 1 << 3 | 1 << 2 | 1 << 1:
    return wall->left_right_bottom_tex;
  case 1 << 2 | 1 << 1:
    return wall->left_right_tex;
  case 1 << 4 | 1 << 2 | 1 << 1:
    return wall->left_right_tex;
  case 1 << 4 | 1 << 3:
    return wall->top_bottom_tex;
  case 1 << 4 | 1 << 1:
    return wall->right_top_tex;
  case 1 << 2 | 1 << 4:
    return wall->left_top_tex;
  case 1 << 1 | 1 << 3:
    return wall->right_bottom_tex;
  case 1 << 2 | 1 << 3:
    return wall->left_bottom_tex;
  case 1 << 4 | 1 << 3 | 1 << 2:
    return wall->left_top_bottom_tex;
  case 1 << 4 | 1 << 3 | 1 << 1:
    return wall->right_top_bottom_tex;
  default:
    return 0;
  }
}

void G_Add_Wall(game_t *game, int map, int x, int y, float health,
                wall_t *wall_recipe) {
  // Since JPS isn't multithread friendly, we maintain a copy in each worker. It
  // forces us to lock and make the modification multiple times. We lock them
  // all to be sure there is not bad surprise.
  zpl_mutex_lock(&game->maps[map].mutex);

  map_t *the_map = &game->maps[map];
  jps_set_obstacle(the_map->jps_map, x, y, 1);
  unsigned idx = y * the_map->w + x;
  // Place the wall with its correct orientation
  the_map->gpu_tiles[idx].wall_texture =
      G_ComputeWallOrientation(the_map, wall_recipe, x, y);
  the_map->cpu_tiles[idx].related_wall_recipe = wall_recipe;

  // And then, maybe update its neighbors
  for (int xx = x - 1; xx < x + 2; xx++) {
    for (int yy = y - 1; yy < y + 2; yy++) {
      if (xx < 0 || yy < 0 || xx > (int)the_map->w || yy > (int)the_map->h) {
        continue;
      }

      unsigned neighbor_idx = yy * the_map->w + xx;

      if (the_map->gpu_tiles[neighbor_idx].wall_texture != 0) {
        the_map->gpu_tiles[neighbor_idx].wall_texture =
            G_ComputeWallOrientation(
                the_map, the_map->cpu_tiles[neighbor_idx].related_wall_recipe,
                xx, yy);
      }
    }
  }

  zpl_mutex_unlock(&game->maps[map].mutex);
}
