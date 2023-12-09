#version 450

#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 o_color;
layout(location = 1) in vec2 vtx_uv;

// output write
layout(location = 0) out vec4 o_albedo;

layout(set = 0, binding = 0) uniform sampler2D tex;

void main() {
    o_albedo = texture(tex, vtx_uv);
}
