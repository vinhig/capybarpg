#include "SDL_keyboard.h"
#include "cimgui.h"
#include "client/cl_client.h"
#include "qcvm.h"
#include <SDL2/SDL.h>
#include <game/g_private.h>
#include <stdio.h>
#include <string.h>

void UI_Begin_Menu_QC(qcvm_t *qcvm) {
  game_t *game = qcvm_get_user_data(qcvm);

  unsigned screen_width, screen_height;
  CL_GetViewDim(game->client, &screen_width, &screen_height);

  const char *style = qcvm_get_parm_string(qcvm, 0);
  strcpy(&game->current_ui_style[0], style);

  ImGuiWindowFlags flags = 0;

  if (strcmp(style, "main_menu") == 0) {
    ImGui_SetNextWindowBgAlpha(0.0f);
    ImGui_SetNextWindowSize((ImVec2){256.0, 1000.0}, ImGuiCond_Once);
    ImGui_SetNextWindowPos((ImVec2){64, ((float)screen_height / 2.0) - (400.0 / 2.0)}, ImGuiCond_Once);

    flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoSavedSettings;

    ImGui_PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui_Begin(style, NULL, flags);
    ImGui_PopStyleVar();
  } else if (strcmp(style, "settings_menu") == 0) {
    ImGui_SetNextWindowBgAlpha(0.4f);
    ImGui_SetNextWindowSize((ImVec2){(float)screen_width / 2.0, 400.0f}, ImGuiCond_Once);
    ImGui_SetNextWindowPos((ImVec2){400.0f, ((float)screen_height / 2.0) - (400.0f / 2.0)}, ImGuiCond_Once);

    flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysVerticalScrollbar;

    ImGui_PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui_Begin(style, NULL, flags);
    ImGui_PopStyleVar();
  } else if (strcmp(style, "settings_menu_apply") == 0) {
    ImGui_SetNextWindowBgAlpha(0.4f);
    ImGui_SetNextWindowSize((ImVec2){(float)screen_width / 2.0, 64.0f}, ImGuiCond_Once);
    ImGui_SetNextWindowPos((ImVec2){400.0f, ((float)screen_height / 2.0) - (400.0f / 2.0) + 400.0f}, ImGuiCond_Once);

    flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar;

    ImGui_PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui_Begin(style, NULL, flags);
    ImGui_PopStyleVar();
  } else {
    ImGui_Begin("don't care", NULL, flags);
  }
}

void UI_End_Menu_QC(qcvm_t *qcvm) {
  game_t *game = qcvm_get_user_data(qcvm);

  ImGui_End();

  strcpy(&game->current_ui_style[0], "");
}

void UI_Text_QC(qcvm_t *qcvm) {
  const char *label = qcvm_get_parm_string(qcvm, 0);

  ImGui_Text("%s", label);
}

void UI_Subtitle_QC(qcvm_t *qcvm) {
  const char *label = qcvm_get_parm_string(qcvm, 0);

  float width = ImGui_GetContentRegionAvail().x;
  float text_width = ImGui_CalcTextSize(label).x;
  ImGui_SetCursorPosX((width - text_width) * 0.5f);
  ImGui_Text("%s", label);
}

void UI_Button_QC(qcvm_t *qcvm) {
  const char *label = qcvm_get_parm_string(qcvm, 0);
  int enabled = qcvm_get_parm_int(qcvm, 1);

  ImGui_BeginDisabled(!enabled);

  if (!enabled) {
    ImGui_PushStyleVar(ImGuiStyleVar_Alpha, 0.30f);
  }

  game_t *game = qcvm_get_user_data(qcvm);

  bool clicked = false;

  if (strcmp(game->current_ui_style, "main_menu") == 0) {
    clicked = ImGui_ButtonEx(label, (ImVec2){ImGui_GetContentRegionAvail().x, 56.0f});
  } else if (strcmp(game->current_ui_style, "settings_menu") == 0) {
    clicked = ImGui_ButtonEx(label, (ImVec2){ImGui_GetContentRegionAvail().x, 32.0f});
  } else if (strcmp(game->current_ui_style, "settings_menu_apply") == 0) {
    ImGui_SameLine();
    ImGui_SetCursorPosY(16.0f);
    clicked = ImGui_Button(label);
  } else {
    clicked = ImGui_Button(label);
  }

  if (!enabled) {
    ImGui_PopStyleVar();
  }

  ImGui_EndDisabled();

  qcvm_return_int(qcvm, clicked);
}

void UI_CheckBox_QC(qcvm_t *qcvm) {
  const char *label = qcvm_get_parm_string(qcvm, 0);
  bool value = qcvm_get_parm_int(qcvm, 1);

  game_t *game = qcvm_get_user_data(qcvm);

  if (strcmp(game->current_ui_style, "settings_menu") == 0) {
    int total_w = ImGui_GetContentRegionAvail().x;
    ImGui_Text("%s", label);
    ImGui_SameLineEx(total_w - 32.0f, 0.0f);

    char id[64];
    sprintf(&id[0], "##%s", label);
    ImGui_Checkbox(&id[0], &value);
  } else {
    ImGui_Checkbox(label, &value);
  }

  qcvm_return_int(qcvm, value);
}

