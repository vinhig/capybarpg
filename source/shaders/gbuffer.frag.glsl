#version 450

#extension GL_EXT_nonuniform_qualifier : require

// clang-format off
const vec2 offsets_1[3] = vec2[3](
  vec2(-0.1, -0.1), vec2(0.0), vec2(0.0));
const vec2 offsets_2[3] = vec2[3](
  vec2(-0.05, -0.05),
  vec2(-0.48, -0.48), vec2(0.0));
const vec2 offsets_3[3] = vec2[3](
  vec2(-0.12, -0.12),
  vec2(-0.12, -0.87),
  vec2(-0.87, -0.66));
// clang-format on

layout(location = 0) in vec3 o_color;
layout(location = 1) in vec2 vtx_uv;
layout(location = 2) flat in uvec4 o_albedo_id;

layout(location = 3) flat in uint draw_state;

layout(location = 4) flat in uint stack_count;
layout(location = 5) flat in uint stack_textures[4];

// output write
layout(location = 0) out vec4 o_albedo;

layout(set = 1, binding = 0) uniform sampler2D textures[];

void main() {
  // For my future self -> by default the gpu thinks o_albedo_id will be uniform
  // accross the whole vkCmdDraw which spawns different pawn instances
  // However, each pawn might be drawn from a different texture, so o_albedo_id
  // has its value changed depending on the current instance, so it's not
  // uniform

  // Mixing the floor + walls
  if (draw_state == 0) {
    vec4 final_color = vec4(0.0);
    if (o_albedo_id.yzw == uvec3(0.0)) {
      vec4 color = texture(textures[nonuniformEXT(o_albedo_id.x)], vtx_uv);
      if (color.a <= 0.2) {
        color.a = 0.0;
      }
      final_color = color;
    } else {
      vec4 color_1 = texture(textures[nonuniformEXT(o_albedo_id.x)], vtx_uv);
      vec4 color_2 = texture(textures[nonuniformEXT(o_albedo_id.y)], vtx_uv);

      vec4 outcolor = color_1;
      outcolor = outcolor * (1.0 - color_2.a) + color_2;

      final_color = outcolor;
    }

    vec4 stack_color = vec4(0.0);

    vec2 offsets[3];
    float scale = 1.9;
    if (stack_count == 3) {
      offsets = offsets_3;
    } else if (stack_count == 2) {
      scale = 1.5;
      offsets = offsets_2;
    } else if (stack_count == 1) {
      scale = 1.2;
      offsets = offsets_1;
    }

    for (int i = 0; i < stack_count; i++) {
      vec2 wtf_uv = vtx_uv * scale + offsets[i];
      wtf_uv.x = clamp(wtf_uv.x, 0.0, 1.0);
      wtf_uv.y = clamp(wtf_uv.y, 0.0, 1.0);

      vec4 this_stack = texture(textures[stack_textures[i]], wtf_uv);
      stack_color = stack_color * (1.0 - this_stack.a) + this_stack;
    }

    o_albedo = final_color * (1.0 - stack_color.a) + stack_color;
  } else {
    o_albedo = texture(textures[nonuniformEXT(o_albedo_id.x)], vtx_uv);
  }

  // o_albedo = vec4(stack_count);
}
