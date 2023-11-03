#include "vk/vk_system.h"
#include "vk/vk_private.h"
#include "vk/vk_vulkan.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <vk_mem_alloc.h>

void VK_AddWriteECS(vk_rend_t *rend, VkBuffer src, VkBuffer dst, size_t offset,
                    size_t size) {
  if (!rend->ecs->writes) {
    rend->ecs->write_size = 16;
    rend->ecs->write_count = 0;
    rend->ecs->writes = calloc(1, rend->ecs->write_size * sizeof(vk_write_t));
  }

  if (rend->ecs->write_count >= rend->ecs->write_size) {
    printf("realloc...\n");
    rend->ecs->write_size *= 2;
    rend->ecs->writes =
        realloc(rend->ecs->writes, rend->ecs->write_size * sizeof(vk_write_t));
  }

  rend->ecs->writes[rend->ecs->write_count].src = src;
  rend->ecs->writes[rend->ecs->write_count].dst = dst;
  rend->ecs->writes[rend->ecs->write_count].offset = offset;
  rend->ecs->writes[rend->ecs->write_count].size = size;
  rend->ecs->write_count += 1;
}

bool VK_InitECS(vk_rend_t *rend, unsigned count) {
  rend->ecs = calloc(1, sizeof(vk_ecs_t));
  rend->ecs->max_entities = count;

  // Create relevant components buffer, with an arbitrary huge size
  // ENTITIES & TRANSFORMS & MODEL_TRANSFORMS & SPRITES
  {
    VkBufferCreateInfo entity_buffer = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(unsigned) * count,
        .pQueueFamilyIndices = &rend->queue_family_graphics_index,
        .queueFamilyIndexCount = 1,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    };

    VmaAllocationCreateInfo entity_allocation = {
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    };

    vmaCreateBuffer(rend->allocator, &entity_buffer, &entity_allocation,
                    &rend->ecs->e_buffer, &rend->ecs->e_alloc, NULL);

    VkDebugUtilsObjectNameInfoEXT e_buffer_name = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_BUFFER,
        .objectHandle = (uint64_t)rend->ecs->e_buffer,
        .pObjectName = "Entities Buffer",
    };
    rend->vkSetDebugUtilsObjectName(rend->device, &e_buffer_name);

    VkBufferCreateInfo transform_buffer = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(struct Transform) * count,
        .pQueueFamilyIndices = &rend->queue_family_graphics_index,
        .queueFamilyIndexCount = 1,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    };

    VmaAllocationCreateInfo transform_allocation = {
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    };

    vmaCreateBuffer(rend->allocator, &transform_buffer, &transform_allocation,
                    &rend->ecs->t_buffer, &rend->ecs->t_alloc, NULL);

    VkDebugUtilsObjectNameInfoEXT t_buffer_name = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_BUFFER,
        .objectHandle = (uint64_t)rend->ecs->t_buffer,
        .pObjectName = "Transforms Buffer",
    };
    rend->vkSetDebugUtilsObjectName(rend->device, &t_buffer_name);

    VkBufferCreateInfo model_transform_buffer = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(struct ModelTransform) * count,
        .pQueueFamilyIndices = &rend->queue_family_graphics_index,
        .queueFamilyIndexCount = 1,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    };

    VmaAllocationCreateInfo model_transform_allocation = {
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    };

    vmaCreateBuffer(rend->allocator, &model_transform_buffer,
                    &model_transform_allocation, &rend->ecs->mt_buffer,
                    &rend->ecs->mt_alloc, NULL);

    VkDebugUtilsObjectNameInfoEXT mt_buffer_name = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_BUFFER,
        .objectHandle = (uint64_t)rend->ecs->mt_buffer,
        .pObjectName = "Model Transforms Buffer",
    };
    rend->vkSetDebugUtilsObjectName(rend->device, &mt_buffer_name);

    VkBufferCreateInfo sprite_buffer = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(struct Sprite) * count,
        .pQueueFamilyIndices = &rend->queue_family_graphics_index,
        .queueFamilyIndexCount = 1,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    };

    VmaAllocationCreateInfo sprite_allocation = {
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    };

    vmaCreateBuffer(rend->allocator, &sprite_buffer, &sprite_allocation,
                    &rend->ecs->s_buffer, &rend->ecs->s_alloc, NULL);

    VkDebugUtilsObjectNameInfoEXT s_buffer_name = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_BUFFER,
        .objectHandle = (uint64_t)rend->ecs->s_buffer,
        .pObjectName = "Sprites Buffer",
    };
    rend->vkSetDebugUtilsObjectName(rend->device, &s_buffer_name);
  }

  // Create the "instance" buffer, contains depth-ordered entity that should be
  // drawn for a given frame
  {
    VkBufferCreateInfo instance_buffer = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(unsigned) * count,
        .pQueueFamilyIndices = &rend->queue_family_graphics_index,
        .queueFamilyIndexCount = 1,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
    };

    VmaAllocationCreateInfo instance_allocation = {
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    };

    VmaAllocationInfo instance_alloc_info;

    vmaCreateBuffer(rend->allocator, &instance_buffer, &instance_allocation,
                    &rend->ecs->instance_buffer, &rend->ecs->instance_alloc,
                    &instance_alloc_info);
    rend->ecs->instances = instance_alloc_info.pMappedData;
  }

  // Create the mapped buffer to write/copy to
  {
    VkBufferCreateInfo entity_tmp_buffer = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(unsigned) * count,
        .pQueueFamilyIndices = &rend->queue_family_graphics_index,
        .queueFamilyIndexCount = 1,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    };

    VmaAllocationCreateInfo entity_tmp_allocation = {
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    };

    VmaAllocationInfo entity_alloc_info;

    vmaCreateBuffer(rend->allocator, &entity_tmp_buffer, &entity_tmp_allocation,
                    &rend->ecs->e_tmp_buffer, &rend->ecs->e_tmp_alloc,
                    &entity_alloc_info);
    rend->ecs->entities = entity_alloc_info.pMappedData;

    VkDebugUtilsObjectNameInfoEXT e_tmp_buffer_name = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_BUFFER,
        .objectHandle = (uint64_t)rend->ecs->e_tmp_buffer,
        .pObjectName = "Entities Tmp Buffer",
    };
    rend->vkSetDebugUtilsObjectName(rend->device, &e_tmp_buffer_name);

    VkBufferCreateInfo transform_tmp_buffer = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(struct Transform) * count,
        .pQueueFamilyIndices = &rend->queue_family_graphics_index,
        .queueFamilyIndexCount = 1,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    };

    VmaAllocationCreateInfo transform_tmp_allocation = {
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    };

    VmaAllocationInfo transform_alloc_info;

    vmaCreateBuffer(rend->allocator, &transform_tmp_buffer,
                    &transform_tmp_allocation, &rend->ecs->t_tmp_buffer,
                    &rend->ecs->t_tmp_alloc, &transform_alloc_info);
    rend->ecs->transforms = transform_alloc_info.pMappedData;

    VkDebugUtilsObjectNameInfoEXT t_tmp_buffer_name = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_BUFFER,
        .objectHandle = (uint64_t)rend->ecs->t_tmp_buffer,
        .pObjectName = "Transforms Tmp Buffer",
    };
    rend->vkSetDebugUtilsObjectName(rend->device, &t_tmp_buffer_name);

    VkBufferCreateInfo model_transform_tmp_buffer = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(struct ModelTransform) * count,
        .pQueueFamilyIndices = &rend->queue_family_graphics_index,
        .queueFamilyIndexCount = 1,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    };

    VmaAllocationCreateInfo model_transform_tmp_allocation = {
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    };

    VmaAllocationInfo model_transform_alloc_info;

    vmaCreateBuffer(rend->allocator, &model_transform_tmp_buffer,
                    &model_transform_tmp_allocation, &rend->ecs->mt_tmp_buffer,
                    &rend->ecs->mt_tmp_alloc, &model_transform_alloc_info);
    rend->ecs->model_transforms = model_transform_alloc_info.pMappedData;

    VkDebugUtilsObjectNameInfoEXT mt_tmp_buffer_name = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_BUFFER,
        .objectHandle = (uint64_t)rend->ecs->mt_tmp_buffer,
        .pObjectName = "Model Transforms Tmp Buffer",
    };
    rend->vkSetDebugUtilsObjectName(rend->device, &mt_tmp_buffer_name);

    VkBufferCreateInfo sprite_tmp_buffer = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(struct Sprite) * count,
        .pQueueFamilyIndices = &rend->queue_family_graphics_index,
        .queueFamilyIndexCount = 1,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    };

    VmaAllocationCreateInfo sprite_tmp_allocation = {
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    };

    VmaAllocationInfo sprite_transform_alloc_info;

    vmaCreateBuffer(rend->allocator, &sprite_tmp_buffer, &sprite_tmp_allocation,
                    &rend->ecs->s_tmp_buffer, &rend->ecs->s_tmp_alloc,
                    &sprite_transform_alloc_info);
    rend->ecs->sprites = sprite_transform_alloc_info.pMappedData;

    VkDebugUtilsObjectNameInfoEXT s_tmp_buffer_name = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_BUFFER,
        .objectHandle = (uint64_t)rend->ecs->s_tmp_buffer,
        .pObjectName = "Sprites Tmp Buffer",
    };
    rend->vkSetDebugUtilsObjectName(rend->device, &s_tmp_buffer_name);
  }

  // BE AWARE, all descriptor also points to the entity buffer!!!!

  // Create descriptor set layout and the related descriptor set for systems
  // that work with transforms/model_transforms
  {
    VkDescriptorSetLayoutBinding entities = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .binding = 0,
        .descriptorCount = 1,
    };

    VkDescriptorSetLayoutBinding transforms = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .binding = 1,
        .descriptorCount = 1,
    };

    VkDescriptorSetLayoutBinding model_transforms = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .binding = 2,
        .descriptorCount = 1,
    };

    VkDescriptorSetLayoutBinding bindings[] = {entities, transforms,
                                               model_transforms};

    VkDescriptorSetLayoutCreateInfo t_layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 3,
        .pBindings = &bindings[0],
    };

    if (vkCreateDescriptorSetLayout(rend->device, &t_layout_info, NULL,
                                    &rend->ecs->t_layout) != VK_SUCCESS) {
      printf("Couldn't create descriptor layout for `t_layout`.\n");
    }

    // Allocate a single descriptor
    VkDescriptorSetAllocateInfo t_set_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = rend->descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &rend->ecs->t_layout,
    };

    if (vkAllocateDescriptorSets(rend->device, &t_set_info,
                                 &rend->ecs->t_set)) {
      printf("Couldn't create descriptor set for `t_set`.\n");
    }

    VkDescriptorBufferInfo comp_entity_buffer = {
        .buffer = rend->ecs->e_buffer,
        .offset = 0,
        .range = sizeof(unsigned) * count,
    };

    VkDescriptorBufferInfo comp_transform_buffer = {
        .buffer = rend->ecs->t_buffer,
        .offset = 0,
        .range = sizeof(struct Transform) * count,
    };

    VkDescriptorBufferInfo comp_model_transform_buffer = {
        .buffer = rend->ecs->mt_buffer,
        .offset = 0,
        .range = sizeof(struct ModelTransform) * count,
    };

    VkWriteDescriptorSet writes[3] = {
        [0] =
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = rend->ecs->t_set,
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .pBufferInfo = &comp_entity_buffer,
            },
        [1] =
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = rend->ecs->t_set,
                .dstBinding = 1,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .pBufferInfo = &comp_transform_buffer,
            },
        [2] =
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = rend->ecs->t_set,
                .dstBinding = 2,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .pBufferInfo = &comp_model_transform_buffer,
            },
    };

    vkUpdateDescriptorSets(rend->device, 3, &writes[0], 0, NULL);

    VkPipelineLayoutCreateInfo t_pipeline_layout = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &rend->ecs->t_layout,
    };

    if (vkCreatePipelineLayout(rend->device, &t_pipeline_layout, NULL,
                               &rend->ecs->t_pipeline_layout) != VK_SUCCESS) {
      printf("Couldn't create pipeline layout for `t_pipeline_layout`.\n");
    }
  }

  // Create descriptor set layout and the related descriptor set for systems
  // that work with transforms/model_transforms/sprites
  {
    VkDescriptorSetLayoutBinding entities = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .binding = 0,
        .descriptorCount = 1,
    };

    VkDescriptorSetLayoutBinding transforms = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .binding = 1,
        .descriptorCount = 1,
    };

    VkDescriptorSetLayoutBinding model_transforms = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .binding = 2,
        .descriptorCount = 1,
    };

    VkDescriptorSetLayoutBinding sprites = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .binding = 3,
        .descriptorCount = 1,
    };

    VkDescriptorSetLayoutBinding bindings[] = {entities, transforms,
                                               model_transforms, sprites};

    VkDescriptorSetLayoutCreateInfo ts_layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 4,
        .pBindings = &bindings[0],
    };

    if (vkCreateDescriptorSetLayout(rend->device, &ts_layout_info, NULL,
                                    &rend->ecs->ts_layout) != VK_SUCCESS) {
      printf("Couldn't create descriptor layout for `t_layout`.\n");
    }

    // Allocate a single descriptor
    VkDescriptorSetAllocateInfo t_set_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = rend->descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &rend->ecs->ts_layout,
    };

    if (vkAllocateDescriptorSets(rend->device, &t_set_info,
                                 &rend->ecs->ts_set)) {
      printf("Couldn't create descriptor set for `t_set`.\n");
    }

    VkDescriptorBufferInfo comp_entity_buffer = {
        .buffer = rend->ecs->e_buffer,
        .offset = 0,
        .range = sizeof(unsigned) * count,
    };

    VkDescriptorBufferInfo comp_transform_buffer = {
        .buffer = rend->ecs->t_buffer,
        .offset = 0,
        .range = sizeof(struct Transform) * count,
    };

    VkDescriptorBufferInfo comp_model_transform_buffer = {
        .buffer = rend->ecs->mt_buffer,
        .offset = 0,
        .range = sizeof(struct ModelTransform) * count,
    };

    VkDescriptorBufferInfo comp_sprite_buffer = {
        .buffer = rend->ecs->s_buffer,
        .offset = 0,
        .range = sizeof(struct Sprite) * count,
    };

    VkWriteDescriptorSet writes[4] = {
        [0] =
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = rend->ecs->ts_set,
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .pBufferInfo = &comp_entity_buffer,
            },
        [1] =
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = rend->ecs->ts_set,
                .dstBinding = 1,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .pBufferInfo = &comp_transform_buffer,
            },
        [2] =
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = rend->ecs->ts_set,
                .dstBinding = 2,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .pBufferInfo = &comp_model_transform_buffer,
            },
        [3] =
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = rend->ecs->ts_set,
                .dstBinding = 3,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .pBufferInfo = &comp_sprite_buffer,
            },
    };

    vkUpdateDescriptorSets(rend->device, 4, &writes[0], 0, NULL);

    VkPipelineLayoutCreateInfo t_pipeline_layout = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &rend->ecs->ts_layout,
    };

    if (vkCreatePipelineLayout(rend->device, &t_pipeline_layout, NULL,
                               &rend->ecs->ts_pipeline_layout) != VK_SUCCESS) {
      printf("Couldn't create pipeline layout for `t_pipeline_layout`.\n");
    }
  }

  // Create descriptor set layout and the related descriptor set for instances
  // (related to the "instance" buffer that contains depth-ordered entities)
  {
    VkDescriptorSetLayoutBinding instances = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .binding = 0,
        .descriptorCount = 1,
    };

    VkDescriptorSetLayoutBinding bindings[] = {instances};

    VkDescriptorSetLayoutCreateInfo instance_layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &bindings[0],
    };

    if (vkCreateDescriptorSetLayout(rend->device, &instance_layout_info, NULL,
                                    &rend->ecs->instance_layout) !=
        VK_SUCCESS) {
      printf("Couldn't create descriptor layout for `t_layout`.\n");
    }

    // Allocate a single descriptor
    VkDescriptorSetAllocateInfo instance_set_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = rend->descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &rend->ecs->instance_layout,
    };

    if (vkAllocateDescriptorSets(rend->device, &instance_set_info,
                                 &rend->ecs->instance_set)) {
      printf("Couldn't create descriptor set for `t_set`.\n");
    }

    VkDescriptorBufferInfo comp_entity_buffer = {
        .buffer = rend->ecs->instance_buffer,
        .offset = 0,
        .range = sizeof(unsigned) * count,
    };

    VkWriteDescriptorSet writes[1] = {
        [0] =
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = rend->ecs->instance_set,
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .pBufferInfo = &comp_entity_buffer,
            },
    };

    vkUpdateDescriptorSets(rend->device, 1, &writes[0], 0, NULL);
  }

  return true;
}

