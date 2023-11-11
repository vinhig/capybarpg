#include <cglm/vec2.h>

void G_Rectangle(ivec2 start, ivec2 end, unsigned *indices, unsigned *count) {
  // start
  //  ----------------
  // |               |
  // |               |
  // |               |
  // -----------------
  //               end

  *count = 0;

  for (unsigned i = start[0]; i < end[0]; i++) {
    ivec2 coord = {
        [0] = i,
        [1] = start[1],
    };
    indices[*count] = coord[1] * 256 + coord[0];
    (*count)++;
  }

  for (unsigned i = start[0]; i < end[0]; i++) {
    ivec2 coord = {
        [0] = i,
        [1] = end[1],
    };
    indices[*count] = coord[1] * 256 + coord[0];
    (*count)++;
  }

  for (unsigned i = start[1]; i < end[1]; i++) {
    ivec2 coord = {
        [0] = start[0],
        [1] = i,
    };
    indices[*count] = coord[1] * 256 + coord[0];
    (*count)++;
  }

  for (unsigned i = start[1]; i < end[1]+1; i++) {
    ivec2 coord = {
        [0] = end[0],
        [1] = i,
    };
    indices[*count] = coord[1] * 256 + coord[0];
    (*count)++;
  }
}
