#include <cglm/vec2.h>
#include <intlist.h>
#include <jps.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

void load_map(struct map *m, const char *path) {
  FILE *f = fopen(path, "r");
  size_t size = 0;
  fseek(f, 0, SEEK_END);
  size = ftell(f);
  fseek(f, 0, SEEK_SET);

  char *data = malloc(size);

  fread(data, 1, size, f);
  fclose(f);

  unsigned x = 0;
  unsigned y = 0;
  for (size_t i = 0; i < size; i++) {
    switch (data[i]) {
    case '#': {
      jps_set_obstacle(m, x, y, true);
      break;
    }
    case ' ': {
      jps_set_obstacle(m, x, y, false);
      break;
    }
    case '\n': {
      x = 0;
      y++;
    }
    default: {
      break;
    }
    }

    if (data[i] != '\n') {
      x++;
    }
  }
}

int main(int argc, char const *argv[]) {

  ivec2 starts[] = {
      [0] = {103, 16}, [1] = {92, 61}, [2] = {66, 33},
      [3] = {13, 15},  [4] = {15, 3},
  };

  struct map *m = jps_create(256, 256);
  time_t seed = time(NULL);
  srand(seed);

  load_map(m, "../benchmark/map1.txt");

  for (unsigned j = 0; j < 3000; j++) {
    int s = rand() % 4;
    int e = rand() % 4;
    jps_set_start(m, starts[s][0], starts[s][1]);
    jps_set_end(m, starts[e][0], starts[e][1]);

    IntList *list = il_create(2);
    if (jps_path_finding(m, 2, list) != 0) {
      printf("something went wrong... %d to %d\n", s, e);
    }
    il_destroy(list);
  }

  return 0;
}
