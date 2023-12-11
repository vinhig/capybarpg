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

void Vk_System_MemoryBarrier(VkCommandBuffer cmd, VkBuffer buffer) {
  VkBufferMemoryBarrier2 barrier = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
      .buffer = buffer,
      .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT |
                       VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT |
                       VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
      .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
      .offset = 0,
      .size = VK_WHOLE_SIZE,
  };

  VkDependencyInfo dependency = {
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .bufferMemoryBarrierCount = 1,
      .pBufferMemoryBarriers = &barrier,
  };

  vkCmdPipelineBarrier2(cmd, &dependency);
}

bool VK_InitECS(vk_rend_t *rend, unsigned count) {
  rend->ecs = calloc(1, sizeof(vk_ecs_t));
  rend->ecs->max_entities = count;

  // Create an empty map
  {
    VkBufferCreateInfo map_buffer = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(struct Tile) * 256 * 256,
        .pQueueFamilyIndices = &rend->queue_family_graphics_index,
        .queueFamilyIndexCount = 1,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    };

    VmaAllocationCreateInfo map_allocation = {
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    };

    vmaCreateBuffer(rend->allocator, &map_buffer, &map_allocation,
                    &rend->ecs->map_buffer, &rend->ecs->map_alloc, NULL);

    VkDebugUtilsObjectNameInfoEXT map_buffer_name = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_BUFFER,
        .objectHandle = (uint64_t)rend->ecs->map_buffer,
        .pObjectName = "Map Buffer",
    };
    rend->vkSetDebugUtilsObjectName(rend->device, &map_buffer_name);
  }
  {
    VkBufferCreateInfo map_tmp_buffer = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(struct Tile) * 256 * 256,
        .pQueueFamilyIndices = &rend->queue_family_graphics_index,
        .queueFamilyIndexCount = 1,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .usage =
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    };

    VmaAllocationCreateInfo map_tmp_allocation = {
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    };

    VmaAllocationInfo map_alloc_info;

    vmaCreateBuffer(rend->allocator, &map_tmp_buffer, &map_tmp_allocation,
                    &rend->ecs->map_tmp_buffer, &rend->ecs->map_tmp_alloc,
                    &map_alloc_info);
    rend->ecs->map = map_alloc_info.pMappedData;

    VkDebugUtilsObjectNameInfoEXT map_buffer_name = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_BUFFER,
        .objectHandle = (uint64_t)rend->ecs->map_tmp_buffer,
        .pObjectName = "Map Tmp Buffer",
    };
    rend->vkSetDebugUtilsObjectName(rend->device, &map_buffer_name);
  }

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

    VkBufferCreateInfo agent_buffer = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(struct Agent) * count,
        .pQueueFamilyIndices = &rend->queue_family_graphics_index,
        .queueFamilyIndexCount = 1,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    };

    VmaAllocationCreateInfo agent_allocation = {
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    };

    vmaCreateBuffer(rend->allocator, &agent_buffer, &agent_allocation,
                    &rend->ecs->a_buffer, &rend->ecs->a_alloc, NULL);

    VkDebugUtilsObjectNameInfoEXT a_buffer_name = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_BUFFER,
        .objectHandle = (uint64_t)rend->ecs->a_buffer,
        .pObjectName = "Agents Buffer",
    };
    rend->vkSetDebugUtilsObjectName(rend->device, &a_buffer_name);

    VkBufferCreateInfo immovable_buffer = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(struct Sprite) * count,
        .pQueueFamilyIndices = &rend->queue_family_graphics_index,
        .queueFamilyIndexCount = 1,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    };

    VmaAllocationCreateInfo immovable_allocation = {
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    };

    vmaCreateBuffer(rend->allocator, &immovable_buffer, &immovable_allocation,
                    &rend->ecs->i_buffer, &rend->ecs->i_alloc, NULL);

    VkDebugUtilsObjectNameInfoEXT i_buffer_name = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_BUFFER,
        .objectHandle = (uint64_t)rend->ecs->i_buffer,
        .pObjectName = "Immovables Buffer",
    };
    rend->vkSetDebugUtilsObjectName(rend->device, &i_buffer_name);
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

    VkDebugUtilsObjectNameInfoEXT instance_buffer_name = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_BUFFER,
        .objectHandle = (uint64_t)rend->ecs->instance_buffer,
        .pObjectName = "Instances Buffer",
    };
    rend->vkSetDebugUtilsObjectName(rend->device, &instance_buffer_name);
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

    VkBufferCreateInfo agent_tmp_buffer = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(struct Agent) * count,
        .pQueueFamilyIndices = &rend->queue_family_graphics_index,
        .queueFamilyIndexCount = 1,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    };

    VmaAllocationCreateInfo agent_tmp_allocation = {
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    };

    VmaAllocationInfo agent_transform_alloc_info;

    vmaCreateBuffer(rend->allocator, &agent_tmp_buffer, &agent_tmp_allocation,
                    &rend->ecs->a_tmp_buffer, &rend->ecs->a_tmp_alloc,
                    &agent_transform_alloc_info);
    rend->ecs->agents = agent_transform_alloc_info.pMappedData;

    VkDebugUtilsObjectNameInfoEXT a_tmp_buffer_name = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_BUFFER,
        .objectHandle = (uint64_t)rend->ecs->a_tmp_buffer,
        .pObjectName = "Agents Tmp Buffer",
    };
    rend->vkSetDebugUtilsObjectName(rend->device, &a_tmp_buffer_name);

    VkBufferCreateInfo immovable_tmp_buffer = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(struct Immovable) * count,
        .pQueueFamilyIndices = &rend->queue_family_graphics_index,
        .queueFamilyIndexCount = 1,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    };

    VmaAllocationCreateInfo immovable_tmp_allocation = {
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    };

    VmaAllocationInfo immovable_alloc_info;

    vmaCreateBuffer(rend->allocator, &immovable_tmp_buffer,
                    &immovable_tmp_allocation, &rend->ecs->i_tmp_buffer,
                    &rend->ecs->i_tmp_alloc, &immovable_alloc_info);
    rend->ecs->immovables = immovable_alloc_info.pMappedData;

    VkDebugUtilsObjectNameInfoEXT i_tmp_buffer_name = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_BUFFER,
        .objectHandle = (uint64_t)rend->ecs->i_tmp_buffer,
        .pObjectName = "Immovables Tmp Buffer",
    };
    rend->vkSetDebugUtilsObjectName(rend->device, &i_tmp_buffer_name);
  }

  // Create the ECS pipeline layout with the related descriptor set/layout
  {
    VkDescriptorSetLayoutBinding map = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .binding = 0,
        .descriptorCount = 1,
    };

    VkDescriptorSetLayoutBinding entities = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .binding = 1,
        .descriptorCount = 1,
    };

    VkDescriptorSetLayoutBinding transforms = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .binding = 2,
        .descriptorCount = 1,
    };

    VkDescriptorSetLayoutBinding model_transforms = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .binding = 3,
        .descriptorCount = 1,
    };

    VkDescriptorSetLayoutBinding sprites = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .binding = 4,
        .descriptorCount = 1,
    };

    VkDescriptorSetLayoutBinding agents = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .binding = 5,
        .descriptorCount = 1,
    };

    VkDescriptorSetLayoutBinding immovables = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .binding = 6,
        .descriptorCount = 1,
    };

    VkDescriptorSetLayoutBinding bindings[] = {
        map,     entities, transforms, model_transforms,
        sprites, agents,   immovables,
    };

    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = sizeof(bindings) / sizeof(VkDescriptorSetLayoutBinding),
        .pBindings = &bindings[0],
    };

    if (vkCreateDescriptorSetLayout(rend->device, &layout_info, NULL,
                                    &rend->ecs->ecs_layout) != VK_SUCCESS) {
      printf("Couldn't create descriptor layout for `t_layout`.\n");
    }

    // Allocate a single descriptor
    VkDescriptorSetAllocateInfo set_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = rend->descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &rend->ecs->ecs_layout,
    };

    if (vkAllocateDescriptorSets(rend->device, &set_info,
                                 &rend->ecs->ecs_set)) {
      printf("Couldn't create descriptor set for `t_set`.\n");
    }

    VkDescriptorBufferInfo comp_map_buffer = {
        .buffer = rend->ecs->map_buffer,
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };

    VkDescriptorBufferInfo comp_entity_buffer = {
        .buffer = rend->ecs->e_buffer,
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };

    VkDescriptorBufferInfo comp_transform_buffer = {
        .buffer = rend->ecs->t_buffer,
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };

    VkDescriptorBufferInfo comp_model_transform_buffer = {
        .buffer = rend->ecs->mt_buffer,
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };

    VkDescriptorBufferInfo comp_sprite_buffer = {
        .buffer = rend->ecs->s_buffer,
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };

    VkDescriptorBufferInfo comp_agent_buffer = {
        .buffer = rend->ecs->a_buffer,
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };

    VkDescriptorBufferInfo comp_immovable_buffer = {
        .buffer = rend->ecs->i_buffer,
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };

    VkWriteDescriptorSet writes[7] = {
        [0] =
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = rend->ecs->ecs_set,
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .pBufferInfo = &comp_map_buffer,
            },
        [1] =
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = rend->ecs->ecs_set,
                .dstBinding = 1,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .pBufferInfo = &comp_entity_buffer,
            },
        [2] =
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = rend->ecs->ecs_set,
                .dstBinding = 2,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .pBufferInfo = &comp_transform_buffer,
            },
        [3] =
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = rend->ecs->ecs_set,
                .dstBinding = 3,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .pBufferInfo = &comp_model_transform_buffer,
            },
        [4] =
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = rend->ecs->ecs_set,
                .dstBinding = 4,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .pBufferInfo = &comp_sprite_buffer,
            },
        [5] =
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = rend->ecs->ecs_set,
                .dstBinding = 5,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .pBufferInfo = &comp_agent_buffer,
            },

        [6] =
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = rend->ecs->ecs_set,
                .dstBinding = 6,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .pBufferInfo = &comp_immovable_buffer,
            },
    };

    vkUpdateDescriptorSets(rend->device, 7, &writes[0], 0, NULL);

    VkDescriptorSetLayout layouts[] = {rend->global_ubo_desc_set_layout,
                                       rend->ecs->ecs_layout};

    VkPipelineLayoutCreateInfo t_pipeline_layout = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 2,
        .pSetLayouts = &layouts[0],
    };

    if (vkCreatePipelineLayout(rend->device, &t_pipeline_layout, NULL,
                               &rend->ecs->ecs_pipeline_layout) != VK_SUCCESS) {
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
      .layout = rend->ecs->ecs_pipeline_layout,
      .stage = shader_stage,
  };

  vkCreateComputePipelines(rend->device, VK_NULL_HANDLE, 1, &t_pipeline, NULL,
                           &new_system->pipeline);

  VkDebugUtilsObjectNameInfoEXT name_info = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
      .objectType = VK_OBJECT_TYPE_PIPELINE,
      .objectHandle = (uint64_t)new_system->pipeline,
      .pObjectName = name,
  };
  rend->vkSetDebugUtilsObjectName(rend->device, &name_info);

  vkDestroyShaderModule(rend->device, module, NULL);

  return new_system;
}