void UI_Space_QC(qcvm_t *qcvm) {
  ImGui_Dummy((ImVec2){0.0f, 32.0f});
}

void UI_Begin_Select_QC(qcvm_t *qcvm) {
  game_t *game = qcvm_get_user_data(qcvm);

  const char *label = qcvm_get_parm_string(qcvm, 0);
  const char *original_value = qcvm_get_parm_string(qcvm, 1);

  char id[32];
  sprintf(&id[0], "##%s", label);

  bool r = false;

  if (strcmp(game->current_ui_style, "settings_menu") == 0) {
    int total_w = ImGui_GetContentRegionAvail().x;
    ImGui_Text("%s", label);
    ImGui_SameLineEx(total_w / 2.0, 0.0);
    ImGui_SetNextItemWidth(total_w / 2.0);

    r = ImGui_BeginCombo(&id[0], original_value, ImGuiComboFlags_None);
  } else {
    r = ImGui_BeginCombo(label, original_value, ImGuiComboFlags_None);
  }

  qcvm_return_int(qcvm, r);
}

void UI_Option_QC(qcvm_t *qcvm) {
  const char *label = qcvm_get_parm_string(qcvm, 0);
  bool r = ImGui_Selectable(label);

  qcvm_return_int(qcvm, r);
}

void UI_End_Select_QC(qcvm_t *qcvm) {
  ImGui_EndCombo();
}

void UI_GetKeyFromName_QC(qcvm_t *qcvm) {
  const char *str = qcvm_get_parm_string(qcvm, 0);
  SDL_KeyCode code = SDL_GetKeyFromName(str);

  qcvm_return_int(qcvm, code);
}

void UI_GetNameFromKey_QC(qcvm_t *qcvm) {
  SDL_KeyCode code = qcvm_get_parm_int(qcvm, 0);
  const char *str = SDL_GetKeyName(code);

  qcvm_return_string(qcvm, str);
}

SDL_KeyCode UI_WaitForKeyPress() {
  SDL_Event event;
  while (true) {
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_KEYUP) {
        return event.key.keysym.sym;
      }
    }
  }
}

void UI_Keybinding_Begin_QC(qcvm_t *qcvm) {
  const char *label = qcvm_get_parm_string(qcvm, 0);
  ImGui_Text("%s", label);

  ImGui_PushStyleVarImVec2(ImGuiStyleVar_ButtonTextAlign, (ImVec2){0.5f, 0.5f});
}

void UI_Keybinding_End_QC(qcvm_t *qcvm) {
  ImGui_PopStyleVar();
}

void UI_Keybinding_Primary_QC(qcvm_t *qcvm) {
  const char *primary = qcvm_get_parm_string(qcvm, 0);

  int primary_key = 0;

  char id[64];
  sprintf(&id[0], "%s##primary", primary);

  float total_width = ImGui_GetContentRegionAvail().x;
  ImGui_SameLineEx(total_width / 2.0, 3.0);

  if (ImGui_ButtonEx(id, (ImVec2){total_width / 4.0 - 3.0, 32.0f})) {
    primary_key = UI_WaitForKeyPress();
  }

  qcvm_return_int(qcvm, primary_key);
}

void UI_Keybinding_Secondary_QC(qcvm_t *qcvm) {
  const char *secondary = qcvm_get_parm_string(qcvm, 0);

  int secondary_key = 0;

  char id[64];
  sprintf(&id[0], "%s##secondary", secondary);

  float total_width = ImGui_GetContentRegionAvail().x;
  ImGui_SameLineEx((total_width / 4.0) * 3.0, 3.0);

  if (ImGui_ButtonEx(id, (ImVec2){total_width / 4.0 - 3.0, 32.0f})) {
    secondary_key = UI_WaitForKeyPress();
  }

  qcvm_return_int(qcvm, secondary_key);
}

void C_Quit_QC(qcvm_t *qcvm) {
  game_t *game = qcvm_get_user_data(qcvm);

  CL_ExitClient(game->client);
}

void C_Restart_QC(qcvm_t *qcvm) {
  game_t *game = qcvm_get_user_data(qcvm);

  CL_RestartClient(game->client);
}

