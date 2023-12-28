#pragma once

#include "cglm/types.h"
#include "vk_mem_alloc.h"

#include <stdbool.h>
#include <vulkan/vulkan_core.h>

typedef struct client_t client_t;
typedef struct vk_rend_t vk_rend_t;
typedef struct vk_system_t vk_system_t;
typedef struct game_state_t game_state_t;

typedef struct vk_write_t {
  VkBuffer src;
  VkBuffer dst;
  size_t offset;
  size_t size;
} vk_write_t;

typedef struct vertex_t {
  float pos[2];
  float uv[2];
} vertex_t;

typedef struct render_target_t {
  VkImage image;
  VkImageView image_view;
  VmaAllocation alloc;
  VkRenderingAttachmentInfo attachment_info;
} render_target_t;

struct vk_system_t {
  VkPipeline pipeline;
};

typedef struct vk_map_t {
  VkBuffer buffer;
  VmaAllocation alloc;
  VkBuffer tmp_buffer;
  VmaAllocation tmp_alloc;
  void *mapped_data;

  unsigned w, h;
} vk_map_t;

typedef struct vk_ecs_t {
  VkDescriptorSetLayout instance_layout;
  VkDescriptorSet instance_set;

  VkDescriptorSetLayout ecs_layout;
  VkDescriptorSet ecs_set;

  vk_map_t maps[16];
  unsigned map_count;
  unsigned current_map;

  VkBuffer e_buffer;
  VmaAllocation e_alloc;
  VkBuffer e_tmp_buffer;
  VmaAllocation e_tmp_alloc;
  void *entities;

  VkBuffer t_buffer;
  VmaAllocation t_alloc;
  VkBuffer mt_buffer;
  VmaAllocation mt_alloc;
  VkBuffer t_tmp_buffer;
  VmaAllocation t_tmp_alloc;
  VkBuffer mt_tmp_buffer;
  VmaAllocation mt_tmp_alloc;
  void *transforms;
  void *model_transforms;

  VkBuffer a_buffer;
  VmaAllocation a_alloc;
  VkBuffer a_tmp_buffer;
  VmaAllocation a_tmp_alloc;
  void *agents;

  VkBuffer i_buffer;
  VmaAllocation i_alloc;
  VkBuffer i_tmp_buffer;
  VmaAllocation i_tmp_alloc;
  void *immovables;

  VkBuffer s_buffer;
  VmaAllocation s_alloc;
  VkBuffer s_tmp_buffer;
  VmaAllocation s_tmp_alloc;
  void *sprites;

  VkPipelineLayout ecs_pipeline_layout;
  VkPipeline ecs_pipeline;

  VkBuffer instance_buffer;
  VmaAllocation instance_alloc;
  void *instances;

  vk_system_t systems[16];
  unsigned int system_count;

  unsigned int max_entities;
  unsigned int entity_size;
  unsigned int entity_count;

  vk_write_t *writes;
  size_t write_count;
  size_t write_size;
} vk_ecs_t;

// typedef struct vk_shading_t {
//   VkPipelineLayout pipeline_layout;
//   VkPipeline pipeline;

//   VkDescriptorSetLayout hold_layout;
//   VkDescriptorSet hold_set;

//   VkImage shading_image;
//   VkImageView shading_view;
//   VmaAllocation shading_image_alloc;
// } vk_shading_t;

typedef struct vk_gbuffer_t {
  VkPipelineLayout pipeline_layout;
  VkPipeline pipeline;

  render_target_t depth_target;
  render_target_t albedo_target;
} vk_gbuffer_t;

typedef struct vk_texture_handle_t {
  VkImage image;
  VmaAllocation image_alloc;

  VkBuffer staging;
  VmaAllocation staging_alloc;

  VkImageView image_view;

  // In case the texture should be drawn in the immediate pipeline
  // Not all textures will be
  VkDescriptorSet set;
} vk_texture_handle_t;

typedef struct vk_immediate_t {
  VkPipelineLayout pipeline_layout;
  VkPipeline pipeline;

  VkDescriptorSetLayout immediate_layout;
} vk_immediate_t;

typedef struct vk_text_t {
  VkPipelineLayout pipeline_layout;
  VkPipeline pipeline;

  VkDescriptorSetLayout text_layout;
  VkDescriptorSet text_set;
  
  VkBuffer text_buffer;
  VmaAllocation text_alloc;
  void* text_data; // mapped data
} vk_text_t;

typedef struct vk_assets_t {
  // Only GPU Visible
  VkImage *textures;
  VkImageView *texture_views;
  VmaAllocation *textures_allocs;
  // Staging textures
  VkBuffer *textures_staging;
  VmaAllocation *textures_staging_allocs;

  unsigned texture_count;
} vk_assets_t;

typedef struct vk_global_ubo_t {
  mat4 view;
  mat4 proj;
  mat4 view_proj;
  vec4 view_dir;
  vec2 view_dim;

  float min_depth;
  float max_depth;
  unsigned entity_count;

  unsigned map_width;
  unsigned map_height;
  vec2 map_offset;
} vk_global_ubo_t;

struct vk_rend_t {
  VkDebugUtilsMessengerEXT debug_messenger;
  VkInstance instance;
  VkPhysicalDevice physical_device;
  VkDevice device;
  VkQueue graphics_queue;
  // VkQueue transfer_queue;
  VkSurfaceKHR surface;
  VkSwapchainKHR swapchain;

