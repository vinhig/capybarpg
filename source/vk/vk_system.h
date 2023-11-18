#pragma once

#include <stdbool.h>

#include <cglm/cglm.h>

typedef struct vk_rend_t vk_rend_t;
typedef struct vk_system_t vk_system_t;

#define uint unsigned

#define is_C
#include "shaders/systems/components.glsl"

bool VK_InitECS(vk_rend_t *rend, unsigned count);
void VK_DestroyECS(vk_rend_t *rend);

// Append a system that only works on Transform.
vk_system_t *VK_AddSystem_Transform(vk_rend_t *rend, const char *name,
                                    const char *src);

// Append a system that works on Transform/Agent pair.
vk_system_t *VK_AddSystem_Agent_Transform(vk_rend_t *rend, const char *name,
                                          const char *src);

void VK_TickSystems(vk_rend_t *rend);

unsigned VK_Add_Entity(vk_rend_t *rend, unsigned signature);

void VK_Add_Transform(vk_rend_t *rend, unsigned entity,
                      struct Transform *transform);
void VK_Add_Model_Transform(vk_rend_t *rend, unsigned entity,
                            struct ModelTransform *model);
void VK_Add_Agent(vk_rend_t *rend, unsigned entity, struct Agent *agent);
void VK_Add_Sprite(vk_rend_t *rend, unsigned entity, struct Sprite *sprite);
void VK_Add_Immovable(vk_rend_t *rend, unsigned entity,
                      struct Immovable *immovable);

void VK_SetMap(vk_rend_t *rend, struct Tile *tiles, unsigned map_width,
               unsigned map_height);

void *VK_GetAgents(vk_rend_t *rend);
void *VK_GetTransforms(vk_rend_t *rend);
void *VK_GetMap(vk_rend_t *rend);
void *VK_GetEntities(vk_rend_t *rend);
