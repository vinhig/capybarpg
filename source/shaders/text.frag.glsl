#version 450

#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec4 foreground_color;
layout(location = 1) in vec2 vtx_uv;
layout(location = 2) in flat uint texture_id;

// output write
layout(location = 0) out vec4 o_albedo;

vec4 blend(in vec4 src, in vec4 dst) {
  return src * src.a + dst * (1.0 - src.a);
}

layout(set = 1, binding = 0) uniform usampler2D textures[];

#define PI 3.14159265359

#define SAMPLES 8
#define WIDTH 1.2
#define COLOR vec4(0.0, 0.0, 1.0, 1.0)
#define NUM_FRAMES 6.0

void main() {
  vec2 tex_coord = vtx_uv;
  uint alpha_bitmap = texture(textures[nonuniformEXT(texture_id)], tex_coord).r;

  vec4 bitmap = vec4(vec3(1.0), float(alpha_bitmap) / 256.0) * foreground_color;
  bitmap.xyz *= bitmap.a;

  o_albedo = bitmap;
  // o_albedo.a = clamp(o_albedo.a, 0.0, 1.0);
}
