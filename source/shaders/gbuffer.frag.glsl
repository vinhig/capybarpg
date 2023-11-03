// glsl version 4.5
#version 450

#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 o_color;
layout(location = 1) in vec2 vtx_uv;
layout(location = 2) flat in uint o_albedo_id;

layout(location = 3) in vec3 vtx_position;
layout(location = 4) in vec3 vtx_normal;

// output write
layout(location = 0) out vec4 o_albedo;

layout(set = 1, binding = 0) uniform sampler2D textures[];

void main() {
  vec4 color = texture(textures[nonuniformEXT(o_albedo_id)], vtx_uv);
  o_albedo = color;
}
