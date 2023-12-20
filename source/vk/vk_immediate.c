#include "vk/vk_private.h"
#include "vk/vk_system.h"
#include "vk/vk_vulkan.h"

#include "game/g_game.h"
#include "vk_mem_alloc.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan_core.h>

render_target_t VK_CreateRenderTarget(vk_rend_t *rend, unsigned width,
                                      unsigned height, VkFormat format,
                                      char *label);

bool VK_InitImmediate(vk_rend_t *rend) {
  // The immediate rendering pass use the render target of the GBuffer
  // Cause we usually use this pass to draw sprites/text and UI element
  rend->immediate = calloc(1, sizeof(vk_immediate_t));
  rend->text = calloc(1, sizeof(vk_text_t));

  // Create the descriptor set layout
  // :-(
  {
    VkDescriptorSetLayoutBinding image_binding = {
        .binding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    };

    VkDescriptorSetLayoutBinding bindings[] = {image_binding};

    VkDescriptorSetLayoutCreateInfo image_set_layout = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &bindings[0],
    };

    vkCreateDescriptorSetLayout(rend->device, &image_set_layout, NULL,
                                &rend->immediate->immediate_layout);
  }

  // Create the descriptor layout, the descriptor set, and the buffer
  // responsible for holding characters data.
  {
    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(game_text_draw_t) * 1024,
        .pQueueFamilyIndices = &rend->queue_family_graphics_index,
        .queueFamilyIndexCount = 1,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
    };

    VmaAllocationCreateInfo alloc_info = {
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    };

    VmaAllocationInfo allocation = {};

    vmaCreateBuffer(rend->allocator, &buffer_info, &alloc_info,
                    &rend->text->text_buffer, &rend->text->text_alloc,
                    &allocation);

    rend->text->text_data = allocation.pMappedData;

    VkDescriptorSetLayoutBinding characters_binding = {
        .binding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
    };

    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &characters_binding,
    };

    vkCreateDescriptorSetLayout(rend->device, &layout_info, NULL,
                                &rend->text->text_layout);

    VkDescriptorBufferInfo characters_buffer_info = {
        .range = sizeof(game_text_draw_t) * 1024,
        .buffer = rend->text->text_buffer,
    };

    VkDescriptorSetAllocateInfo characters_set_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = rend->descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &rend->text->text_layout,
    };

    VkResult r = vkAllocateDescriptorSets(rend->device, &characters_set_info,
                                          &rend->text->text_set);

    if (r != VK_SUCCESS) {
      printf("[ERROR] vkAllocateDescriptorSets(rend->text->text_set) failed. "
             "Error code is %d\n",
             r);
      return false;
    }

    VkWriteDescriptorSet characters_write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstBinding = 0,
        .dstSet = rend->text->text_set,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &characters_buffer_info,
    };

    vkUpdateDescriptorSets(rend->device, 1, &characters_write, 0, NULL);
  }

  // Create graphics pipeline for immediate images. It's rendered before text
  // rendering, and uses the same render target as the gbuffer as explained
  // previously.
  {
    VkShaderModule vertex_shader =
        VK_LoadShaderModule(rend, "../test.spv");
    if (!vertex_shader) {
      printf("Couldn't create vertex shader module from "
             "`immediate.vert.spv`.\n");
      return false;
    }
    VkShaderModule fragment_shader =
        VK_LoadShaderModule(rend, "immediate.frag.spv");
    if (!fragment_shader) {
      printf("Couldn't create vertex shader module from "
             "`immediate.frag.spv`.\n");
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

    VkPushConstantRange push_constant_pos_info = {
        .offset = 0,
        .size = sizeof(vec4) * 2,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
    };

    VkDescriptorSetLayout layouts[] = {
        rend->immediate->immediate_layout,
    };

    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pPushConstantRanges = &push_constant_pos_info,
        .pushConstantRangeCount = 1,
        .pSetLayouts = &layouts[0],
        .setLayoutCount = 1,
    };

    vkCreatePipelineLayout(rend->device, &pipeline_layout_info, NULL,
                           &rend->immediate->pipeline_layout);

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
        .layout = rend->immediate->pipeline_layout,
        .pNext = &pipeline_rendering_create_info,
        .pDepthStencilState = &depth_state_info,
    };

    vkCreateGraphicsPipelines(rend->device, VK_NULL_HANDLE, 1, &pipeline_info,
                              NULL, &rend->immediate->pipeline);

    vkDestroyShaderModule(rend->device, vertex_shader, NULL);
    vkDestroyShaderModule(rend->device, fragment_shader, NULL);
  }

  {
    VkShaderModule vertex_shader = VK_LoadShaderModule(rend, "text.vert.spv");
    if (!vertex_shader) {
      printf("Couldn't create vertex shader module from "
             "`text.vert.spv`.\n");
      return false;
    }
    VkShaderModule fragment_shader = VK_LoadShaderModule(rend, "text.frag.spv");
    if (!fragment_shader) {
      printf("Couldn't create vertex shader module from "
             "`text.frag.spv`.\n");
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

    VkDescriptorSetLayout layouts[] = {
        rend->global_ubo_desc_set_layout,
        rend->global_textures_desc_set_layout,
        rend->text->text_layout,
    };

    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pSetLayouts = &layouts[0],
        .setLayoutCount = 3,
    };

    vkCreatePipelineLayout(rend->device, &pipeline_layout_info, NULL,
                           &rend->text->pipeline_layout);

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
        .layout = rend->text->pipeline_layout,
        .pNext = &pipeline_rendering_create_info,
        .pDepthStencilState = &depth_state_info,
    };

    vkCreateGraphicsPipelines(rend->device, VK_NULL_HANDLE, 1, &pipeline_info,
                              NULL, &rend->text->pipeline);

    vkDestroyShaderModule(rend->device, vertex_shader, NULL);
    vkDestroyShaderModule(rend->device, fragment_shader, NULL);
  }

  return true;
}