  unsigned queue_family_graphics_index;
  // unsigned queue_family_transfer_index;

  VkFormat swapchain_format;
  VkImage *swapchain_images;
  VkImageView *swapchain_image_views;
  unsigned swapchain_image_count;
  VkSemaphore swapchain_present_semaphore[3];
  VkSemaphore swapchain_render_semaphore[3];

  // VK_Draw waits on rend->logic_fence[i]
  // VK_TickSystems waits on rend->rend_fence[i-1]
  VkFence rend_fence[3];
  VkFence logic_fence[3];
  VkFence transfer_fence;

  VkCommandPool graphics_command_pool;
  VkCommandBuffer graphics_command_buffer[3];
  VkCommandBuffer compute_command_buffer[3];
  // VkCommandPool transfer_command_pool;
  VkCommandBuffer transfer_command_buffer;
  VkDescriptorPool descriptor_pool;
  VkDescriptorPool descriptor_bindless_pool;

  VkDescriptorSetLayout global_ubo_desc_set_layout;
  VkDescriptorSet global_ubo_desc_set[3];

  VkDescriptorSetLayout global_textures_desc_set_layout;
  VkDescriptorSet map_textures_desc_set;
  VkDescriptorSet font_textures_desc_set;

  VkBuffer global_buffers[3];
  VmaAllocation global_allocs[3];

  VkSampler nearest_sampler;
  VkSampler linear_sampler;
  VkSampler anisotropy_sampler;
  VkSampler font_sampler;

  vk_gbuffer_t *gbuffer;
  vk_immediate_t *immediate;
  vk_text_t *text;
  vk_ecs_t *ecs;
  vk_assets_t map_assets;
  vk_assets_t font_assets;

  vk_texture_handle_t *immediate_handles;
  unsigned immediate_handle_count;
  unsigned immediate_handle_capacity;

  VmaAllocator allocator;

  vk_global_ubo_t global_ubo;

  VkResult (*vkSetDebugUtilsObjectName)(
      VkDevice device, const VkDebugUtilsObjectNameInfoEXT *pNameInfo);

  unsigned current_frame;

  unsigned width;
  unsigned height;
};

bool VK_InitGBuffer(vk_rend_t *rend);
void VK_DrawGBuffer(vk_rend_t *rend);
void VK_DestroyGBuffer(vk_rend_t *rend);

bool VK_InitImmediate(vk_rend_t *rend);
void VK_DrawImmediate(vk_rend_t *rend, game_state_t *state);
void VK_DestroyImmediate(vk_rend_t *rend);

void *CL_GetWindow(client_t *client);

// bool VK_InitShading(vk_rend_t *rend);
// void VK_DrawShading(vk_rend_t *rend);
// void VK_DestroyShading(vk_rend_t *rend);

VkShaderModule VK_LoadShaderModule(vk_rend_t *rend, const char *path);

void VK_Gigabarrier(VkCommandBuffer cmd);

void VK_TransitionColorTexture(VkCommandBuffer cmd, VkImage image,
                               VkImageLayout from_layout,
                               VkImageLayout to_layout,
                               VkPipelineStageFlags2 from_stage,
                               VkPipelineStageFlags2 to_stage,
                               VkAccessFlags2 src_access_mask,
                               VkAccessFlags2 dst_access_mask);
void VK_TransitionDepthTexture(VkCommandBuffer cmd, VkImage image,
                               VkImageLayout from_layout,
                               VkImageLayout to_layout,
                               VkPipelineStageFlags from_stage,
                               VkPipelineStageFlags to_stage,
                               VkAccessFlags access_mask);

static inline VkPipelineShaderStageCreateInfo
VK_PipelineShaderStageCreateInfo(VkShaderStageFlagBits stage,
                                 VkShaderModule module) {
  return (VkPipelineShaderStageCreateInfo){
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .pName = "main",
      .stage = stage,
      .module = module,
  };
}

static inline VkPipelineVertexInputStateCreateInfo
VK_PipelineVertexInputStateCreateInfo() {
  return (VkPipelineVertexInputStateCreateInfo){
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
  };
}

static inline VkPipelineInputAssemblyStateCreateInfo
VK_PipelineInputAssemblyStateCreateInfo(VkPrimitiveTopology topology) {
  return (VkPipelineInputAssemblyStateCreateInfo){
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = topology,
      .primitiveRestartEnable = false,
  };
}

static inline VkPipelineRasterizationStateCreateInfo
VK_PipelineRasterizationStateCreateInfo(VkPolygonMode polygon_mode) {
  return (VkPipelineRasterizationStateCreateInfo){
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .depthClampEnable = false,
      .rasterizerDiscardEnable = false,
      .polygonMode = polygon_mode,
      .lineWidth = 1.0f,
      .cullMode = VK_CULL_MODE_NONE,
      .frontFace = VK_FRONT_FACE_CLOCKWISE,
  };
}

static inline VkPipelineMultisampleStateCreateInfo
VK_PipelineMultisampleStateCreateInfo() {
  return (VkPipelineMultisampleStateCreateInfo){
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .sampleShadingEnable = VK_FALSE,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      .minSampleShading = 1.0f,
      .pSampleMask = NULL,
  };
}

static inline VkPipelineColorBlendAttachmentState
VK_PipelineColorBlendAttachmentState() {
  return (VkPipelineColorBlendAttachmentState){
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
      .blendEnable = VK_FALSE,
  };
}