vk_system_t *VK_AddSystem_Agent_Transform(vk_rend_t *rend, const char *name,
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

  VkComputePipelineCreateInfo at_pipeline = {
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .layout = rend->ecs->ecs_pipeline_layout,
      .stage = shader_stage,
  };

  vkCreateComputePipelines(rend->device, VK_NULL_HANDLE, 1, &at_pipeline, NULL,
                           &new_system->pipeline);

  VkDebugUtilsObjectNameInfoEXT name_info = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
      .objectType = VK_OBJECT_TYPE_PIPELINE,
      .objectHandle = (uint64_t)new_system->pipeline,
      .pObjectName = name,
  };
  rend->vkSetDebugUtilsObjectName(rend->device, &name_info);

  vkDestroyShaderModule(rend->device, module, NULL);

  return new_system;
}

void VK_DestroyECS(vk_rend_t *rend) {
  vmaDestroyBuffer(rend->allocator, rend->ecs->e_buffer, rend->ecs->e_alloc);
  vmaDestroyBuffer(rend->allocator, rend->ecs->e_tmp_buffer,
                   rend->ecs->e_tmp_alloc);

  vmaDestroyBuffer(rend->allocator, rend->ecs->map_buffer,
                   rend->ecs->map_alloc);
  vmaDestroyBuffer(rend->allocator, rend->ecs->map_tmp_buffer,
                   rend->ecs->map_tmp_alloc);

  vmaDestroyBuffer(rend->allocator, rend->ecs->t_buffer, rend->ecs->t_alloc);
  vmaDestroyBuffer(rend->allocator, rend->ecs->mt_buffer, rend->ecs->mt_alloc);

  vmaDestroyBuffer(rend->allocator, rend->ecs->t_tmp_buffer,
                   rend->ecs->t_tmp_alloc);
  vmaDestroyBuffer(rend->allocator, rend->ecs->mt_tmp_buffer,
                   rend->ecs->mt_tmp_alloc);

  vmaDestroyBuffer(rend->allocator, rend->ecs->a_buffer, rend->ecs->a_alloc);
  vmaDestroyBuffer(rend->allocator, rend->ecs->a_tmp_buffer,
                   rend->ecs->a_tmp_alloc);

  vmaDestroyBuffer(rend->allocator, rend->ecs->i_tmp_buffer,
                   rend->ecs->i_tmp_alloc);
  vmaDestroyBuffer(rend->allocator, rend->ecs->i_buffer, rend->ecs->i_alloc);

  vmaDestroyBuffer(rend->allocator, rend->ecs->s_tmp_buffer,
                   rend->ecs->s_tmp_alloc);
  vmaDestroyBuffer(rend->allocator, rend->ecs->s_buffer, rend->ecs->s_alloc);

  vmaDestroyBuffer(rend->allocator, rend->ecs->instance_buffer,
                   rend->ecs->instance_alloc);

  for (unsigned i = 0; i < rend->ecs->system_count; i++) {
    vkDestroyPipeline(rend->device, rend->ecs->systems[i].pipeline, NULL);
  }

  vkDestroyPipelineLayout(rend->device, rend->ecs->ecs_pipeline_layout, NULL);

  vkDestroyDescriptorSetLayout(rend->device, rend->ecs->instance_layout, NULL);

  vkDestroyDescriptorSetLayout(rend->device, rend->ecs->ecs_layout, NULL);
}