vk_system_t *VK_AddSystem_Transform(vk_rend_t *rend, const char *name,
                                    const char *src) {
  VkShaderModule module = VK_LoadShaderModule(rend, src);

  if (rend->ecs->system_count == 15) {
    printf("Couldn't create a new system! Max number of systems reached.\n");
    return NULL;
  }

  vk_system_t *new_system = &rend->ecs->systems[rend->ecs->system_count];
  rend->ecs->system_count++;

  // Create a compute pipeline for this system
  VkPipelineShaderStageCreateInfo shader_stage = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_COMPUTE_BIT,
      .module = module,
      .pName = "main",
  };

  VkComputePipelineCreateInfo t_pipeline = {
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .layout = rend->ecs->t_pipeline_layout,
      .stage = shader_stage,
  };

  vkCreateComputePipelines(rend->device, VK_NULL_HANDLE, 1, &t_pipeline, NULL,
                           &new_system->pipeline);

  new_system->related_set = rend->ecs->t_set;
  new_system->related_layout = rend->ecs->t_pipeline_layout;

  vkDestroyShaderModule(rend->device, module, NULL);

  return NULL;
}

vk_system_t *VK_AddSystem_Agent_Transform(vk_rend_t *rend, const char *name,
                                          const char *src) {
  // VkShaderModule module = VK_LoadShaderModule(rend, src);

  return NULL;
}

