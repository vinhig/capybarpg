// glsl version 4.5
#version 450

#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 o_color;
layout(location = 1) in vec2 vtx_uv;
layout(location = 2) flat in uvec4 o_albedo_id;
layout(location = 3) flat in vec4 o_albedo_importance;

// output write
layout(location = 0) out vec4 o_albedo;

layout(set = 1, binding = 0) uniform sampler2D textures[];

void main() {
  // For my future self -> by default the gpu thinks o_albedo_id will be uniform
  // accross the whole vkCmdDraw which spawns different pawn instances
  // However, each pawn might be drawn from a different texture, so o_albedo_id
  // has its value changed depending on the current instance, so it's not
  // uniform
  if (o_albedo_importance == vec4(1.0, 0.0, 0.0, 0.0)) {
    vec4 color = texture(textures[nonuniformEXT(o_albedo_id.x)], vtx_uv);
    if (color.a <= 0.2) {
      color.a = 0.0;
    }
    o_albedo = color;
  } else {
    vec4 color = texture(textures[nonuniformEXT(o_albedo_id.x)], vtx_uv);
    o_albedo = color;
  }
}
