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
layout(location = 2) flat out uvec4 o_albedo_id;
layout(location = 3) flat out vec4 o_albedo_importance;

layout(std430, set = 3, binding = 0) readonly buffer Visibles {
  uint visibles[];
};

layout(push_constant) uniform DrawState { uint draw_state; };

void main() {
  uint instance = visibles[gl_InstanceIndex];

  if (draw_state == 0) {
    int instance_x = int(gl_InstanceIndex % global_ubo.map_width);
    int instance_y = int(gl_InstanceIndex / global_ubo.map_width);
    // instance_x -= int(global_ubo.map_width) / 2;
    // instance_y -= int(global_ubo.map_height) / 2;
    gl_Position = global_ubo.view_proj * vec4(square_pos[gl_VertexIndex] +
                                                  vec2(instance_x, instance_y),
                                              0.0, 1.0);
    vtx_uv = square_uv[gl_VertexIndex];
    gl_Position.z = 0.9999;
    o_albedo_id = uvec4(tiles[instance_x][instance_y].texture, uvec3(0));
    o_albedo_importance = vec4(1.0, 0.0, 0.0, 0.0);

  } else if (draw_state == 1) {
    float z = float(gl_InstanceIndex + 1) / (global_ubo.entity_count + 1);

    // z = 0.01 + ((1.0 - 0.01) / (global_ubo.max_depth - global_ubo.min_depth))
    // *
    //                (z - global_ubo.min_depth);

    gl_Position = global_ubo.view_proj * model_transforms[instance].model *
                  vec4(square_pos[gl_VertexIndex], 0.03, 1.0);

    gl_Position.z = 1.0 - z;

    o_color = vec3(square_uv[gl_VertexIndex], 1.0);
    vtx_uv = square_uv[gl_VertexIndex];

    o_albedo_id = uvec4(sprites[instance].current, 0, 0, 0);
    o_albedo_importance = vec4(1.0, 0.0, 0.0, 0.0);
  }
}
