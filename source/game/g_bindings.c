#include <stddef.h>
// clang-format sometimes dumb
#include "game/g_game.h"
#include "qcvm/qcvm.h"
#include <client/cl_client.h>
#include <game/g_private.h>
#include <stdlib.h>
#include <string.h>

ZPL_TABLE_DECLARE(extern, string_dict_t, CL_Strings_, short_string_t)
ZPL_TABLE_DECLARE(extern, float_dict_t, CL_Floats_, float)
ZPL_TABLE_DECLARE(extern, int_dict_t, CL_Integers_, int)

void C_Rand(qcvm_t *qcvm) {
  float min = qcvm_get_parm_float(qcvm, 0);
  float max = qcvm_get_parm_float(qcvm, 1);
  float scale = ((double)rand() / (float)RAND_MAX); /* [0, 1.0] */
  float r = min + scale * (max - min);              /* [min, max] */

  qcvm_return_float(qcvm, r);
}

void C_Global_HasFloat_QC(qcvm_t *qcvm) {
  game_t *game = qcvm_get_user_data(qcvm);

  const char *name = qcvm_get_parm_string(qcvm, 0);
  zpl_u64 key = zpl_fnv64(name, strlen(name));

  qcvm_return_int(qcvm, CL_Floats_get(CL_GetFloatGlobalVariables(game->client), key) != NULL);
}

void C_Global_SetFloat_QC(qcvm_t *qcvm) {
  game_t *game = qcvm_get_user_data(qcvm);

  const char *name = qcvm_get_parm_string(qcvm, 0);
  float value = qcvm_get_parm_float(qcvm, 1);
  zpl_u64 key = zpl_fnv64(name, strlen(name));

  CL_Floats_set(CL_GetFloatGlobalVariables(game->client), key, value);

  short_string_t str_key;
  strcpy(&str_key.str[0], name);
  CL_Strings_set(CL_GetKeyGlobalVariables(game->client), key, str_key);
}

void C_Global_GetFloat_QC(qcvm_t *qcvm) {
  game_t *game = qcvm_get_user_data(qcvm);

  const char *name = qcvm_get_parm_string(qcvm, 0);
  zpl_u64 key = zpl_fnv64(name, strlen(name));

  float *the_float = CL_Floats_get(CL_GetFloatGlobalVariables(game->client), key);

  if (the_float) {
    qcvm_return_float(qcvm, *the_float);
  } else {
    qcvm_return_float(qcvm, 0.0f);
  }
}

void C_Global_HasInteger_QC(qcvm_t *qcvm) {
  game_t *game = qcvm_get_user_data(qcvm);

  const char *name = qcvm_get_parm_string(qcvm, 0);
  zpl_u64 key = zpl_fnv64(name, strlen(name));

  qcvm_return_int(qcvm, CL_Integers_get(CL_GetIntegerlobalVariables(game->client), key) != NULL);
}

void C_Global_SetInteger_QC(qcvm_t *qcvm) {
  game_t *game = qcvm_get_user_data(qcvm);

  const char *name = qcvm_get_parm_string(qcvm, 0);
  int value = qcvm_get_parm_int(qcvm, 1);
  zpl_u64 key = zpl_fnv64(name, strlen(name));

  CL_Integers_set(CL_GetIntegerlobalVariables(game->client), key, value);

  short_string_t str_key;
  strcpy(&str_key.str[0], name);
  CL_Strings_set(CL_GetKeyGlobalVariables(game->client), key, str_key);
}

void C_Global_GetInteger_QC(qcvm_t *qcvm) {
  game_t *game = qcvm_get_user_data(qcvm);

  const char *name = qcvm_get_parm_string(qcvm, 0);
  zpl_u64 key = zpl_fnv64(name, strlen(name));

  int *the_int = CL_Integers_get(CL_GetIntegerlobalVariables(game->client), key);

  if (the_int) {
    qcvm_return_int(qcvm, *the_int);
  } else {
    qcvm_return_int(qcvm, 0.0f);
  }
}

void C_Global_HasString_QC(qcvm_t *qcvm) {
  game_t *game = qcvm_get_user_data(qcvm);

  const char *name = qcvm_get_parm_string(qcvm, 0);
  zpl_u64 key = zpl_fnv64(name, strlen(name));

  qcvm_return_int(qcvm, CL_Strings_get(CL_GetStringGlobalVariables(game->client), key) != NULL);
}

void C_Global_SetString_QC(qcvm_t *qcvm) {
  game_t *game = qcvm_get_user_data(qcvm);

  const char *name = qcvm_get_parm_string(qcvm, 0);
  const char *string = qcvm_get_parm_string(qcvm, 1);
  zpl_u64 key = zpl_fnv64(name, strlen(name));

  short_string_t str;
  strcpy(str.str, string);

  CL_Strings_set(CL_GetStringGlobalVariables(game->client), key, str);

  short_string_t str_key;
  strcpy(&str_key.str[0], name);
  CL_Strings_set(CL_GetKeyGlobalVariables(game->client), key, str_key);
}

void C_Global_GetString_QC(qcvm_t *qcvm) {
  game_t *game = qcvm_get_user_data(qcvm);

  const char *name = qcvm_get_parm_string(qcvm, 0);
  zpl_u64 key = zpl_fnv64(name, strlen(name));

  const char *yo = &CL_Strings_get(CL_GetStringGlobalVariables(game->client), key)->str[0];

  qcvm_return_string(qcvm, yo);
}

