#include "vk/vk_private.h"
#include "vk/vk_system.h"
#include "vk/vk_vulkan.h"

#include "game/g_game.h"

#include <float.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <SDL2/SDL_vulkan.h>

render_target_t VK_CreateRenderTarget(vk_rend_t *rend, unsigned width,
                                      unsigned height, VkFormat format,
                                      char *label) {
  render_target_t render_target;
  VkExtent3D extent = {
      .depth = 1,
      .width = width,
      .height = height,
  };

  VkImageCreateInfo image_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = format,
      .extent = extent,
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
               VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
  };

  if (format == VK_FORMAT_D32_SFLOAT) {
    image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                       VK_IMAGE_USAGE_SAMPLED_BIT;
  }

  VmaAllocationCreateInfo alloc_info = {
      .usage = VMA_MEMORY_USAGE_GPU_ONLY,
      .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
  };

  vmaCreateImage(rend->allocator, &image_info, &alloc_info,
                 &render_target.image, &render_target.alloc, NULL);

  VkImageViewCreateInfo image_view_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .image = render_target.image,
      .format = format,
      .subresourceRange.baseMipLevel = 0,
      .subresourceRange.levelCount = 1,
      .subresourceRange.baseArrayLayer = 0,
      .subresourceRange.layerCount = 1,
      .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
  };

  if (format == VK_FORMAT_D32_SFLOAT) {
    image_view_info.subresourceRange.aspectMask =
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  }

  VkClearValue clear_color;
  if (format == VK_FORMAT_D32_SFLOAT) {
    clear_color.depthStencil.depth = 1.0;
  } else {
    clear_color.color.float32[0] = 0.0;
    clear_color.color.float32[1] = 0.0;
    clear_color.color.float32[2] = 0.0;
    clear_color.color.float32[3] = 0.0;
  }

  vkCreateImageView(rend->device, &image_view_info, NULL,
                    &render_target.image_view);

  VkRenderingAttachmentInfo attachment_info = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = render_target.image_view,
      .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = clear_color,
  };

  render_target.attachment_info = attachment_info;

  char image_name_view[1024];
  snprintf(image_name_view, 1024, "%s_view", label);

  VkDebugUtilsObjectNameInfoEXT image_name = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
      .objectType = VK_OBJECT_TYPE_IMAGE,
      .objectHandle = (uint64_t)render_target.image,
      .pObjectName = label,
  };

  VkDebugUtilsObjectNameInfoEXT image_view_name = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
      .objectType = VK_OBJECT_TYPE_IMAGE,
      .objectHandle = (uint64_t)render_target.image,
      .pObjectName = image_name_view,
  };

  rend->vkSetDebugUtilsObjectName(rend->device, &image_name);
  rend->vkSetDebugUtilsObjectName(rend->device, &image_view_name);

  return render_target;
}

