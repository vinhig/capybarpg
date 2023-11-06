#version 450

layout(local_size_x = 16, local_size_y = 1, local_size_z = 1) in;

#extension GL_GOOGLE_include_directive : enable

#include "../global_ubo.glsl"

#define SET 1
#include "components.glsl"

vec2 a_star(vec2 pos, vec2 target) {
  vec2 dist = target - pos;

  return min(dist, 0.1);
}

void main() {
  uint id = gl_GlobalInvocationID.x;

  vec2 target = agents[id].target.xy;
  bool path_finding = agents[id].target.w != 0.0;

  if (path_finding) {
    // Get tile of the current pawn
    vec2 pos = transforms[id].position.xy;

    transforms[id].position.xy += a_star(pos, target);
  }
}
