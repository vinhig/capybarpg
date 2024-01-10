#pragma once

#include <stdbool.h>
#include <wchar.h>

typedef struct game_t game_t;
typedef struct game_state_t game_state_t;
typedef struct client_t client_t;
typedef struct vk_rend_t vk_rend_t;
typedef struct client_console_t client_console_t;

typedef struct client_desc_t {
  unsigned width, height;
  char *desired_gpu;
  char *game;
  bool fullscreen;
  bool only_scripting;
} client_desc_t;

typedef enum client_state_t {
  CLIENT_RUNNING,
  CLIENT_QUITTING,
  CLIENT_PAUSED,
  CLIENT_LOADING,
  CLIENT_DESTROYING,
} client_state_t;

bool CL_ParseClientDesc(client_desc_t *desc, int argc, char *argv[]);
client_t *CL_CreateClient(const char *title, client_desc_t *desc);

client_state_t CL_GetClientState(client_t *client);
void CL_SetClientState(client_t *client, client_state_t state);

vk_rend_t *CL_GetRend(client_t *client);
void CL_GetViewDim(client_t *client, unsigned *width, unsigned *height);

void CL_UpdateClient(client_t *client);

void CL_DrawClient(client_t *client, game_t* game, game_state_t *state);

void CL_DestroyClient(client_t *client);

void CL_ExitClient(client_t *client);

bool CL_InitConsole(client_t *client, client_console_t **console);
void CL_DrawConsole(client_t *client, game_t* game, game_state_t *state,
                    client_console_t *console);
void CL_DestroyConsole(client_t *client, client_console_t *console);
bool CL_ConsoleOpened(client_console_t *console);
void CL_ToggleConsole(client_console_t *console);
void CL_UpdateConsole(client_t *client, client_console_t *console);

typedef bool (*cmd_callback_t)(client_console_t *, void *user_data,
                               wchar_t args[64][64], unsigned);
typedef struct cmd_desc_t {
  wchar_t *command;
  cmd_callback_t callback;
  void* user_data;
} cmd_desc_t;

void CL_ExportCommandConsole(client_console_t *console, cmd_desc_t *desc);
