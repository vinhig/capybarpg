layout(set = 0, binding = 0) uniform GlobalUbo {
  mat4 view;
  mat4 proj;
  mat4 view_proj;
  vec4 view_dir;
  vec2 view_dim;

  float min_depth;
  float max_depth;
  uint entity_count;

  uint map_width;
  uint map_height;
  vec2 map_offset;
}
global_ubo;
