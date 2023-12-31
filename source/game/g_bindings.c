#include <stddef.h>
// clang-format sometimes dumb
#include "qcvm/qcvm.h"
#include <stdio.h>
#include <stdlib.h>

void C_Rand(qcvm_t *qcvm) {
  float min = qcvm_get_parm_float(qcvm, 0);
  float max = qcvm_get_parm_float(qcvm, 1);
  float scale = ((double)rand() / (float)RAND_MAX); /* [0, 1.0] */
  float r = min + scale * (max - min);              /* [min, max] */

  qcvm_return_float(qcvm, r);
}

void G_CommonInstall(qcvm_t *qcvm) {
  qcvm_export_t export_C_Rand = {
      .func = C_Rand,
      .name = "C_Rand",
      .argc = 2,
      .args[0] = {.name = "min", .type = QCVM_FLOAT},
      .args[1] = {.name = "max", .type = QCVM_FLOAT}};

  qcvm_add_export(qcvm, &export_C_Rand);
}
