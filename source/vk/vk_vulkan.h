#pragma once

#include <stddef.h>

typedef struct game_state_t game_state_t;
typedef struct client_t client_t;
typedef struct vk_rend_t vk_rend_t;

typedef struct texture_t {
  int width, height, c;
  unsigned char *data;
  const char *label;
} texture_t;

typedef struct vk_model_t vk_model_t;

vk_rend_t *VK_CreateRend(client_t *client, unsigned width, unsigned height);

void VK_Draw(vk_rend_t *rend, game_state_t* game);

void VK_DestroyRend(vk_rend_t *rend);

void VK_UploadTextures(vk_rend_t* rend, texture_t *textures, unsigned count);

const char *VK_GetError();
