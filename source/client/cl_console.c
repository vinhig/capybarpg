#include "cglm/types.h"
#include "client/cl_client.h"
#include "client/cl_input.h"
#include "game/g_game.h"
#include "vk/vk_vulkan.h"

#include <ft2build.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include FT_FREETYPE_H

#include <cglm/vec2.h>
#include <zpl/zpl.h>

ZPL_TABLE_DECLARE(extern, character_bank_t, CL_Characters_, character_t)
ZPL_TABLE_DEFINE(character_bank_t, CL_Characters_, character_t)

typedef struct client_console_t {
  wchar_t *history;
  // for sprintf operation that requires restricted pointer
  wchar_t *history_temp;
  unsigned history_size;

  wchar_t *output;

  bool opened;

  unsigned char
      *white_space; // TODO: some font family return a 0x0 glyph for the white
                    // space, instead of submitting an invalid texture to the
                    // renderer, we'll submit a dummy 64x64 black square. Its
                    // dimension/margins/etc will be copied from the "_"
                    // character.

  cmd_desc_t *descriptions;
  unsigned description_count;
  unsigned description_capacity;
} client_console_t;

bool CL_PrintVersionConsole(client_console_t *console, void *user_data,
                            wchar_t args[64][64], unsigned count) {
  if (count != 0) {
    console->output = L"CL_PrintVersionConsole accepts no argument.";

    return false;
  } else {
    console->output = L"CapybaRPG (maidenless v0.0.1)";

    return true;
  }
}

bool CL_ExitConsole(client_console_t *console, void *user_data,
                    wchar_t args[64][64], unsigned count) {
  if (count != 0) {
    console->output = L"CL_ExitConsole accepts no argument.";

    return false;
  } else {
    console->output = L"Exiting.";

    CL_ExitClient((client_t *)user_data);

    return true;
  }
}

bool CL_InitConsole(client_t *client, client_console_t **c) {
  (*c) = calloc(1, sizeof(client_console_t));
  client_console_t *console = *c;

  console->history = calloc(1024, sizeof(wchar_t));
  console->history_temp = calloc(1024, sizeof(wchar_t));
  console->history_size = 1024;

  console->description_capacity = 16;
  console->description_count = 0;
  console->descriptions = calloc(16, sizeof(cmd_desc_t));

  cmd_desc_t version_command = {
      .command = L"version",
      .callback = CL_PrintVersionConsole,
  };
  cmd_desc_t exit_command = {
      .command = L"exit",
      .callback = CL_ExitConsole,
      .user_data = client,
  };

  CL_ExportCommandConsole(console, &version_command);
  CL_ExportCommandConsole(console, &exit_command);

  return true;
}

void CL_ExportCommandConsole(client_console_t *console, cmd_desc_t *desc) {
  if (console->description_capacity == console->description_count) {
    console->descriptions =
        realloc(console->descriptions,
                console->description_capacity * 2 * sizeof(cmd_desc_t));
    console->description_capacity *= 2;
  }

  console->descriptions[console->description_count] = *desc;
  console->description_count++;
}