bool VK_InitGBuffer(vk_rend_t *rend) {
  if (rend->gbuffer) {
    printf("GBuffer seems to be already initialized.\n");
    return false;
  }

  rend->gbuffer = calloc(1, sizeof(vk_gbuffer_t));

  render_target_t albedo = VK_CreateRenderTarget(
      rend, rend->width, rend->height, VK_FORMAT_R16G16B16A16_SFLOAT,
      "render_target_albedo");
  render_target_t depth =
      VK_CreateRenderTarget(rend, rend->width, rend->height,
                            VK_FORMAT_D32_SFLOAT, "render_target_depth");

  rend->gbuffer->albedo_target = albedo;
  rend->gbuffer->depth_target = depth;

  // Prepare render targets by setting optimal tiling
  {
    vkWaitForFences(rend->device, 1, &rend->transfer_fence, true, UINT64_MAX);
    vkResetFences(rend->device, 1, &rend->transfer_fence);

    VkImageMemoryBarrier image_memory_barrier_albedo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        // .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .image = albedo.image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        }};

    VkImageMemoryBarrier image_memory_barrier_depth = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        // .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .image = depth.image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        }};

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    vkBeginCommandBuffer(rend->transfer_command_buffer, &begin_info);

    vkCmdPipelineBarrier(rend->transfer_command_buffer,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0,
                         NULL, 0, NULL, 1, &image_memory_barrier_albedo);

    vkCmdPipelineBarrier(rend->transfer_command_buffer,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0, 0, NULL,
                         0, NULL, 1, &image_memory_barrier_depth);

    vkEndCommandBuffer(rend->transfer_command_buffer);

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &rend->transfer_command_buffer,
    };

    vkQueueSubmit(rend->graphics_queue, 1, &submit_info, rend->transfer_fence);
  }

  {
    VkShaderModule vertex_shader =
        VK_LoadShaderModule(rend, "gbuffer.vert.spv");
    if (!vertex_shader) {
      printf("Couldn't create vertex shader module from "
             "`gbuffer.vert.spv`.\n");
      return false;
    }
    VkShaderModule fragment_shader =
        VK_LoadShaderModule(rend, "gbuffer.frag.spv");
    if (!fragment_shader) {
      printf("Couldn't create vertex shader module from "
             "`gbuffer.frag.spv`.\n");
      return false;
    }

    VkPipelineShaderStageCreateInfo vertex_stage =
        VK_PipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT,
                                         vertex_shader);
    VkPipelineShaderStageCreateInfo fragment_stage =
        VK_PipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT,
                                         fragment_shader);

    VkPipelineShaderStageCreateInfo stages[2] = {
        [0] = vertex_stage,
        [1] = fragment_stage,
    };

    VkViewport viewport = {
        .height = rend->height,
        .width = rend->width,
        .x = 0,
        .y = 0,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    VkRect2D scissor = {
        .extent =
            {
                .height = rend->height,
                .width = rend->width,
            },
        .offset =
            {
                .x = 0,
                .y = 0,
            },
    };

    VkPipelineViewportStateCreateInfo viewport_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
    };

    VkPipelineColorBlendAttachmentState blend_attachment = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .alphaBlendOp = VK_BLEND_OP_ADD,
    };

    VkPipelineVertexInputStateCreateInfo input_state_info =
        VK_PipelineVertexInputStateCreateInfo();

    input_state_info.pVertexAttributeDescriptions = NULL;
    input_state_info.vertexAttributeDescriptionCount = 0;
    input_state_info.pVertexBindingDescriptions = NULL;
    input_state_info.vertexBindingDescriptionCount = 0;

    VkPipelineInputAssemblyStateCreateInfo input_assembly_info =
        VK_PipelineInputAssemblyStateCreateInfo(
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    VkPipelineRasterizationStateCreateInfo rasterization_info =
        VK_PipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL);

    VkPipelineMultisampleStateCreateInfo multisample_info =
        VK_PipelineMultisampleStateCreateInfo();

    VkPipelineColorBlendStateCreateInfo blending_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = NULL,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &blend_attachment,
        .blendConstants = {[0] = 1.0f, [1] = 1.0f, [2] = 1.0f, [3] = 1.0f},
    };

    VkPushConstantRange push_constant_info = {
        .offset = 0,
        .size = sizeof(unsigned),
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
    };

    VkDescriptorSetLayout layouts[] = {
        rend->global_ubo_desc_set_layout,
        rend->global_textures_desc_set_layout,
        rend->ecs->ecs_layout,
        rend->ecs->instance_layout,
    };

    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pPushConstantRanges = &push_constant_info,
        .pushConstantRangeCount = 1,
        .pSetLayouts = &layouts[0],
        .setLayoutCount = 4,
    };

    vkCreatePipelineLayout(rend->device, &pipeline_layout_info, NULL,
                           &rend->gbuffer->pipeline_layout);

    VkPipelineDepthStencilStateCreateInfo depth_state_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f,
        .stencilTestEnable = VK_FALSE,
    };

    VkFormat attachment_format = VK_FORMAT_R16G16B16A16_SFLOAT;

    const VkPipelineRenderingCreateInfo pipeline_rendering_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &attachment_format,
        .depthAttachmentFormat = VK_FORMAT_D32_SFLOAT,
    };

    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = &stages[0],
        .pVertexInputState = &input_state_info,
        .pInputAssemblyState = &input_assembly_info,
        .pViewportState = &viewport_info,
        .pRasterizationState = &rasterization_info,
        .pMultisampleState = &multisample_info,
        .pColorBlendState = &blending_info,
        .layout = rend->gbuffer->pipeline_layout,
        .pNext = &pipeline_rendering_create_info,
        .pDepthStencilState = &depth_state_info,
    };

    vkCreateGraphicsPipelines(rend->device, VK_NULL_HANDLE, 1, &pipeline_info,
                              NULL, &rend->gbuffer->pipeline);

    vkDestroyShaderModule(rend->device, vertex_shader, NULL);
    vkDestroyShaderModule(rend->device, fragment_shader, NULL);
  }

  return true;
}

void VK_DrawGBuffer(vk_rend_t *rend) {
  vk_gbuffer_t *gbuffer = rend->gbuffer;
  VkCommandBuffer cmd = rend->graphics_command_buffer[rend->current_frame % 3];

  VK_Gigabarrier(cmd);

  VkRenderingInfo render_info = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .renderArea =
          {
              .extent = {.width = rend->width, .height = rend->height},
              .offset = {.x = 0, .y = 0},
          },
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &rend->gbuffer->albedo_target.attachment_info,
      .pDepthAttachment = &rend->gbuffer->depth_target.attachment_info,
  };

  vkCmdBeginRendering(cmd, &render_info);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gbuffer->pipeline);

  vkCmdBindDescriptorSets(
      cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, rend->gbuffer->pipeline_layout, 0,
      1, &rend->global_ubo_desc_set[rend->current_frame % 3], 0, NULL);

  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          rend->gbuffer->pipeline_layout, 1, 1,
                          &rend->global_textures_desc_set, 0, NULL);

  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          rend->gbuffer->pipeline_layout, 2, 1,
                          &rend->ecs->ecs_set, 0, NULL);

  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          rend->gbuffer->pipeline_layout, 3, 1,
                          &rend->ecs->instance_set, 0, NULL);

  unsigned draw_state = 0; // Map
  // vkCmdPushConstants(cmd, rend->gbuffer->pipeline_layout,
                    //  VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(unsigned),
                    //  &draw_state);
  // vkCmdDraw(cmd, 6, 256 * 256, 0, 0);

  draw_state = 1; // Pawn
  vkCmdPushConstants(cmd, rend->gbuffer->pipeline_layout,
                     VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(unsigned),
                     &draw_state);
  vkCmdDraw(cmd, 6, rend->ecs->entity_count, 0, 0);

  vkCmdEndRendering(cmd);
}

void VK_DestroyGBuffer(vk_rend_t *rend) {
  vkDestroyImageView(rend->device, rend->gbuffer->depth_target.image_view,
                     NULL);
  vmaDestroyImage(rend->allocator, rend->gbuffer->depth_target.image,
                  rend->gbuffer->depth_target.alloc);

  vkDestroyImageView(rend->device, rend->gbuffer->albedo_target.image_view,
                     NULL);
  vmaDestroyImage(rend->allocator, rend->gbuffer->albedo_target.image,
                  rend->gbuffer->albedo_target.alloc);

  vkDestroyPipeline(rend->device, rend->gbuffer->pipeline, NULL);
  vkDestroyPipelineLayout(rend->device, rend->gbuffer->pipeline_layout, NULL);
}
