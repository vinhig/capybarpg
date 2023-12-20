#include <shady.h>

location(0) input vec2 vtx_uv;

location(0) output vec4 o_albedo;

descriptor_set(0) descriptor_binding(0) uniform sampler2D tex;

fragment_shader void main() {
    o_albedo = texture2D(tex, vtx_uv);
}