void VK_TickSystems(vk_rend_t *rend) {
  // VK_Draw waits on rend->logic_fence[i]
  // VK_TickSystems waits on rend->rend_fence[i-1]
  if (rend->current_frame != 0) {
    vkWaitForFences(rend->device, 1,
                    &rend->rend_fence[(rend->current_frame - 1) % 3], true, 1000000);
    vkResetFences(rend->device, 1,
                  &rend->rend_fence[(rend->current_frame - 1) % 3]);
  }

  VkCommandBuffer cmd = rend->compute_command_buffer[rend->current_frame % 3];

  vkResetCommandBuffer(cmd, 0);

  VkCommandBufferBeginInfo begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };

  vkBeginCommandBuffer(cmd, &begin_info);

  // Apply ECS writes (but whole buffer one)
  VK_AddWriteECS(rend, rend->ecs->t_tmp_buffer, rend->ecs->t_buffer, 0,
                 sizeof(struct Transform) * rend->ecs->max_entities);
  VK_AddWriteECS(rend, rend->ecs->a_tmp_buffer, rend->ecs->a_buffer, 0,
                 sizeof(struct Agent) * rend->ecs->max_entities);

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

  VkBufferMemoryBarrier2 a_barrier = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
      .buffer = rend->ecs->a_buffer,
      .size = VK_WHOLE_SIZE,
      .offset = 0,
      .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
      .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
  };

  VkBufferMemoryBarrier2 barriers[] = {t_barrier, s_barrier, a_barrier};

  VkDependencyInfo dependency = {
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .bufferMemoryBarrierCount = 3,
      .pBufferMemoryBarriers = &barriers[0],
  };

  vkCmdPipelineBarrier2(cmd, &dependency);

  for (unsigned i = 0; i < rend->ecs->system_count; i++) {
    vk_system_t *system = &rend->ecs->systems[i];
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, system->pipeline);
    vkCmdBindDescriptorSets(
        cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rend->ecs->ecs_pipeline_layout, 0,
        1, &rend->global_ubo_desc_set[rend->current_frame % 3], 0, 0);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            rend->ecs->ecs_pipeline_layout, 1, 1,
                            &rend->ecs->ecs_set, 0, 0);
    //
    vkCmdDispatch(cmd, (rend->ecs->entity_count + 16) / 16, 1, 1);

    Vk_System_MemoryBarrier(cmd, rend->ecs->a_buffer);
    Vk_System_MemoryBarrier(cmd, rend->ecs->s_buffer);
    Vk_System_MemoryBarrier(cmd, rend->ecs->t_buffer);
    Vk_System_MemoryBarrier(cmd, rend->ecs->mt_buffer);
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
  size_t size = sizeof(struct Agent);
  size_t offset = entity * sizeof(struct Agent);

  ((struct Agent *)rend->ecs->agents)[entity] = *agent;

  VK_AddWriteECS(rend, rend->ecs->a_tmp_buffer, rend->ecs->a_buffer, offset,
                 size);
}

void VK_Add_Sprite(vk_rend_t *rend, unsigned entity, struct Sprite *sprite) {
  size_t size = sizeof(struct Sprite);
  size_t offset = entity * sizeof(struct Sprite);

  ((struct Sprite *)rend->ecs->sprites)[entity] = *sprite;

  VK_AddWriteECS(rend, rend->ecs->s_tmp_buffer, rend->ecs->s_buffer, offset,
                 size);
}

void VK_Add_Immovable(vk_rend_t *rend, unsigned entity,
                      struct Immovable *immovable) {
  size_t size = sizeof(struct Immovable);
  size_t offset = entity * sizeof(struct Immovable);

  ((struct Immovable *)rend->ecs->immovables)[entity] = *immovable;

  VK_AddWriteECS(rend, rend->ecs->i_tmp_buffer, rend->ecs->i_buffer, offset,
                 size);
}

void VK_SetMap(vk_rend_t *rend, struct Tile *tiles, unsigned map_width,
               unsigned map_height) {
  size_t size = sizeof(struct Tile) * map_height * map_width;
  memcpy(rend->ecs->map, tiles, size);
  VK_AddWriteECS(rend, rend->ecs->map_tmp_buffer, rend->ecs->map_buffer, 0,
                 size);
}