void G_UIInstall(qcvm_t *qcvm) {
  qcvm_export_t export_UI_Begin_Menu = {
      .func = UI_Begin_Menu_QC,
      .name = "UI_Begin_Menu",
      .argc = 1,
      .args[0] = {.name = "style", .type = QCVM_STRING},
  };

  qcvm_export_t export_UI_End_Menu = {
      .func = UI_End_Menu_QC,
      .name = "UI_End_Menu",
      .argc = 0,
  };

  qcvm_export_t export_UI_Text = {
      .func = UI_Text_QC,
      .name = "UI_Text",
      .argc = 1,
      .args[0] = {.name = "text", .type = QCVM_STRING},
  };

  qcvm_export_t export_UI_Subtitle = {
      .func = UI_Subtitle_QC,
      .name = "UI_Subtitle",
      .argc = 1,
      .args[0] = {.name = "text", .type = QCVM_STRING},
  };

  qcvm_export_t export_UI_Button = {
      .func = UI_Button_QC,
      .name = "UI_Button",
      .argc = 2,
      .args[0] = {.name = "text", .type = QCVM_STRING},
      .args[1] = {.name = "enabled", .type = QCVM_INT},
  };

  qcvm_export_t export_UI_CheckBox = {
      .func = UI_CheckBox_QC,
      .name = "UI_CheckBox",
      .argc = 2,
      .args[0] = {.name = "text", .type = QCVM_STRING},
      .args[1] = {.name = "original_value", .type = QCVM_INT},
  };

  qcvm_export_t export_UI_Space = {
      .func = UI_Space_QC,
      .name = "UI_Space",
      .argc = 0,
  };

  qcvm_export_t export_C_Quit = {
      .func = C_Quit_QC,
      .name = "C_Quit",
      .argc = 0,
  };

    qcvm_export_t export_C_Restart = {
      .func = C_Restart_QC,
      .name = "C_Restart",
      .argc = 0,
  };

  qcvm_export_t export_UI_Begin_Select = {
      .func = UI_Begin_Select_QC,
      .name = "UI_Begin_Select",
      .argc = 2,
      .args[0] = {.name = "text", .type = QCVM_STRING},
      .args[1] = {.name = "original_value", .type = QCVM_INT},
      .type = QCVM_INT,
  };

  qcvm_export_t export_UI_Option = {
      .func = UI_Option_QC,
      .name = "UI_Option",
      .argc = 1,
      .args[0] = {.name = "text", .type = QCVM_STRING},
      .type = QCVM_INT,
  };

  qcvm_export_t export_UI_End_Select = {
      .func = UI_End_Select_QC,
      .name = "UI_End_Select",
      .argc = 0,
  };

  qcvm_export_t export_UI_Keybinding_Begin = {
      .func = UI_Keybinding_Begin_QC,
      .name = "UI_Keybinding_Begin",
      .argc = 1,
      .args[0] = {.name = "text", .type = QCVM_STRING},
      .type = QCVM_VOID,
  };

  qcvm_export_t export_UI_Keybinding_Primary = {
      .func = UI_Keybinding_Primary_QC,
      .name = "UI_Keybinding_Primary",
      .argc = 1,
      .args[0] = {.name = "text", .type = QCVM_STRING},
      .type = QCVM_INT,
  };

  qcvm_export_t export_UI_Keybinding_Secondary = {
      .func = UI_Keybinding_Secondary_QC,
      .name = "UI_Keybinding_Secondary",
      .argc = 1,
      .args[0] = {.name = "text", .type = QCVM_STRING},
      .type = QCVM_INT,
  };

  qcvm_export_t export_UI_Keybinding_End = {
      .func = UI_Keybinding_End_QC,
      .name = "UI_Keybinding_End",
      .argc = 0,
      .type = QCVM_VOID,
  };

  qcvm_export_t export_UI_GetKeyFromName = {
      .func = UI_GetKeyFromName_QC,
      .name = "UI_GetKeyFromName",
      .argc = 1,
      .args[0] = {.name = "name", .type = QCVM_STRING},
  };

  qcvm_export_t export_UI_GetNameFromKey = {
      .func = UI_GetNameFromKey_QC,
      .name = "UI_GetNameFromKey",
      .argc = 1,
      .args[0] = {.name = "key", .type = QCVM_INT},
  };

  qcvm_add_export(qcvm, &export_UI_Begin_Menu);
  qcvm_add_export(qcvm, &export_UI_End_Menu);
  qcvm_add_export(qcvm, &export_UI_Text);
  qcvm_add_export(qcvm, &export_UI_Subtitle);
  qcvm_add_export(qcvm, &export_UI_Button);
  qcvm_add_export(qcvm, &export_UI_CheckBox);
  qcvm_add_export(qcvm, &export_UI_Space);
  qcvm_add_export(qcvm, &export_C_Quit);
  qcvm_add_export(qcvm, &export_C_Restart);
  qcvm_add_export(qcvm, &export_UI_Begin_Select);
  qcvm_add_export(qcvm, &export_UI_Option);
  qcvm_add_export(qcvm, &export_UI_End_Select);
  qcvm_add_export(qcvm, &export_UI_Keybinding_Begin);
  qcvm_add_export(qcvm, &export_UI_Keybinding_Primary);
  qcvm_add_export(qcvm, &export_UI_Keybinding_Secondary);
  qcvm_add_export(qcvm, &export_UI_Keybinding_End);
  qcvm_add_export(qcvm, &export_UI_GetKeyFromName);
  qcvm_add_export(qcvm, &export_UI_GetNameFromKey);
}
