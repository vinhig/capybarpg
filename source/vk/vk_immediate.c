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
                                      char *label);

bool VK_InitImmediate(vk_rend_t *rend) {
  // The immediate rendering pass use the render target of the GBuffer
  // Cause we usually use this pass to draw sprites/text and UI element
  rend->immediate = calloc(1, sizeof(vk_immediate_t));

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

  // Create graphics pipeline
  {
    VkShaderModule vertex_shader =
        VK_LoadShaderModule(rend, "immediate.vert.spv");
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
  return true;
}

void VK_DrawImmediate(vk_rend_t *rend, game_state_t *state) {
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
    // Maybe we have to create a descriptor set
    if (!((vk_texture_handle_t *)state->draws[i].handle)->set) {
      VkDescriptorSetAllocateInfo set_info = {
          .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
          .descriptorPool = rend->descriptor_pool,
          .descriptorSetCount = 1,
          .pSetLayouts = &rend->immediate->immediate_layout,
      };

      vkAllocateDescriptorSets(
          rend->device, &set_info,
          &((vk_texture_handle_t *)state->draws[i].handle)->set);

      VkDescriptorImageInfo image_info = {
          .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          .imageView =
              ((vk_texture_handle_t *)state->draws[i].handle)->image_view,
          .sampler = rend->linear_sampler,
      };

      VkWriteDescriptorSet image_write = {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstBinding = 0,
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .dstSet = ((vk_texture_handle_t *)state->draws[i].handle)->set,
          .dstBinding = 0,
          .pImageInfo = &image_info,
      };

      printf("vkUpdateDescriptorSets();\n");
      vkUpdateDescriptorSets(rend->device, 1, &image_write, 0, NULL);
    }

    vkCmdBindDescriptorSets(
        cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, rend->immediate->pipeline_layout,
        0, 1, &((vk_texture_handle_t *)state->draws[i].handle)->set, 0, NULL);

    vec4 transform[] = {
        [0] = {state->draws[i].x, state->draws[i].y, state->draws[i].z, 0.0f},
        [1] = {state->draws[i].w, state->draws[i].h, 0.0f, 0.0f},
    };

    vkCmdPushConstants(cmd, rend->immediate->pipeline_layout,
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(vec4) * 2,
                       &transform);

    vkCmdDraw(cmd, 6, 1, 0, 0);
  }

  vkCmdEndRendering(cmd);
}

void VK_DestroyImmediate(vk_rend_t *rend) {}
