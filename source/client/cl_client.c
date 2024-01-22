#include "cl_client.h"
#include "SDL_events.h"
#include "SDL_keycode.h"
#include "SDL_mouse.h"
#include "SDL_video.h"
#include "cl_input.h"
#include "vk/vk_vulkan.h"

#include <SDL2/SDL.h>
#include <errno.h>
#include <minini/minIni.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <wchar.h>
#include <zpl/zpl.h>

#include <cimgui.h>

#include "game/g_game.h"

char *const *Global_Argv;

ZPL_TABLE_DECLARE(extern, string_dict_t, CL_Strings_, short_string_t)
ZPL_TABLE_DECLARE(extern, float_dict_t, CL_Floats_, float)
ZPL_TABLE_DECLARE(extern, int_dict_t, CL_Integers_, int)

struct client_t {
  SDL_Window *window;
  client_state_t state;

  client_console_t *console;

  unsigned screen_width, screen_height;
  unsigned view_width, view_height;

  vk_rend_t *rend;

  input_t input;

  string_dict_t global_variable_strings;
  string_dict_t global_variable_keys; // save the string key (used when writing config file that are a dump of certain global variables)
  float_dict_t global_variable_floats;
  int_dict_t global_variable_ints;
};

client_desc_t client_desc_default() {
  return (client_desc_t){
      .vsync = 2,
      .fullscreen = 2,
  };
}

void *CL_GetWindow(client_t *client) { return client->window; }

input_t *CL_GetInput(client_t *client) { return &client->input; }

void ImGui_ProcessEvent(client_t *client, SDL_Event *event);

bool CL_ParseClientDesc(client_desc_t *desc, int argc, char *argv[]) {
  // Arbitrary decision: in debug mode, a badly formed argument is fatal
  //                     in release mode, it's not
  // It's handled on the function call site (int main)
  Global_Argv = argv;
  bool is_error = false;
  for (int i = 1; i < argc; i += 2) {
    char *arg = argv[i];
    // Check for each pair of argv what's the name of the property to change.
    // Most of the properties are strings that should be transformed into
    // number, resulting in a lot of if-else. Deeply sorry, but no other way to
    // ensure that.
    if (!strcmp(arg, "--width") || !strcmp(arg, "-w")) {
      char *width = argv[i + 1];
      char *endptr;
      unsigned val = strtol(width, &endptr, 10);

      if ((endptr - width) == 0 || (endptr - width) != (long)strlen(width)) {
        printf("Couldn't parse '--width' argument value '%s'.\n", width);
        is_error = true;
      } else {
        desc->width = val;
      }
    } else if (!strcmp(arg, "--height") || !strcmp(arg, "-h")) {
      char *height = argv[i + 1];
      char *endptr;
      unsigned val = strtol(height, &endptr, 10);

      if ((endptr - height) == 0 || (endptr - height) != (long)strlen(height)) {
        printf("Couldn't parse '--height' argument value '%s'.\n", height);
        is_error = true;
      } else {
        desc->height = val;
      }
    } else if (!strcmp(arg, "--fullscreen") || !strcmp(arg, "-f")) {
      if (i + 1 >= argc) {
        printf("Missing 'true' or 'false' after '--fullscreen' or '-f'.\n");
        is_error = true;
        break;
      }
      char *fullscreen = argv[i + 1];

      if (!strcmp(fullscreen, "true")) {
        desc->fullscreen = true;
      } else if (!strcmp(fullscreen, "false")) {
        desc->fullscreen = false;
      } else {
        printf("Fullscreen is either 'true' or 'false'.\n");
        is_error = true;
      }
    } else if (!strcmp(arg, "--physical_device") || !strcmp(arg, "-gpu")) {
      char *gpu = argv[i + 1];

      // Remove " " if they are present
      if (gpu[0] == '"' && gpu[strlen(gpu) - 1] == '"') {
        gpu[strlen(gpu) - 1] = '\0';
        gpu++;
      }

      desc->desired_gpu = malloc(strlen(gpu) + 1);
      strcpy(desc->desired_gpu, gpu);
    } else if (!strcmp(arg, "--base") || !strcmp(arg, "-b")) {
      char *base = argv[i + 1];

      // Remove " " if they are present
      if (base[0] == '"' && base[strlen(base) - 1] == '"') {
        base[strlen(base) - 1] = '\0';
        base++;
      }

      desc->game = malloc(strlen(base) + 1);
      strcpy(desc->game, base);
    } else if (!strcmp(arg, "--only-scripting") || !strcmp(arg, "-s")) {
      if (i + 1 >= argc) {
        printf("Missing 'true' or 'false' after '--only_scripting' or '-s'.\n");
        is_error = true;
        break;
      }
      char *only_scripting = argv[i + 1];

      if (!strcmp(only_scripting, "true")) {
        desc->only_scripting = true;
      } else if (!strcmp(only_scripting, "false")) {
        desc->only_scripting = false;
      } else {
        printf("Only Scripting is either 'true' or 'false'.\n");
        is_error = true;
      }
    }
  }

  return is_error;
}