void CL_UpdateConsole(client_t *client, client_console_t *console) {
  if (CL_GetInput(client)->text_editing.submit) {
    // Execute the command
    wchar_t *content = &CL_GetInput(client)->text_editing.content[0];
    unsigned len = wcslen(content);

    wchar_t args[64][64] = {};
    unsigned arg_len[64] = {};
    unsigned arg_count = 0;

    for (unsigned i = 0; i < len; i++) {
      if (content[i] == L'"') {
        i++;
        while (content[i] != L'"') {
          args[arg_count][arg_len[arg_count]] = content[i];
          arg_len[arg_count]++;
          i++;
        }
      } else if (content[i] == L' ') {
        while (content[i + 1] == L' ') {
          i++;
        }
        arg_count++;
      } else {
        args[arg_count][arg_len[arg_count]] = content[i];
        arg_len[arg_count]++;
      }
    }

    if (args[arg_count][0] != L'\0') {
      arg_count++;
    }

    console->output = NULL;

    bool result = false;
    bool found = false;
    for (unsigned c = 0; c < console->description_count; c++) {
      if (wcscmp(console->descriptions[c].command, args[0]) == 0) {
        found = true;
        result = console->descriptions[c].callback(
            console, console->descriptions[c].user_data, args, arg_count - 1);
      }
    }

    if (result) {
      swprintf(console->history_temp, console->history_size,
               L"%ls<green> >> <white>%ls\n", console->history,
               &CL_GetInput(client)->text_editing.content[0]);
    } else {
      if (found) {
        swprintf(console->history_temp, console->history_size,
                 L"%ls<red>[X] <white>%ls\n", console->history,
                 &CL_GetInput(client)->text_editing.content[0]);
      } else {
        swprintf(console->history_temp, console->history_size,
                 L"%ls<red>[X] <white>%ls\n<grey>Command not found.\n",
                 console->history,
                 &CL_GetInput(client)->text_editing.content[0]);
      }
    }

    memcpy(console->history, console->history_temp,
           wcslen(console->history_temp) * sizeof(wchar_t));

    if (console->output) {
      swprintf(console->history_temp, console->history_size, L"%ls<grey>%ls\n",
               console->history, console->output);

      memcpy(console->history, console->history_temp,
             wcslen(console->history_temp) * sizeof(wchar_t));
    }

    CL_GetInput(client)->text_editing.content[0] = L'\0';
  }
}

