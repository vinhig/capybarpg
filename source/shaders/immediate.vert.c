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

typedef struct Transform {
  vec4 pos;
  vec4 scale;
} Transform;

push_constant Transform transform;

vertex_shader void main() {
  gl_Position = (vec4){
      square_pos[gl_VertexIndex].x * transform.scale.x + transform.pos.x,
      square_pos[gl_VertexIndex].y * transform.scale.y + transform.pos.y,
      transform.pos.z,
      1.0f
  };

  vtx_uv = square_uv[gl_VertexIndex];
}