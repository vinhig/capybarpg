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

layout(location = 3) flat out uint draw_state;

layout(location = 4) flat out uint stack_count;
layout(location = 5) flat out uint stack_textures[4];

layout(std430, set = 3, binding = 0) readonly buffer Visibles {
  uint visibles[];
};

layout(push_constant) uniform DrawState { uint uber_draw_state; };

int wall_signature(int t, int b, int l, int r) {
  return t << 4 | b << 3 | l << 2 | r << 1;
}

int get_idx(int instance_x, int instance_y) {
  return instance_y * int(global_ubo.map_width) + instance_x;
}

// int get_wall_texture(int instance_x, int instance_y) {
//   int top = int(tiles[get_idx(instance_x, instance_y - 1)].wall);
//   int bottom = int(tiles[get_idx(instance_x, instance_y + 1)].wall);
//   int left = int(tiles[get_idx(instance_x - 1, instance_y)].wall);
//   int right = int(tiles[get_idx(instance_x + 1, instance_y)].wall);

//   int wall_sig = wall_signature(top, bottom, left, right);

//   switch (wall_sig) {
//   case 1 << 1:
//     return 0 + 17;
//   case 0:
//     return 1 + 17;
//   case 1 << 2:
//     return 2 + 17;
//   case 1 << 4:
//     return 3 + 17;
//   case 1 << 3:
//     return 4 + 17;
//   case 1 << 4 | 1 << 3 | 1 << 2 | 1 << 1:
//     return 5 + 17;
//   case 1 << 3 | 1 << 2 | 1 << 1:
//     return 6 + 17;
//   case 1 << 2 | 1 << 1:
//     return 7 + 17;
//   case 1 << 4 | 1 << 2 | 1 << 1:
//     return 8 + 17;
//   case 1 << 4 | 1 << 3:
//     return 9 + 17;
//   case 1 << 4 | 1 << 1:
//     return 10 + 17;
//   case 1 << 2 | 1 << 4:
//     return 11 + 17;
//   case 1 << 1 | 1 << 3:
//     return 12 + 17;
//   case 1 << 2 | 1 << 3:
//     return 13 + 17;
//   case 1 << 4 | 1 << 3 | 1 << 2:
//     return 14 + 17;
//   case 1 << 4 | 1 << 3 | 1 << 1:
//     return 15 + 17;
//   default:
//     return 0;
//   }
// }

void main() {
  uint instance = visibles[gl_InstanceIndex];

  draw_state = uber_draw_state;

  if (uber_draw_state == 0) {
    // This is drawing a map TILE
    int instance_x = int(gl_InstanceIndex % global_ubo.map_width);
    int instance_y = int(gl_InstanceIndex / global_ubo.map_width);
    gl_Position = global_ubo.view_proj * vec4(square_pos[gl_VertexIndex] +
                                                  vec2(instance_x, instance_y),
                                              0.0, 1.0);
    vtx_uv = square_uv[gl_VertexIndex];
    gl_Position.z = 0.9999;
    if (tiles[gl_InstanceIndex].wall_texture != 0) {
      uint the_floor = tiles[gl_InstanceIndex].terrain_texture;
      uint the_wall = tiles[gl_InstanceIndex].wall_texture;

      o_albedo_id = uvec4(the_floor, the_wall, 0, 0);
    } else {
      o_albedo_id = uvec4(tiles[gl_InstanceIndex].terrain_texture, uvec3(0));
    }

    stack_count = tiles[gl_InstanceIndex].stack_count;
    stack_textures = tiles[gl_InstanceIndex].stack_textures;

  } else if (uber_draw_state == 1) {
    // This is drawing a PAWN/FURNITURE
    float flip = 1.0;

    if ((entities[instance] & immovable_signature) == immovable_signature) {
      o_albedo_id = uvec4(sprites[instance].current, 0, 0, 0);
    } else {
      if (agents[instance].direction.x != 0) {
        o_albedo_id = uvec4(sprites[instance].texture_east, 0, 0, 0);
        flip = agents[instance].direction.x;
      } else if (agents[instance].direction.y < 0.0) {
        o_albedo_id = uvec4(sprites[instance].texture_north, 0, 0, 0);
      } else {
        o_albedo_id = uvec4(sprites[instance].texture_south, 0, 0, 0);
      }
    }

    float z = float(gl_InstanceIndex + 1) / (global_ubo.entity_count + 1);

    gl_Position = global_ubo.view_proj * model_transforms[instance].model *
                  vec4(square_pos[gl_VertexIndex] * vec2(flip, 1.0), 0.03, 1.0);

    gl_Position.z = 1.0 - z;

    o_color = vec3(square_uv[gl_VertexIndex], 1.0);
    vtx_uv = square_uv[gl_VertexIndex];
  }

  vtx_uv.y = 1.0 - vtx_uv.y;
}