void VK_DrawImmediate(vk_rend_t *rend, game_state_t *state) {
  memset(rend->text->text_data, 0, sizeof(game_text_draw_t) * 1024);
  struct Character {
    vec4 color;
    vec2 pos;
    vec2 size;
    uint texture;
  };

  for (unsigned i = 0; i < state->text_count; i++) {
    ((struct Character *)rend->text->text_data)[i].texture =
        state->texts[i].tex;
    ((struct Character *)rend->text->text_data)[i].pos[0] =
        state->texts[i].pos[0];
    ((struct Character *)rend->text->text_data)[i].pos[1] =
        state->texts[i].pos[1];
    ((struct Character *)rend->text->text_data)[i].size[0] =
        state->texts[i].size[0];
    ((struct Character *)rend->text->text_data)[i].size[1] =
        state->texts[i].size[1];

    ((struct Character *)rend->text->text_data)[i].color[0] =
        state->texts[i].color[0];
    ((struct Character *)rend->text->text_data)[i].color[1] =
        state->texts[i].color[1];
    ((struct Character *)rend->text->text_data)[i].color[2] =
        state->texts[i].color[2];
    ((struct Character *)rend->text->text_data)[i].color[3] =
        state->texts[i].color[3];
  }
  VkCommandBuffer cmd = rend->graphics_command_buffer[rend->current_frame % 3];

  VkRenderingInfo render_info = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .renderArea =
          {
              .extent = {.width = rend->width, .height = rend->height},
              .offset = {.x = 0, .y = 0},
          },
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments =
          &(VkRenderingAttachmentInfo){
              .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
              .imageView = rend->gbuffer->albedo_target.image_view,
              .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
              .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
              .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
          },
      .pDepthAttachment = &rend->gbuffer->depth_target.attachment_info,
  };

  vkCmdBeginRendering(cmd, &render_info);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    rend->immediate->pipeline);

  for (unsigned i = 0; i < state->draw_count; i++) {
    vk_texture_handle_t *handle =
        &rend->immediate_handles[state->draws[i].handle];
    // Maybe we have to create a descriptor set
    if (!handle->set) {
      VkDescriptorSetAllocateInfo set_info = {
          .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
          .descriptorPool = rend->descriptor_pool,
          .descriptorSetCount = 1,
          .pSetLayouts = &rend->immediate->immediate_layout,
      };

      vkAllocateDescriptorSets(rend->device, &set_info, &handle->set);

      VkDescriptorImageInfo image_info = {
          .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          .imageView = handle->image_view,
          .sampler = rend->linear_sampler,
      };

      VkWriteDescriptorSet image_write = {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstBinding = 0,
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .dstSet = handle->set,
          .pImageInfo = &image_info,
      };

      vkUpdateDescriptorSets(rend->device, 1, &image_write, 0, NULL);
    }

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            rend->immediate->pipeline_layout, 0, 1,
                            &handle->set, 0, NULL);

    vec4 transform[] = {
        [0] = {state->draws[i].x, state->draws[i].y, state->draws[i].z, 0.0f},
        [1] = {state->draws[i].w, state->draws[i].h, 0.0f, 0.0f},
    };

    vkCmdPushConstants(cmd, rend->immediate->pipeline_layout,
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vec4) * 2,
                       &transform);

    vkCmdDraw(cmd, 6, 1, 0, 0);
  }

  {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      rend->text->pipeline);

    vkCmdBindDescriptorSets(
        cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, rend->text->pipeline_layout, 0, 1,
        &rend->global_ubo_desc_set[rend->current_frame % 3], 0, NULL);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            rend->text->pipeline_layout, 1, 1,
                            &rend->font_textures_desc_set, 0, NULL);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            rend->text->pipeline_layout, 2, 1,
                            &rend->text->text_set, 0, NULL);

    vkCmdDraw(cmd, 6, state->text_count, 0, 0);
  }

  vkCmdEndRendering(cmd);
}

void VK_DestroyImmediate(vk_rend_t *rend) {
  {
    vkDestroyPipeline(rend->device, rend->immediate->pipeline, NULL);
    vkDestroyPipelineLayout(rend->device, rend->immediate->pipeline_layout,
                            NULL);

    vkDestroyDescriptorSetLayout(rend->device,
                                 rend->immediate->immediate_layout, NULL);
  }

  {
    vkDestroyPipeline(rend->device, rend->text->pipeline, NULL);
    vkDestroyPipelineLayout(rend->device, rend->text->pipeline_layout, NULL);

    vkDestroyDescriptorSetLayout(rend->device, rend->text->text_layout, NULL);

    vmaDestroyBuffer(rend->allocator, rend->text->text_buffer,
                     rend->text->text_alloc);
  }
}
