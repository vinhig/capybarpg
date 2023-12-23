#version 450

#extension GL_GOOGLE_include_directive : enable

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

layout(location = 0) out vec2 vtx_uv;

layout(push_constant) uniform Transform {
  vec4 pos;
  vec4 scale;
};

void main() {
  gl_Position =
      vec4((square_pos[gl_VertexIndex] * scale.xy) + (pos.xy), pos.z, 1.0);
  vtx_uv = square_uv[gl_VertexIndex];
}
