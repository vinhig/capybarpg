#include "client/cl_client.h"
#include "game/g_game.h"
#include "vk/vk_vulkan.h"

#include <assert.h>
#include <ft2build.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include FT_FREETYPE_H

#include <cglm/vec2.h>
#include <zpl/zpl.h>

typedef struct character_t {
  unsigned texture_idx;

  vec2 size;
  vec2 bearing;
  unsigned advance;
} character_t;

ZPL_TABLE_DECLARE(extern, character_bank_t, CL_Characters_, character_t)
ZPL_TABLE_DEFINE(character_bank_t, CL_Characters_, character_t)

typedef struct client_console_t {
  char *history;
  wchar_t current_cmd[256];

  bool opened;

  // TODO: the console shouldn't own it's own copy of freetype
  FT_Library ft;
  FT_Face source_code_face;

  character_bank_t character_bank;

  texture_t *textures;
  unsigned texture_count;
  unsigned texture_capacity;

  unsigned char
      *white_space; // TODO: some font family return a 0x0 glyph for the white
                    // space, instead of submitting an invalid texture to the
                    // renderer, we'll submit a dummy 64x64 black square
} client_console_t;

bool CL_InitConsole(client_t *client, client_console_t **c) {
  (*c) = calloc(1, sizeof(client_console_t));
  client_console_t *console = *c;

  CL_Characters_init(&console->character_bank, zpl_heap_allocator());

  console->textures = calloc(128, sizeof(texture_t));
  console->texture_capacity = 128;
  console->texture_count = 0;
  console->white_space = calloc(1, 64 * 64);

  if (FT_Init_FreeType(&console->ft)) {
    printf("[ERROR] Couldn't init freetype for the console.\n");
    return false;
  }

  if (FT_New_Face(console->ft, "../source/resources/JF-Dot-Paw16.ttf", 0,
                  &console->source_code_face)) {
    printf(
        "[ERROR] Couldn't load Source Code font family "
        "(\"../source/resources/SourceCodePro-Bold.ttf\") for the console.\n");
    return false;
  }

  FT_Set_Pixel_Sizes(console->source_code_face, 0, 48);

  wchar_t alphabet[] = L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ()"
                       L"{}:/+-_*12345789おはよう!?èé&#<>\\\"ùàç ";

  for (unsigned i = 0; i < sizeof(alphabet) / sizeof(wchar_t); i++) {
    if (FT_Load_Char(console->source_code_face, alphabet[i], FT_LOAD_RENDER | FT_LOAD_COLOR)) {
      printf("[WARNING] Ooopsie doopsie, the char `%lc` isn't supported...\n",
             alphabet[i]);
    }

    FT_GlyphSlot the_glyph = console->source_code_face->glyph;
    // See console note
    if (the_glyph->bitmap.width == 0 || the_glyph->bitmap.rows == 0) {
      the_glyph->bitmap.width = 64;
      the_glyph->bitmap.rows = 64;
      the_glyph->bitmap.buffer = console->white_space;
    }

    unsigned data_size = the_glyph->bitmap.width * the_glyph->bitmap.rows;
    console->textures[console->texture_count] = (texture_t){
        .c = 1,
        .width = the_glyph->bitmap.width,
        .height = the_glyph->bitmap.rows,
        .data = memcpy(malloc(data_size), the_glyph->bitmap.buffer, data_size),
        .label = "character",
    };

    console->texture_count++;

    CL_Characters_set(&console->character_bank, alphabet[i],
                      (character_t){
                          i,
                          {
                              the_glyph->bitmap.width,
                              the_glyph->bitmap.rows,
                          },
                          {
                              the_glyph->bitmap_left,
                              the_glyph->bitmap_top,
                          },
                          the_glyph->advance.x,
                      });
  }

  VK_UploadFontTextures(CL_GetRend(client), console->textures,
                        console->texture_count);

  memcpy(console->current_cmd,
         L"add_wall 3 3 concrete_wall //おはよう! さようなら?",
         42 * sizeof(wchar_t));

  return true;
}