void C_LoadGlobalVariables_QC(qcvm_t *qcvm) {
  const char *config_file = qcvm_get_parm_string(qcvm, 0);

  game_t *game = qcvm_get_user_data(qcvm);

  CL_LoadGlobalVariables(game->client, config_file);
}

void C_DumpGlobalVariables_QC(qcvm_t *qcvm) {
  const char *prefix = qcvm_get_parm_string(qcvm, 0);
  const char *config_file = qcvm_get_parm_string(qcvm, 1);

  game_t *game = qcvm_get_user_data(qcvm);

  CL_DumpGlobalVariables(game->client, prefix, config_file);
}

void G_CommonInstall(qcvm_t *qcvm) {
  qcvm_export_t export_C_Rand = {
      .func = C_Rand,
      .name = "C_Rand",
      .argc = 2,
      .args[0] = {.name = "min", .type = QCVM_FLOAT},
      .args[1] = {.name = "max", .type = QCVM_FLOAT},
  };

  qcvm_export_t export_C_Global_HasFloat = {
      .func = C_Global_HasFloat_QC,
      .name = "C_Global_HasFloat",
      .argc = 2,
      .args[0] = {.name = "key", .type = QCVM_STRING},
      .type = QCVM_INT,
  };

  qcvm_export_t export_C_Global_GetFloat = {
      .func = C_Global_GetFloat_QC,
      .name = "C_Global_GetFloat",
      .argc = 1,
      .args[0] = {.name = "key", .type = QCVM_STRING},
  };

  qcvm_export_t export_C_Global_SetFloat = {
      .func = C_Global_SetFloat_QC,
      .name = "C_Global_SetFloat",
      .argc = 2,
      .args[0] = {.name = "key", .type = QCVM_STRING},
      .args[1] = {.name = "value", .type = QCVM_FLOAT},
  };

  qcvm_export_t export_C_Global_HasInteger = {
      .func = C_Global_HasInteger_QC,
      .name = "C_Global_HasInteger",
      .argc = 1,
      .args[0] = {.name = "key", .type = QCVM_STRING},
      .type = QCVM_INT,
  };

  qcvm_export_t export_C_Global_GetInteger = {
      .func = C_Global_GetInteger_QC,
      .name = "C_Global_GetInteger",
      .argc = 1,
      .args[0] = {.name = "key", .type = QCVM_STRING},
      .type = QCVM_INT,
  };

  qcvm_export_t export_C_Global_SetInteger = {
      .func = C_Global_SetInteger_QC,
      .name = "C_Global_SetInteger",
      .argc = 2,
      .args[0] = {.name = "key", .type = QCVM_STRING},
      .args[1] = {.name = "value", .type = QCVM_INT},
  };

  qcvm_export_t export_C_Global_HasString = {
      .func = C_Global_HasString_QC,
      .name = "C_Global_HasString",
      .argc = 1,
      .args[0] = {.name = "key", .type = QCVM_STRING},
      .type = QCVM_INT,
  };

  qcvm_export_t export_C_Global_GetString = {
      .func = C_Global_GetString_QC,
      .name = "C_Global_GetString",
      .argc = 1,
      .args[0] = {.name = "key", .type = QCVM_STRING},
      .type = QCVM_STRING,
  };

  qcvm_export_t export_C_Global_SetString = {
      .func = C_Global_SetString_QC,
      .name = "C_Global_SetString",
      .argc = 2,
      .args[0] = {.name = "key", .type = QCVM_STRING},
      .args[1] = {.name = "value", .type = QCVM_STRING},
  };

  qcvm_export_t export_C_DumpGlobalVariables = {
      .func = C_DumpGlobalVariables_QC,
      .name = "C_DumpGlobalVariables",
      .argc = 2,
      .args[0] = {.name = "prefix", .type = QCVM_STRING},
      .args[1] = {.name = "config_file", .type = QCVM_STRING},
  };

  qcvm_export_t export_C_LoadGlobalVariables = {
      .func = C_LoadGlobalVariables_QC,
      .name = "C_LoadGlobalVariables",
      .argc = 1,
      .args[0] = {.name = "config_file", .type = QCVM_STRING},
  };

  qcvm_add_export(qcvm, &export_C_Rand);

  qcvm_add_export(qcvm, &export_C_Global_HasFloat);
  qcvm_add_export(qcvm, &export_C_Global_GetFloat);
  qcvm_add_export(qcvm, &export_C_Global_SetFloat);

  qcvm_add_export(qcvm, &export_C_Global_HasInteger);
  qcvm_add_export(qcvm, &export_C_Global_GetInteger);
  qcvm_add_export(qcvm, &export_C_Global_SetInteger);

  qcvm_add_export(qcvm, &export_C_Global_HasString);
  qcvm_add_export(qcvm, &export_C_Global_GetString);
  qcvm_add_export(qcvm, &export_C_Global_SetString);

  qcvm_add_export(qcvm, &export_C_LoadGlobalVariables);
  qcvm_add_export(qcvm, &export_C_DumpGlobalVariables);
}