void VK_DestroyECS(vk_rend_t *rend) {
  vmaDestroyBuffer(rend->allocator, rend->ecs->t_buffer, rend->ecs->t_alloc);
  vmaDestroyBuffer(rend->allocator, rend->ecs->mt_buffer, rend->ecs->mt_alloc);

  for (unsigned i = 0; i < rend->ecs->system_count; i++) {
    vkDestroyPipeline(rend->device, rend->ecs->systems[i].pipeline, NULL);
  }

  vkDestroyPipelineLayout(rend->device, rend->ecs->t_pipeline_layout, NULL);

  vkDestroyDescriptorSetLayout(rend->device, rend->ecs->t_layout, NULL);
}

void VK_TickSystems(vk_rend_t *rend) {
  // VK_Draw waits on rend->logic_fence[i]
  // VK_TickSystems waits on rend->rend_fence[i-1]
  vkWaitForFences(rend->device, 1,
                  &rend->rend_fence[(rend->current_frame - 1) % 3], true,
                  1000000000);
  vkResetFences(rend->device, 1,
                &rend->rend_fence[(rend->current_frame - 1) % 3]);

  VkCommandBuffer cmd = rend->compute_command_buffer[rend->current_frame % 3];

  vkResetCommandBuffer(cmd, 0);

  VkCommandBufferBeginInfo begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };

  vkBeginCommandBuffer(cmd, &begin_info);

  if (rend->ecs->write_count) {
    vmaFlushAllocation(rend->allocator, rend->ecs->t_tmp_alloc, 0,
                       VK_WHOLE_SIZE);
    // vmaFlushAllocation(rend->allocator, rend->ecs->a_tmp_alloc, 0,
    //                    VK_WHOLE_SIZE);
    vmaFlushAllocation(rend->allocator, rend->ecs->s_tmp_alloc, 0,
                       VK_WHOLE_SIZE);
  }

  // Apply all ECS writes
  // !TODO should probably consolidate the copies to avoid copying multiple
  // little region
  for (unsigned i = 0; i < rend->ecs->write_count; i++) {
    vk_write_t *write = &rend->ecs->writes[i];
    VkBufferCopy copy = {
        .size = write->size,
        .dstOffset = write->offset,
        .srcOffset = write->offset,
    };

    vkCmdCopyBuffer(cmd, write->src, write->dst, 1, &copy);

    VkBufferMemoryBarrier2 barrier = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .buffer = write->dst,
        .size = write->size,
        .offset = write->offset,
        .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
    };

    VkDependencyInfo dependency = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .bufferMemoryBarrierCount = 1,
        .pBufferMemoryBarriers = &barrier,
    };

    vkCmdPipelineBarrier2(cmd, &dependency);
  }

  rend->ecs->write_count = 0;

  VkBufferMemoryBarrier2 t_barrier = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
      .buffer = rend->ecs->t_buffer,
      .size = VK_WHOLE_SIZE,
      .offset = 0,
      .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
      .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
  };

  VkBufferMemoryBarrier2 s_barrier = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
      .buffer = rend->ecs->s_buffer,
      .size = VK_WHOLE_SIZE,
      .offset = 0,
      .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
      .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
  };

  VkBufferMemoryBarrier2 barriers[] = {t_barrier, s_barrier};

  VkDependencyInfo dependency = {
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .bufferMemoryBarrierCount = 2,
      .pBufferMemoryBarriers = &barriers[0],
  };

  vkCmdPipelineBarrier2(cmd, &dependency);

  for (unsigned i = 0; i < rend->ecs->system_count; i++) {
    vk_system_t *system = &rend->ecs->systems[i];
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, system->pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            system->related_layout, 0, 1, &system->related_set,
                            0, 0);
    //
    vkCmdDispatch(cmd, (rend->ecs->entity_count + 16) / 16, 1, 1);
  }

  vkEndCommandBuffer(cmd);

  VkSubmitInfo submit_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pWaitDstStageMask = NULL,
      .waitSemaphoreCount = 0,
      .signalSemaphoreCount = 0,
      .commandBufferCount = 1,
      .pCommandBuffers = &rend->compute_command_buffer[rend->current_frame % 3],
  };

  vkQueueSubmit(rend->graphics_queue, 1, &submit_info,
                rend->logic_fence[rend->current_frame % 3]);
}