#define IF_NULL(type, var, value, default) \
  type *check = value;                     \
  if (check) {                             \
    var = *check;                          \
  } else {                                 \
    var = default;                         \
  }

void CL_CompleteClientDesc(client_t *client, client_desc_t *desc) {
  if (desc->width == 0) {
    zpl_u64 key = zpl_fnv64("video$resolution_width", strlen("video$resolution_width"));
    IF_NULL(int, desc->width, CL_Integers_get(&client->global_variable_ints, key), 1280)
  }

  if (desc->height == 0) {
    zpl_u64 key = zpl_fnv64("video$resolution_height", strlen("video$resolution_height"));
    IF_NULL(int, desc->height, CL_Integers_get(&client->global_variable_ints, key), 768)
  }

  if (desc->vsync == 2) {
    zpl_u64 key = zpl_fnv64("video$vsync", strlen("video$vsync"));
    IF_NULL(int, desc->vsync, CL_Integers_get(&client->global_variable_ints, key), 1)
  }

  if (desc->framerate == 0) {
    zpl_u64 key = zpl_fnv64("video$framerate", strlen("video$framerate"));
    IF_NULL(int, desc->framerate, CL_Integers_get(&client->global_variable_ints, key), 1)
  }

  if (desc->fullscreen == 2) {
    zpl_u64 key = zpl_fnv64("video$fullscreen", strlen("video$fullscreen"));
    IF_NULL(int, desc->fullscreen, CL_Integers_get(&client->global_variable_ints, key), 0)
  }
}

client_t *CL_CreateClient(const char *title, client_desc_t *desc) {
  client_t *client = calloc(1, sizeof(client_t));

  CL_Floats_init(&client->global_variable_floats, zpl_heap_allocator());
  CL_Strings_init(&client->global_variable_strings, zpl_heap_allocator());
  CL_Strings_init(&client->global_variable_keys, zpl_heap_allocator());
  CL_Integers_init(&client->global_variable_ints, zpl_heap_allocator());

  CL_LoadGlobalVariables(client, "settings.ini");

  CL_CompleteClientDesc(client, desc);

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_JOYSTICK) != 0) {
    printf("Failed to initialize SDL2.\n");
    return NULL;
  }

  SDL_WindowFlags flags = SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN;

  int window_pos_x = SDL_WINDOWPOS_CENTERED;
  int window_pos_y = SDL_WINDOWPOS_CENTERED;

  SDL_DisplayMode display_mode;
  SDL_GetCurrentDisplayMode(0, &display_mode);
  int screen_width = (int)desc->width;
  int screen_height = (int)desc->height;

  if (desc->fullscreen) {
    flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    screen_width = display_mode.w;
    screen_height = display_mode.h;
  }

  SDL_Window *window =
      SDL_CreateWindow(title, window_pos_x, window_pos_y,
                       screen_width, screen_height, flags);

  if (window == NULL) {
    printf("Failed to create a SDL2 window: %s\n", SDL_GetError());
    return NULL;
  }

  client->state = CLIENT_RUNNING;
  client->window = window;

  client->view_width = desc->width;
  client->screen_width = screen_width;
  client->view_height = desc->height;
  client->screen_height = screen_height;

  // TODO: the referenced GPU in the description should be passed
  client->rend = VK_CreateRend(client,
                               desc->width, desc->height,
                               screen_width, screen_height,
                               desc->vsync, desc->framerate);

  if (client->rend == NULL) {
    printf("Failed to create a Vulkan renderer.\n");
    SDL_DestroyWindow(window);
    SDL_Quit();
    return NULL;
  }

  // SDL_SetRelativeMouseMode(true);

  for (int i = 0; i < SDL_NumJoysticks(); i++) {
    SDL_JoystickOpen(i);
  }

  printf("[VERBOSE] %d joystick(s) connected.\n", SDL_NumJoysticks());

  CL_InitConsole(client, &client->console);

  return client;
}

client_state_t CL_GetClientState(client_t *client) { return client->state; }

void CL_SetClientState(client_t *client, client_state_t state) { client->state = state; }

vk_rend_t *CL_GetRend(client_t *client) { return client->rend; }

void CL_GetViewDim(client_t *client, unsigned *width, unsigned *height) {
  *width = client->view_width;
  *height = client->view_height;
}

void CL_GetScreenDim(client_t *client, unsigned *width, unsigned *height) {
  *width = client->screen_width;
  *height = client->screen_height;
}