void CL_DrawConsole(client_t *client, game_t *game, game_state_t *state,
                    client_console_t *console) {
  if (!console->opened) {
    return;
  }

  wchar_t *input = &CL_GetInput(client)->text_editing.content[0];

  wchar_t cmd[256];

  if ((int)(zpl_time_rel() * 2.2f) % 2) {
    swprintf(&cmd[0], 256, L"!> %ls", input);
  } else {
    swprintf(&cmd[0], 256, L"?> %ls", input);
  }

  unsigned cmd_len = wcslen(cmd);
  unsigned history_len = wcslen(console->history);

  float current_pos = 0.0f;

  unsigned screen_width, screen_height;
  CL_GetViewDim(client, &screen_width, &screen_height);

  float top = 0.0f;

  vec4 current_color = {1.0f, 1.0f, 1.0f, 1.0f};

  unsigned off = atomic_load(&state->text_count);

  // printf("offset is %d\n", off);

  for (unsigned i = 0; i < history_len; i++) {
    if (console->history[i] == L'\n') {
      top += 48.0f;
      current_pos = 0.0f;
      continue;
    } else if (console->history[i] == L'<') {
      if (wcsncmp(console->history + i, L"<red>", 5) == 0) {
        current_color[0] = 1.0f;
        current_color[1] = 0.0f;
        current_color[2] = 0.0f;
        current_color[3] = 1.0f;
        i += 4;
        continue;
      } else if (wcsncmp(console->history + i, L"<yellow>", 8) == 0) {
        current_color[0] = 1.0f;
        current_color[1] = 1.0f;
        current_color[2] = 0.0f;
        current_color[3] = 1.0f;
        i += 7;
        continue;
      } else if (wcsncmp(console->history + i, L"<blue>", 6) == 0) {
        current_color[0] = 0.0f;
        current_color[1] = 1.0f;
        current_color[2] = 0.0f;
        current_color[3] = 1.0f;
        i += 5;
        continue;
      } else if (wcsncmp(console->history + i, L"<green>", 7) == 0) {
        current_color[0] = 0.0f;
        current_color[1] = 1.0f;
        current_color[2] = 0.0f;
        current_color[3] = 1.0f;
        i += 6;
        continue;
      } else if (wcsncmp(console->history + i, L"<orange>", 8) == 0) {
        current_color[0] = 1.0f;
        current_color[1] = 165.0f / 255.0f;
        current_color[2] = 0.0f;
        current_color[3] = 1.0f;
        i += 7;
        continue;
      } else if (wcsncmp(console->history + i, L"<white>", 7) == 0) {
        current_color[0] = 1.0f;
        current_color[1] = 1.0f;
        current_color[2] = 1.0f;
        current_color[3] = 1.0f;
        i += 6;
        continue;
      } else if (wcsncmp(console->history + i, L"<grey>", 6) == 0) {
        current_color[0] = 0.69f;
        current_color[1] = 0.69f;
        current_color[2] = 0.69f;
        current_color[3] = 1.0f;
        i += 5;
        continue;
      } else {
        printf("no matching markup =-( at %ls\n", console->history + i);
      }
    }

    character_t *character =
        G_GetCharacter(game, "console", console->history[i]);

    if (!character) {
      printf("pls cry about %lc\n", console->history[i]);
      continue;
    }

    state->texts[i + off].color[0] = current_color[0];
    state->texts[i + off].color[1] = current_color[1];
    state->texts[i + off].color[2] = current_color[2];
    state->texts[i + off].color[3] = current_color[3];
    state->texts[i + off].tex = character->texture_idx;
    // Position according to character
    state->texts[i + off].pos[0] = current_pos + character->bearing[0];
    state->texts[i + off].pos[1] = -character->bearing[1];

    // Position at the top left of the screen

    state->texts[i + off].pos[0] -= screen_width;
    state->texts[i + off].pos[0] += 16;
    state->texts[i + off].pos[1] -= screen_height;
    state->texts[i + off].pos[1] += (48 + top);

    // printf("advance --> %d\n", character->advance);
    state->texts[i + off].size[0] = character->size[0] / (float)screen_width;
    state->texts[i + off].size[1] = character->size[1] / (float)screen_height;

    state->texts[i + off].pos[0] /= (float)screen_width;
    state->texts[i + off].pos[1] /= (float)screen_height;

    current_pos += (float)(character->advance >> 6);
  }

  off += history_len;

  for (unsigned i = 0; i < cmd_len; i++) {
    character_t *character = G_GetCharacter(game, "console", cmd[i]);

    // We didn't load all possible characters, maybe it's a new one, let's load
    // it
    if (!character) {
      continue;
    }

    state->texts[i + off].color[0] = current_color[0];
    state->texts[i + off].color[1] = current_color[1];
    state->texts[i + off].color[2] = current_color[2];
    state->texts[i + off].color[3] = current_color[3];
    state->texts[i + off].tex = character->texture_idx;
    // Position according to character
    state->texts[i + off].pos[0] = current_pos + character->bearing[0];
    state->texts[i + off].pos[1] = -character->bearing[1];

    // Position at the top left of the screen

    state->texts[i + off].pos[0] -= screen_width;
    state->texts[i + off].pos[0] += 16;
    state->texts[i + off].pos[1] -= screen_height;
    state->texts[i + off].pos[1] += (48 + top);

    // printf("advance --> %d\n", character->advance);
    state->texts[i + off].size[0] = character->size[0] / (float)screen_width;
    state->texts[i + off].size[1] = character->size[1] / (float)screen_height;

    state->texts[i + off].pos[0] /= (float)screen_width;
    state->texts[i + off].pos[1] /= (float)screen_height;

    current_pos += (float)(character->advance >> 6);
  }

  atomic_fetch_add(&state->text_count, history_len + cmd_len);
}

bool CL_ConsoleOpened(client_console_t *console) { return console->opened; }

void CL_ToggleConsole(client_console_t *console) {
  console->opened = !console->opened;
}

void CL_DestroyConsole(client_t *client, client_console_t *console) {
  free(console->descriptions);
  free(console->history);
  free(console->history_temp);

  free(console);
}
