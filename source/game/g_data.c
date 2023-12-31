#include <zpl/zpl.h>
#include "game/g_game.h"
#include "vk/vk_vulkan.h"

ZPL_TABLE_DECLARE(extern, material_bank_t, G_Materials_, material_t)
ZPL_TABLE_DEFINE(material_bank_t, G_Materials_, material_t)

ZPL_TABLE_DECLARE(extern, wall_bank_t, G_Walls_, wall_t)
ZPL_TABLE_DEFINE(wall_bank_t, G_Walls_, wall_t)

ZPL_TABLE_DECLARE(extern, texture_bank_t, G_ImmediateTextures_, texture_t)
ZPL_TABLE_DEFINE(texture_bank_t, G_ImmediateTextures_, texture_t)

ZPL_TABLE_DECLARE(extern, terrain_bank_t, G_Terrains_, terrain_t)
ZPL_TABLE_DEFINE(terrain_bank_t, G_Terrains_, terrain_t)

ZPL_TABLE_DECLARE(extern, pawn_bank_t, G_Pawns_, pawn_t)
ZPL_TABLE_DEFINE(pawn_bank_t, G_Pawns_, pawn_t)

ZPL_TABLE_DECLARE(extern, inventory_t, G_Inventory_, float)
ZPL_TABLE_DEFINE(inventory_t, G_Inventory_, float)

