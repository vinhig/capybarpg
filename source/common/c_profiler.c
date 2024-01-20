#include <cimgui.h>
#include <common/c_profiler.h>
#include <zpl/zpl.h>

#define NUMBER_RECORDS 16

typedef struct profiler_block_t {
  zpl_f64 start;
  zpl_f64 end;

  zpl_f64 lasts[NUMBER_RECORDS];
  zpl_f64 mean;
} profiler_block_t;

typedef struct profiler_t {
  profiler_block_t blocks[PROFILER_BLOCK_COUNT];
  bool enabled;
} profiler_t;

profiler_t Global_Profiler = {.enabled = false};

void C_ProfilerInit(void) {
  Global_Profiler.enabled = true;
}

void C_ProfilerStartBlock(profiler_block_name_t block) {
  if (!Global_Profiler.enabled) {
    return;
  }

  Global_Profiler.blocks[block].start = zpl_time_rel();
}

void C_ProfilerEndBlock(profiler_block_name_t block) {
  if (!Global_Profiler.enabled) {
    return;
  }

  Global_Profiler.blocks[block].end = zpl_time_rel();

  zpl_f64 total = Global_Profiler.blocks[block].end - Global_Profiler.blocks[block].start;

  for (unsigned i = 0; i < NUMBER_RECORDS - 1; i++) {
    Global_Profiler.blocks[block].lasts[i] = Global_Profiler.blocks[block].lasts[i + 1];
  }

  Global_Profiler.blocks[block].lasts[NUMBER_RECORDS - 1] = total;

  Global_Profiler.blocks[block].mean = 0.0f;
  for (unsigned i = 0; i < NUMBER_RECORDS; i++) {
    Global_Profiler.blocks[block].mean += Global_Profiler.blocks[block].lasts[i];
  }
  Global_Profiler.blocks[block].mean /= NUMBER_RECORDS;
}

void C_ProfilerDisplay(void) {
  if (!Global_Profiler.enabled) {
    return;
  }

  ImGui_Begin("Profiler data", NULL, 0);

  float game_tick = Global_Profiler.blocks[PROFILER_BLOCK_GAME_TICK].mean;
  ImGui_Text("Game Tick: %.03fms", game_tick * 1000.0f);

  float path_finding = Global_Profiler.blocks[PROFILER_BLOCK_PATH_FINDING].mean;
  ImGui_Text("Path Finding: %.03fms", path_finding * 1000.0f);

  float agent_thinking = Global_Profiler.blocks[PROFILER_BLOCK_THINK].mean;
  ImGui_Text("Agent Thinking: %.03fms", agent_thinking * 1000.0f);

  float setup_tile = Global_Profiler.blocks[PROFILER_BLOCK_SETUP_TILE_TEXT].mean;
  ImGui_Text("Setup Tile Text: %.03fms", setup_tile * 1000.0f);

  float scene_update = Global_Profiler.blocks[PROFILER_BLOCK_SCENE_UPDATE].mean;
  ImGui_Text("Scene Update: %.03fms", scene_update * 1000.0f);

  ImGui_End();
}
