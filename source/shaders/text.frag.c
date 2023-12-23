#include <shady.h>

location(0) input vec4 o_color;
location(1) input vec2 vtx_uv;
location(2) input unsigned int texture_id;

location(0) output vec4 o_albedo;

descriptor_set(1) descriptor_binding(0) uniform extern usampler2D textures[];


void main() {
    unsigned int alpha = utexture2D(textures[texture_id], vtx_uv).r;

    vec4 c = {
        1.0, 1.0, 1.0, ((float)alpha) / 256.0,
    };

    o_albedo = c * o_color;
}