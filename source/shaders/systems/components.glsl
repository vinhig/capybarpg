struct Entity {
  uint signature;
  bool allocated;
};

struct Transform {
  vec4 position;
  vec4 scale;
  vec4 rotation;
};

#define transform_signature 1 << 0

struct ModelTransform {
  mat4 model;
};

#define model_transform_signature 1 << 1

struct Sprite {
  uint texture_east;
  uint texture_north;
  uint texture_south;

  uint current;
  float y_flip;
};

#define sprite_signature 1 << 2

struct Agent {
  vec4 target;
};

#define agent_signature 1 << 3
