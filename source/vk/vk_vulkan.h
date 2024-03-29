#pragma once

#include "client/cl_client.h"
typedef struct game_state_t game_state_t;
typedef struct client_t client_t;
typedef struct vk_rend_t vk_rend_t;

typedef struct texture_t {
  int width, height, c;
  unsigned char *data;
  char *label;

  unsigned idx;
} texture_t;

typedef struct vk_model_t vk_model_t;

/**
 * @brief A vulkan renderer is created. There are two sets of width/height.
 * The first one details the resolution at which the interal resolution of
 * the renderer (the one at which it draws stuff). The second pair details the
 * screen width/height (that can be different when rendering at 1280x768 at
 * fullscreen on a 1920x1080 display for example).
 */
vk_rend_t *VK_CreateRend(client_t *client,
                         unsigned view_width, unsigned view_height,
                         unsigned screen_width, unsigned screen_height,
                         bool vsync, unsigned framerate);

void VK_Draw(client_t *client, vk_rend_t *rend, game_state_t *game);

void VK_DestroyRend(vk_rend_t *rend);

/**
 * @brief Upload an array of textures to the GPU, and create the related
 * descriptor set. The descriptor set will be used in the GBuffer graphics
 * pipeline, all textures will be accessible by an index (in bindless fashion).
 * The index of the texture is guaranteed to be the same as the index in the
 * specified textures array. The upload function should be called once
 * (typically after loading all recipes), at the moment there is no methods to
 * alter it.
 * @param rend The renderer, what do u think it is?
 * @param textures An array of `texture_t`.
 * @param count The number of textures present in the array.
 */
void VK_UploadMapTextures(vk_rend_t *rend, texture_t *textures, unsigned count);

/**
 * @brief Upload an array of textures to the GPU, and create the related
 * descriptor set. The descriptor set will be used in the Immediate graphics
 * pipeline to draw characters, all textures will be accessible by an index (in
 * bindless fashion). The index of the texture is guaranteed to be the same as
 * the index in the specified textures array. The upload function should be
 * called once, but you can add other characters using `VK_UpdateFontTextures`.
 * @param rend The renderer, what do u think it is?
 * @param textures An array of `texture_t`, all textures should have one channel
 * (the red one).
 * @param count The number of textures present in the array.
 */
void VK_UploadFontTextures(vk_rend_t *rend, texture_t *textures,
                           unsigned count);

void VK_UploadUITextures(vk_rend_t *rend, texture_t *textures,
                         unsigned count);

/**
 * @brief Update the list of drawable characters by adding `count` new textures.
 * @param rend The renderer, what do u think it is?
 * @param textures An array of `texture_t`, all textures should have one channel
 * (the red one).
 * @param count The number of textures present in the array, don't include the
 * textures that were already uploaded using `VK_UploadFontTextures`.
 */
void VK_UpdateFontTextures(vk_rend_t *rend, texture_t *textures,
                           unsigned count);

void VK_UploadSingleTexture(vk_rend_t *rend, texture_t *texture);

void* VK_GetTextureHandle(vk_rend_t* rend, unsigned idx);

const char *VK_GetError();

void VK_InitUI(client_t *client, vk_rend_t *rend);
void VK_BeginUI(client_t *client);
void VK_DestroyUI(vk_rend_t *rend);

void VK_LoadFont(const char *path, const char *name);
void *VK_GetFont(const char *name);