void CL_UpdateClient(client_t *client) {
  SDL_Event event;

  if (client->input.mouse_left == 2) {
    client->input.mouse_left = 1;
  }
  if (client->input.mouse_right == 2) {
    client->input.mouse_right = 1;
  }

  client->input.wheel = 0.0;

  if (CL_ConsoleOpened(client->console)) {
    client->input.text_editing.submit = false;

    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        client->state = CLIENT_QUITTING;
        return;
      }

      switch (event.type) {
      case SDL_KEYUP: {
        if (event.key.keysym.sym == 178) {
          // The little ² at the upper left of your keyboard (assuming an AZERTY
          // keyboard)
          CL_ToggleConsole(client->console);
          SDL_StopTextInput();
          memset(&client->input.text_editing.content[0], 0,
                 sizeof(client->input.text_editing.content));
        } else if (event.key.keysym.sym == SDLK_BACKSPACE) {
          client->input.text_editing
              .content[wcslen(client->input.text_editing.content) - 1] = L'\0';
        } else if (event.key.keysym.sym == 13) {
          client->input.text_editing.submit = true;
        }
        break;
      }
      case SDL_TEXTINPUT: {
        wchar_t temp[32];
        mbstowcs(&temp[0], event.text.text, 32);

        wcscat(client->input.text_editing.content, &temp[0]);
        break;
      }
      }
    }

  } else {
    //   client->input.view.x_axis = 0.0;
    //   client->input.view.y_axis = 0.0;

    while (SDL_PollEvent(&event)) {
      ImGui_ProcessEvent(client, &event);

      if (event.type == SDL_QUIT) {
        client->state = CLIENT_QUITTING;
        return;
      }
      switch (event.type) {
      case SDL_MOUSEWHEEL: {
        client->input.wheel = event.wheel.y;
        break;
      }
      case SDL_KEYDOWN: {
        if (event.key.keysym.sym == SDLK_z || event.key.keysym.sym == SDLK_UP) {
          client->input.movement.x_axis = 1.0f;
        } else if (event.key.keysym.sym == SDLK_s ||
                   event.key.keysym.sym == SDLK_DOWN) {
          client->input.movement.x_axis = -1.0f;
        }

        if (event.key.keysym.sym == SDLK_q ||
            event.key.keysym.sym == SDLK_LEFT) {
          client->input.movement.y_axis = 1.0f;
        } else if (event.key.keysym.sym == SDLK_d ||
                   event.key.keysym.sym == SDLK_RIGHT) {
          client->input.movement.y_axis = -1.0f;
        }
        break;
      }
      case SDL_JOYAXISMOTION: {
        // unsigned joystick_id = event.jdevice.which;
        if (event.jaxis.axis == 0) {
          float value = (float)event.jaxis.value / 32767.0f;
          if (value < 5.0e-03 && value > -5.0e-03) {
            value = 0.0f;
          }
          client->input.movement.y_axis = -value;
        }
        if (event.jaxis.axis == 1) {
          float value = (float)event.jaxis.value / 32767.0f;
          if (value < 5.0e-03 && value > -5.0e-03) {
            value = 0.0f;
          }
          client->input.movement.x_axis = -value;
        }
        break;
      }
      case SDL_KEYUP: {
        if (event.key.keysym.sym == SDLK_z || event.key.keysym.sym == SDLK_UP) {
          client->input.movement.x_axis = 0.0f;
        } else if (event.key.keysym.sym == SDLK_s ||
                   event.key.keysym.sym == SDLK_DOWN) {
          client->input.movement.x_axis = 0.0f;
        }

        if (event.key.keysym.sym == SDLK_q ||
            event.key.keysym.sym == SDLK_LEFT) {
          client->input.movement.y_axis = 0.0f;
        } else if (event.key.keysym.sym == SDLK_d ||
                   event.key.keysym.sym == SDLK_RIGHT) {
          client->input.movement.y_axis = 0.0f;
        }

        if (event.key.keysym.sym == 178) {
          // The little ² at the upper left of your keyboard (assuming an AZERTY
          // keyboard)
          CL_ToggleConsole(client->console);
          SDL_StartTextInput();
        }

        break;
      }
      case SDL_MOUSEMOTION: {
        client->input.view.x_axis = event.motion.yrel;
        client->input.view.y_axis = -event.motion.xrel;
        break;
      }
      case SDL_MOUSEBUTTONDOWN: {
        if (event.button.button == SDL_BUTTON_LEFT) {
          client->input.mouse_left = 2;
        } else if (event.button.button == SDL_BUTTON_RIGHT) {
          client->input.mouse_right = 2;
        }
        break;
      }
      case SDL_MOUSEBUTTONUP: {
        if (event.button.button == SDL_BUTTON_LEFT) {
          client->input.mouse_left = 0;
        } else if (event.button.button == SDL_BUTTON_RIGHT) {
          client->input.mouse_right = 0;
        }
        break;
      }
      default: {
      }
      }
    }
  }

  SDL_GetMouseState(&client->input.mouse_x, &client->input.mouse_y);

  CL_UpdateConsole(client, client->console);
}

