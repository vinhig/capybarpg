#version 450

layout(local_size_x = 16, local_size_y = 1, local_size_z = 1) in;

#extension GL_GOOGLE_include_directive : enable

#include "../global_ubo.glsl"

#define SET 1
#include "components.glsl"

struct Node {
  ivec2 position;
  float g;
  float h;
  float f;
};

float h(ivec2 current, ivec2 target) {
  vec2 c = vec2(current);
  vec2 t = vec2(target);
  return sqrt((c.x - t.x, 2.0) + pow(c.y - t.y, 2.0));
}

bool is_cell_valid(vec2 position) {
  return (position.x > 0 && position.y > 0 &&
          position.x < global_ubo.map_width &&
          position.y < global_ubo.map_height);
}

vec2 a_star(vec2 pos, vec2 target) {
  vec2 dist = target - pos;

  ivec2 start = ivec2(floor(pos));
  ivec2 end = ivec2(floor(target));

  Node open_set[200];
  Node closed_set[200];

  int open_set_count = 0;
  int closed_set_count = 0;

  Node start_node;
  start_node.position = start;
  start_node.g = 0;
  start_node.h = h(start, end);
  start_node.f = start_node.g + start_node.h;

  open_set[0] = start_node;
  open_set_count++;

  int iteration = 1;

  while (open_set_count > 0) {
    iteration++;

    int current_idx = 0;
    for (int i = 0; i < open_set_count; i++) {
      if (open_set[i].f < open_set[current_idx].f) {
        current_idx = i;
      }
    }

    Node current_node = open_set[current_idx];

    for (int i = current_idx; i < open_set_count - 1; i++) {
      open_set[i] = open_set[i + 1];
    }
    open_set_count--;

    closed_set[closed_set_count] = current_node;
    closed_set_count++;

    if (current_node.position == target) {
      vec2 direction = closed_set[1].position - pos;

      vec2 s = sign(direction);
      vec2 d = min(abs(direction), 0.1 * agents[gl_GlobalInvocationID.x].speed);
      d *= s;

      return d;
    }

    ivec2 neighbors[8];
    neighbors[0] = current_node.position + ivec2(0, 1);
    neighbors[1] = current_node.position + ivec2(1, 0);
    neighbors[2] = current_node.position + ivec2(0, -1);
    neighbors[3] = current_node.position + ivec2(-1, 0);

    for (int i = 0; i < 4; i++) {
      ivec2 neighbor = neighbors[i];

      if (is_cell_valid(neighbor)) {
        float g = current_node.g + tiles[neighbor.x][neighbor.y].cost;

        bool in_closed_list = false;
        for (int j = 0; j < closed_set_count; j++) {
          if (closed_set[j].position == neighbor && g >= closed_set[j].g) {
            in_closed_list = true;
            break;
          }
        }

        if (in_closed_list) {
          continue;
        }

        bool in_open_list = false;
        for (int j = 0; j < open_set_count; j++) {
          if (open_set[j].position == neighbor && g >= open_set[j].g) {
            in_open_list = true;
            break;
          }
        }

        if (!in_open_list) {
          Node neighbor_node;
          neighbor_node.position = neighbor;
          neighbor_node.g = g;
          neighbor_node.h = h(neighbor, end);
          neighbor_node.f = neighbor_node.g + neighbor_node.h;
          open_set[open_set_count++] = neighbor_node;
        }
      }
    }
  }

  agents[gl_GlobalInvocationID.x].speed = 36.0;

  return vec2(0.0);
}

void main() {
  uint id = gl_GlobalInvocationID.x;

  vec2 target = agents[id].target.xy;
  bool path_finding = agents[id].target.w != 0.0;

  if (gl_GlobalInvocationID.x > global_ubo.entity_count) {
    return;
  }

  if (path_finding) {
    // Get tile of the current pawn
    vec2 pos = transforms[id].position.xy;

    vec2 d = a_star(pos, target);
    vec2 s = sign(d);
    transforms[id].position.xy += d;

    if (transforms[id].position.xy == target) {
      agents[id].target.w = 0.0;
    }
  }
}
