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
};

#define sprite_signature 1 << 2

struct Agent {
  // target.xy => target tile
  // target.w => currently path_finding
  vec2 direction;
  vec2 paddddddding;
  // bool moving;
};

#define agent_signature 1 << 3

struct Immovable {
  // Number of tiles occupied (rotation must be applied)
  // The transform's position will be anchored at the top-left tile
  vec2 size;
  vec2 paddddddding;
};

#define immovable_signature 1 << 4

struct Tile {
  uint wall;
  uint texture;

  uint selected;
  uint fire;
};

#ifndef is_C
layout(std430, set = SET, binding = 0) buffer Map { Tile tiles[256][256]; };

layout(std430, set = SET, binding = 1) buffer Entities { uint entities[]; };

layout(std140, set = SET, binding = 2) buffer Transforms {
  Transform transforms[];
};

layout(std140, set = SET, binding = 3) buffer ModelTransforms {
  ModelTransform model_transforms[];
};

layout(std430, set = SET, binding = 4) buffer Sprites { Sprite sprites[]; };

layout(std140, set = SET, binding = 5) buffer Agents { Agent agents[]; };

layout(std140, set = SET, binding = 6) buffer Immovables {
  Immovable immovables[];
};
#endif
