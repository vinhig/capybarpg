#include <shady.h>

location(0) output vec2 vtx_uv;

private vec2 square_pos[6] = {
    (vec2){-1.0f, -1.0f}, (vec2){1.0f, -1.0f}, (vec2){-1.0f, 1.0f},
    (vec2){-1.0f, 1.0f},  (vec2){1.0f, -1.0f}, (vec2){1.0f, 1.0f},
};

private vec2 square_uv[6] = {
    (vec2){0.0f, 1.0f}, (vec2){1.0f, 1.0f}, (vec2){0.0f, 0.0f},
    (vec2){0.0f, 0.0f}, (vec2){1.0f, 1.0f}, (vec2){1.0f, 0.0f},
};

// 

// typedef struct Transform {
//   vec4 pos;
//   vec4 scale;
// } Transform;

// push_constant Transform transform;

vertex_shader void main() {
  gl_Position = (vec4){
      (float)gl_VertexIndex,
      square_pos[gl_VertexIndex].y,
      0.5f,
      1.0f
  };

  vtx_uv = square_uv[gl_VertexIndex];
}

// #include <shady.h>

// descriptor_set(0) descriptor_binding(1) uniform sampler2D texSampler;

// location(0) input vec3 vertexColor;
// location(1) input vec2 texCoord;

// location(0) output vec4 outColor;

// fragment_shader void main() {
//     vec4 fragColor;
//     fragColor.xyz = vertexColor;
//     fragColor.w = 1.0f;
//     outColor = texture2D(texSampler, texCoord) * fragColor;
// }