void CL_DrawClient(client_t *client, game_t *game, game_state_t *state) {
  CL_DrawConsole(client, game, state, client->console);
  VK_Draw(client, client->rend, state);
}

void CL_ExitClient(client_t *client) { client->state = CLIENT_QUITTING; }

void CL_StartClient() {
  execv(Global_Argv[0], Global_Argv);
  printf("should restart here!\n");
}

void CL_RestartClient(client_t *client) {
  client->state = CLIENT_QUITTING;

  atexit(CL_StartClient);
}

void CL_DestroyClient(client_t *client) {
  CL_Strings_destroy(&client->global_variable_strings);
  CL_Strings_destroy(&client->global_variable_keys);
  CL_Floats_destroy(&client->global_variable_floats);
  CL_Integers_destroy(&client->global_variable_ints);

  CL_DestroyConsole(client, client->console);
  VK_DestroyRend(client->rend);
  SDL_DestroyWindow(client->window);
  SDL_Quit();

  free(client);
}

void CL_LoadGlobalVariables(client_t *client, const char *config_file) {
  char section[64];
  char str[64];

  char value[64];

  char the_key[128];

  char *endptr = NULL;

  for (unsigned s = 0; ini_getsection(s, section, sizeof(section), config_file) > 0; s++) {
    for (unsigned k = 0; ini_getkey(section, k, str, sizeof(section), config_file) > 0; k++) {
      sprintf(&the_key[0], "%s$%s", section, str);

      zpl_u64 key = zpl_fnv64(the_key, strlen(the_key));

      ini_gets(section, str, "", value, sizeof(value), config_file);

      errno = 0;

      long integer = strtol(value, &endptr, 10);

      if (errno == 0 && !*endptr) {
        // it's an integer!
        CL_Integers_set(&client->global_variable_ints, key, integer);
      } else {
        errno = 0;

        float number = strtof(value, &endptr);

        if (errno == 0 && !*endptr) {
          // it's a float!
          CL_Floats_set(&client->global_variable_floats, key, number);
        } else {
          // probably a string
          short_string_t short_str;
          strcpy(short_str.str, value);
          CL_Strings_set(&client->global_variable_strings, key, short_str);
        }
      }

      short_string_t str_key;
      strcpy(&str_key.str[0], the_key);
      CL_Strings_set(&client->global_variable_keys, key, str_key);
    }
  }
}

void CL_DumpGlobalVariables(client_t *client, const char *prefix, const char *config_file) {
  for (unsigned i = 0; i < zpl_array_count(client->global_variable_floats.entries); i++) {
    short_string_t *str_key = CL_Strings_get(&client->global_variable_keys, client->global_variable_floats.entries[i].key);

    if (strncmp(str_key->str, prefix, strlen(prefix)) == 0) {
      ini_putf(prefix, &str_key->str[0] + strlen(prefix) + 1, client->global_variable_floats.entries[i].value, config_file);
    }
  }

  for (unsigned i = 0; i < zpl_array_count(client->global_variable_strings.entries); i++) {
    short_string_t *str_key = CL_Strings_get(&client->global_variable_keys, client->global_variable_strings.entries[i].key);

    if (strncmp(str_key->str, prefix, strlen(prefix)) == 0) {
      ini_puts(prefix, &str_key->str[0] + strlen(prefix) + 1, client->global_variable_strings.entries[i].value.str, config_file);
    }
  }

  for (unsigned i = 0; i < zpl_array_count(client->global_variable_ints.entries); i++) {
    short_string_t *str_key = CL_Strings_get(&client->global_variable_keys, client->global_variable_ints.entries[i].key);

    if (strncmp(str_key->str, prefix, strlen(prefix)) == 0) {
      ini_putl(prefix, &str_key->str[0] + strlen(prefix) + 1, client->global_variable_ints.entries[i].value, config_file);
    }
  }
}

string_dict_t *CL_GetStringGlobalVariables(client_t *client) {
  return &client->global_variable_strings;
}
string_dict_t *CL_GetKeyGlobalVariables(client_t *client) {
  return &client->global_variable_keys;
}
int_dict_t *CL_GetIntegerlobalVariables(client_t *client) {
  return &client->global_variable_ints;
}
float_dict_t *CL_GetFloatGlobalVariables(client_t *client) {
  return &client->global_variable_floats;
}