unsigned VK_Add_Entity(vk_rend_t *rend, unsigned signature) {
  size_t size = sizeof(unsigned);
  size_t offset = rend->ecs->entity_count * sizeof(unsigned);
  ((unsigned *)(rend->ecs->entities))[rend->ecs->entity_count] = signature;
  rend->ecs->entity_count++;
  VK_AddWriteECS(rend, rend->ecs->e_tmp_buffer, rend->ecs->e_buffer, offset,
                 size);

  return rend->ecs->entity_count - 1;
}

void VK_Add_Transform(vk_rend_t *rend, unsigned entity,
                      struct Transform *transform) {
  size_t size = sizeof(struct Transform);
  size_t offset = entity * sizeof(struct Transform);

  printf("writing transform with offset, %lu\n", offset);

  ((struct Transform *)rend->ecs->transforms)[entity] = *transform;

  // memcpy( + entity, transform, size);

  VK_AddWriteECS(rend, rend->ecs->t_tmp_buffer, rend->ecs->t_buffer, offset,
                 size);
}

void VK_Add_Model_Transform(vk_rend_t *rend, unsigned entity,
                            struct ModelTransform *model) {
  // We do nothing at the moment, cause all fields in a model transform
  // component are written during system update
}

void VK_Add_Agent(vk_rend_t *rend, unsigned entity, struct Agent *agent) {
  // size_t size = sizeof(struct Agent);
  // size_t offset = entity * sizeof(struct Agent);

  // memcpy((struct Agent *)rend->ecs->transforms + entity, agent, size);

  // VK_AddWriteECS(rend, rend->ecs->a_tmp_buffer, rend->ecs->a_buffer, offset,
  //                size);

  // !TODO
}

void VK_Add_Sprite(vk_rend_t *rend, unsigned entity, struct Sprite *sprite) {
  size_t size = sizeof(struct Sprite);
  size_t offset = entity * sizeof(struct Sprite);
  printf("writing sprite with offset, %lu\n", offset);

  ((struct Sprite *)rend->ecs->sprites)[entity] = *sprite;

  VK_AddWriteECS(rend, rend->ecs->s_tmp_buffer, rend->ecs->s_buffer, offset,
                 size);
}
