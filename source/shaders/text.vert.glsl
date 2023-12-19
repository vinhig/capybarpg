#version 450

#extension GL_GOOGLE_include_directive : enable

#include "global_ubo.glsl"

struct Character {
  vec4 color;
  vec2 pos;
  vec2 size;
  uint texture;
};

layout(set = 2, binding = 0) readonly buffer AllText { Character chars[]; };

// clang-format off
const vec2 square_pos[6] =
    vec2[6](
      vec2(-1.0f, -1.0f),
      vec2(1.0f, -1.0f),
      vec2(-1.0f, 1.0f),
      
      vec2(-1.0f, 1.0f),
      vec2(1.0f, -1.0f),
      vec2(1.0f, 1.0f));
// clang-format on

// clang-format off
const vec2 square_uv[6] =
    vec2[6](
      vec2(0.0f, 1.0f),
      vec2(1.0f, 1.0f),
      vec2(0.0f, 0.0f),
      vec2(0.0f, 0.0f),
      vec2(1.0f, 1.0f),
      vec2(1.0f, 0.0f));
// clang-format on

layout(location = 0) out vec4 o_color;
layout(location = 1) out vec2 vtx_uv;
layout(location = 2) out uint texture_id;

void main() {
  vec2 anchor_top = (square_pos[gl_VertexIndex] + vec2(1.0)) / 2.0;
  vec2 pos = anchor_top * chars[gl_InstanceIndex].size + chars[gl_InstanceIndex].pos;
  pos /= global_ubo.view_dim;
  gl_Position = vec4(pos, 0.0, 1.0);
  o_color = chars[gl_InstanceIndex].color;
  vtx_uv = square_uv[gl_VertexIndex];
  vtx_uv.y = 1 - vtx_uv.y;
  texture_id = chars[gl_InstanceIndex].texture;
}
