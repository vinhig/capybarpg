#pragma once

typedef struct profiler_t profiler_t;

typedef enum profiler_block_name_t {
  PROFILER_BLOCK_GAME_TICK,
  PROFILER_BLOCK_SCENE_UPDATE,
  PROFILER_BLOCK_CAMERA_UPDATE,
  PROFILER_BLOCK_PATH_FINDING,
  PROFILER_BLOCK_THINK,
  PROFILER_BLOCK_VK_SYSTEM_UPDATE,
  PROFILER_BLOCK_SETUP_TILE_TEXT,

  PROFILER_BLOCK_COUNT,
} profiler_block_name_t;

void C_ProfilerInit(void);

void C_ProfilerStartBlock(profiler_block_name_t block);
void C_ProfilerEndBlock(profiler_block_name_t block);

void C_ProfilerDisplay(void);
