#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "client/cl_client.h"
#include "common/c_profiler.h"
#include "game/g_game.h"

#define VERSION "0.1"

int main(int argc, char **argv) {
  printf("Creating client using Maidenless Engine `%s`.\n", VERSION);

  C_ProfilerInit();

  // Keep the client description as default, so the config file/command line arguments
  // can overwrite it easy
  client_desc_t desc = client_desc_default();

  CL_ParseClientDesc(&desc, argc, argv);

  client_t *client = CL_CreateClient("Maidenless Engine", &desc);

  if (!client) {
    printf("Couldn't create a client. Check error log for details.\n");
    return -1;
  }

  if (strlen(desc.game) == 0) {
    printf("No game specified. Defaulting to 'Example'.\n");
    desc.game = "../base";
  }

  game_t *game = G_CreateGame(client, desc.game);

  if (!game) {
    printf("Couldn't create a game. Check error log for details.\n");
    CL_DestroyClient(client);
    return -1;
  }

  if (!G_Load(client, game)) {
    printf("Couldn't load the main scene. Check error log for details.\n");
    CL_DestroyClient(client);
    return -1;
  }

  game_state_t *state;
  unsigned frame = 0;

  while ((CL_GetClientState(client) != CLIENT_DESTROYING) &&
         CL_GetClientState(client) != CLIENT_QUITTING) {
    if (CL_GetClientState(client) != CLIENT_PAUSED || frame == 0) {
      state = G_TickGame(client, game);
    }
    CL_DrawClient(client, game, state);
    CL_UpdateClient(client);
    frame++;
  }
  G_DestroyGame(game);
  CL_DestroyClient(client);

  printf(
      "Exiting client. Thx for using the Maidenless Engine `%s`. Maybe you'll "
      "find your maiden one day.\n",
      VERSION);

  printf("%s\n", argv[0]);

  return 0;
}
