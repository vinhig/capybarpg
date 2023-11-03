#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "client/cl_client.h"
#include "game/g_game.h"

#define VERSION "0.1"

int main(int argc, char **argv) {
  printf("Creating client using Maidenless Engine `%s`.\n", VERSION);

  client_desc_t desc = {
      .width = 1280,
      .height = 720,
      .fullscreen = false,
      .game = "",
  };

  CL_ParseClientDesc(&desc, argc, argv);

  client_t *client = CL_CreateClient("Maidenless Engine", &desc);

  if (!client) {
    printf("Couldn't create a client. Check error log for details.\n");
    return -1;
  }

  if (strlen(desc.game) == 0) {
    printf("No game specified. Defaulting to 'Zombie Hierarchy'. Check it on "
           "Steam!\n");
    desc.game = "../base_ze";
  }

  game_t *game = G_CreateGame(client, desc.game);

  if (!game) {
    printf("Couldn't create a game. Check error log for details.\n");
    CL_DestroyClient(client);
    return -1;
  }

  CL_PushLoadingScreen(client);
  if (!G_Load(client, game)) {
    printf("Couldn't load the main scene. Check error log for details.\n");
    CL_DestroyClient(client);
    return -1;
  }
  CL_PopLoadingScreen(client);

  game_state_t state;
  unsigned frame = 0;
  
  while ((CL_GetClientState(client) != CLIENT_DESTROYING) &&
         CL_GetClientState(client) != CLIENT_QUITTING) {
    CL_UpdateClient(client);
    if (CL_GetClientState(client) != CLIENT_PAUSED || frame == 0) {
      state = G_TickGame(client, game);
    }
    CL_DrawClient(client, &state);
    frame++;
  }
  G_DestroyGame(game);
  CL_DestroyClient(client);

  printf(
      "Exiting client. Thx for using the Maidenless Engine `%s`. Maybe you'll "
      "find your maiden one day.\n",
      VERSION);

  return 0;
}
