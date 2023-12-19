#version 450

#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec4 o_color;
layout(location = 1) in vec2 vtx_uv;
layout(location = 2) in flat uint texture_id;

// output write
layout(location = 0) out vec4 o_albedo;

layout(set = 1, binding = 0) uniform usampler2D textures[];

void main() {
  uint alpha = texture(textures[nonuniformEXT(texture_id)], vtx_uv).r;
  o_albedo = vec4(vec3(1.0), float(alpha) / 256.0) * o_color;
}