void CL_DrawConsole(client_t *client, game_state_t *state,
                    client_console_t *console) {
  unsigned len = wcslen(console->current_cmd);

  float current_pos = 0.0f;

  // While we register every single characters to be drawn, it's possible we
  // encounter characters that weren't previously loaded. It's not a problem ,
  // they'll be loaded at the end of this function (remember texture are
  // specified using an index, so no reference whatsoever).
  // Let's keep track of potential new texture.
  unsigned new_texture_count = 0;

  for (unsigned i = 0; i < len; i++) {
    character_t *character =
        CL_Characters_get(&console->character_bank, console->current_cmd[i]);

    unsigned screen_width, screen_height;
    CL_GetViewDim(client, &screen_width, &screen_height);

    // We didn't load all possible characters, maybe it's a new one, let's load
    // it
    if (!character) {
      if (FT_Load_Char(console->source_code_face, console->current_cmd[i],
                       FT_LOAD_RENDER | FT_LOAD_COLOR)) {
        printf("[WARNING] Ooopsie doopsie, a char `%lc` isn't supported...\n",
               console->current_cmd[i]);
      }

      FT_GlyphSlot the_glyph = console->source_code_face->glyph;
      // See console note
      if (the_glyph->bitmap.width == 0 || the_glyph->bitmap.rows == 0) {
        the_glyph->bitmap.width = 64;
        the_glyph->bitmap.rows = 64;
        the_glyph->bitmap.buffer = console->white_space;
      }

      unsigned data_size = the_glyph->bitmap.width * the_glyph->bitmap.width;
      console->textures[console->texture_count] = (texture_t){
          .c = 1,
          .width = the_glyph->bitmap.width,
          .height = the_glyph->bitmap.rows,
          .data =
              memcpy(malloc(data_size), the_glyph->bitmap.buffer, data_size),
          .label = "character",
      };

      new_texture_count++;

      CL_Characters_set(&console->character_bank, console->current_cmd[i],
                        (character_t){
                            console->texture_count,
                            {
                                the_glyph->bitmap.width,
                                the_glyph->bitmap.rows,
                            },
                            {
                                the_glyph->bitmap_left,
                                the_glyph->bitmap_top,
                            },
                            the_glyph->advance.x,
                        });

      console->texture_count++;

      character =
          CL_Characters_get(&console->character_bank, console->current_cmd[i]);
    }

    state->texts[i].color[0] = 1.0;
    state->texts[i].color[1] = 1.0;
    state->texts[i].color[2] = 1.0;
    state->texts[i].color[3] = 1.0;
    state->texts[i].tex = character->texture_idx;
    // Position according to character
    state->texts[i].pos[0] = current_pos + character->bearing[0];
    state->texts[i].pos[1] = -character->bearing[1];

    // Position at the top left of the screen

    state->texts[i].pos[0] -= screen_width;
    state->texts[i].pos[0] += 16;
    state->texts[i].pos[1] -= screen_height;
    state->texts[i].pos[1] += 48;

    // printf("advance --> %d\n", character->advance);
    state->texts[i].size[0] = character->size[0];
    state->texts[i].size[1] = character->size[1];

    current_pos += (float)(character->advance >> 6);
  }

  state->text_count = len;

  // Let's upload new glyphs texture
  if (new_texture_count) {
    VK_UpdateFontTextures(CL_GetRend(client),
                          console->textures +
                              (console->texture_count - new_texture_count),
                          new_texture_count);
  }
}

bool CL_ConsoleOpened(client_console_t *console) { return console->opened; }

void CL_ToggleConsole(client_console_t *console) {
  console->opened = !console->opened;
}

void CL_DestroyConsole(client_t *client, client_console_t *console) {
  FT_Done_Face(console->source_code_face);
  FT_Done_FreeType(console->ft);
}
