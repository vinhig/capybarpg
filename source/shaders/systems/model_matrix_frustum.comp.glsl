#version 450

layout(local_size_x = 16, local_size_y = 1, local_size_z = 1) in;

#extension GL_GOOGLE_include_directive : enable

#include "components.glsl"

layout(std140, binding = 0) buffer Entities { uint entities[]; };

layout(std140, binding = 1) buffer Transforms { Transform transform[]; };

layout(std140, binding = 2) buffer ModelTransforms {
  ModelTransform model_transform[];
};

mat4 translate(vec4 position) {
  // clang-format off
  return mat4(
    vec4(1.0, 0.0, 0.0, 0.0),
    vec4(0.0, 1.0, 0.0, 0.0),
    vec4(0.0, 0.0, 1.0, 0.0),
    vec4(position.xyz, 1.0));
  // clang-format on
}

mat4 rotate_x(float angle) {
  // clang-format off
  return mat4(
    vec4(1.0,         0.0,        0.0, 0.0),
    vec4(0.0,  cos(angle), sin(angle), 0.0),
    vec4(0.0, -sin(angle), cos(angle), 0.0),
    vec4(0.0,         0.0,        0.0, 1.0));
  // clang-format on
}

mat4 rotate_y(float angle) {
  // clang-format off
  return mat4(
    vec4(cos(angle), 0.0, -sin(angle), 0.0),
    vec4(0.0,        1.0,         0.0, 0.0),
    vec4(sin(angle), 0.0,  cos(angle), 0.0),
    vec4(0.0, 0.0, 0.0, 1.0));
  // clang-format on
}

mat4 rotate_z(float angle) {
  // clang-format off
  return mat4(
    vec4( cos(angle), sin(angle), 0.0, 0.0),
    vec4(-sin(angle), cos(angle), 0.0, 0.0),
    vec4(        0.0,        0.0, 1.0, 0.0),
    vec4(        0.0,        0.0, 0.0, 1.0));
  // clang-format on
}

mat4 scale(vec4 s) {
  // clang-format off
  return mat4(
    vec4(s.x, 0.0, 0.0, 0.0),
    vec4(0.0, s.y, 0.0, 0.0),
    vec4(0.0, 0.0, s.z, 0.0),
    vec4(0.0, 0.0, 0.0, 1.0));
  // clang-format on
}

void main() {
  uint id = gl_GlobalInvocationID.x;

  Transform the = transform[id];

  transform[id].position.x += 0.03;

  model_transform[id].model =
      translate(the.position) * rotate_x(the.rotation.x) *
      rotate_y(the.rotation.y) * rotate_z(the.rotation.z) * scale(the.scale);
}
