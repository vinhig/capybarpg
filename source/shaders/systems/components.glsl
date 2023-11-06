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
  // target.xy => target tile
  // target.w => currently path_finding
  vec4 target;
  float speed;
};

#define agent_signature 1 << 3

struct Tile {
  float speed;
  float cost;
  uint terrain;
  bool fire;
  bool selected;
};

#ifndef is_C
layout(std140, set = SET, binding = 0) buffer Map { Tile tiles[]; }
map;

layout(std140, set = SET, binding = 1) buffer Entities { uint entities[]; };

layout(std140, set = SET, binding = 2) buffer Transforms {
  Transform transforms[];
};

layout(std140, set = SET, binding = 3) buffer ModelTransforms {
  ModelTransform model_transforms[];
};


layout(std430, set = SET, binding = 4) buffer Sprites {
  Sprite sprites[];
};


layout(std140, set = SET, binding = 5) buffer Agents {
  Agent agents[];
};
#endif