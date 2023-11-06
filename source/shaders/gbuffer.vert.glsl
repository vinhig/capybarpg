#version 450

#extension GL_GOOGLE_include_directive : enable

#include "global_ubo.glsl"

#define SET 2
#include "systems/components.glsl"

// clang-format off
const vec2 square_pos[6] =
    vec2[6](
      vec2(-0.5f, -0.5f),
      vec2(0.5f, -0.5f),
      vec2(-0.5f, 0.5f),
      
      vec2(-0.5f, 0.5f),
      vec2(0.5f, -0.5f),
      vec2(0.5f, 0.5f));
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

layout(location = 0) out vec3 o_color;
layout(location = 1) out vec2 vtx_uv;
layout(location = 2) flat out uint o_albedo_id;

layout(location = 3) out vec3 vtx_position;
layout(location = 4) out vec3 vtx_normal;

layout(std430, set = 3, binding = 0) readonly buffer Visibles {
  uint visibles[];
};

layout(push_constant) uniform DrawState { uint draw_state; };

void main() {
  uint instance = visibles[gl_InstanceIndex];

  float z = float(gl_InstanceIndex + 1) / (global_ubo.entity_count + 1);

  // z = 0.01 + ((1.0 - 0.01) / (global_ubo.max_depth - global_ubo.min_depth))
  // *
  //                (z - global_ubo.min_depth);

  gl_Position = global_ubo.view_proj * model_transforms[instance].model *
                vec4(square_pos[gl_VertexIndex], 0.03, 1.0f);

  gl_Position.z = 1.0 - z;

  o_color = vec3(square_uv[gl_VertexIndex], 1.0);
  vtx_uv = square_uv[gl_VertexIndex];

  o_albedo_id = sprites[instance].current;
}
