#include "vk/vk_private.h"
#include "vk/vk_system.h"
#include "vk/vk_vulkan.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game/g_game.h"
#include "vk_mem_alloc.h"
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan_core.h>

static unsigned error_count = 0;

void VK_Gigabarrier(VkCommandBuffer cmd) {
  VkMemoryBarrier2 barrier = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
      .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
      .srcAccessMask =
          VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
      .dstAccessMask =
          VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
  };

  VkDependencyInfo dependency = {
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .memoryBarrierCount = 1,
      .pMemoryBarriers = &barrier,
  };

  vkCmdPipelineBarrier2(cmd, &dependency);
}

typedef struct depth_entry_t {
  unsigned entity;
  float depth;
  float right;
} depth_entry_t;

int VK_OrderDepth(const void *a, const void *b) {
  // Compare the depth field of two tmp_t structures
  const depth_entry_t *tmp_a = (const depth_entry_t *)a;
  const depth_entry_t *tmp_b = (const depth_entry_t *)b;

  if (tmp_a->depth < tmp_b->depth) {
    return 1;
  } else if (tmp_a->depth > tmp_b->depth) {
    return -1;
  } else {
    if (tmp_a->right < tmp_b->right) {
      return 1;
    } else if (tmp_a->right > tmp_b->right) {
      return -1;
    }
  }

  return 0;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL VK_DebugCallBack(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT *data, void *user_data) {

  if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
    printf("%s\n", data->pMessage);

    error_count += 1;
  }

  if (error_count > 15) {
    // exit(-1);
  }

  return VK_FALSE;
}

void VK_TransitionColorTexture(VkCommandBuffer cmd, VkImage image,
                               VkImageLayout from_layout,
                               VkImageLayout to_layout,
                               VkPipelineStageFlags2 from_stage,
                               VkPipelineStageFlags2 to_stage,
                               VkAccessFlags2 src_access_mask,
                               VkAccessFlags2 dst_access_mask) {
  VkImageMemoryBarrier2 barrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
      .srcAccessMask = src_access_mask,
      .dstAccessMask = dst_access_mask,
      .srcStageMask = from_stage,
      .dstStageMask = to_stage,
      .oldLayout = from_layout,
      .newLayout = to_layout,
      .image = image,
      .subresourceRange =
          {
              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
              .baseMipLevel = 0,
              .levelCount = 1,
              .baseArrayLayer = 0,
              .layerCount = 1,
          },
  };

  VkDependencyInfo info = {
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .pImageMemoryBarriers = &barrier,
      .imageMemoryBarrierCount = 1,
  };

  vkCmdPipelineBarrier2(cmd, &info);
}

void VK_TransitionDepthTexture(VkCommandBuffer cmd, VkImage image,
                               VkImageLayout from_layout,
                               VkImageLayout to_layout,
                               VkPipelineStageFlags from_stage,
                               VkPipelineStageFlags to_stage,
                               VkAccessFlags access_mask) {
  VkImageMemoryBarrier barrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      // .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      .dstAccessMask = access_mask,
      .oldLayout = from_layout,
      .newLayout = to_layout,
      .image = image,
      .subresourceRange = {
          .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
          .baseMipLevel = 0,
          .levelCount = 1,
          .baseArrayLayer = 0,
          .layerCount = 1,
      }};

  vkCmdPipelineBarrier(cmd, from_stage, to_stage, 0, 0, NULL, 0, NULL, 1,
                       &barrier);
}

const char *vk_instance_layers[] = {
    "VK_LAYER_KHRONOS_validation",
};
const unsigned vk_instance_layer_count = 1;

const char *vk_instance_extensions[] = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
const unsigned vk_instance_extension_count = 1;

const char *vk_device_extensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    "VK_KHR_dynamic_rendering",
};
const unsigned vk_device_extension_count = 2;

const char *vk_device_layers[] = {
    "VK_EXT_shader_object",
};
const unsigned vk_device_layer_count = 1;

char vk_error[1024];

#define VK_PUSH_ERROR(r)                                                       \
  {                                                                            \
    memcpy(vk_error, r, strlen(r));                                            \
    return NULL;                                                               \
  }

#define VK_CHECK_R(r)                                                          \
  if (r != VK_SUCCESS) {                                                       \
    memcpy(vk_error, #r, strlen(#r));                                          \
    return NULL;                                                               \
  }

bool VK_CheckDeviceFeatures(VkExtensionProperties *extensions,
                            unsigned extension_count) {
  unsigned req = vk_device_extension_count;

  for (unsigned j = 0; j < req; j++) {
    bool found = false;
    for (unsigned i = 0; i < extension_count; ++i) {
      if (strcmp(extensions[i].extensionName, vk_device_extensions[j]) == 0) {
        found = true;
        break;
      }
    }

    if (!found) {
      printf("Device extension `%s` isn't supported by this physical device.\n",
             vk_device_extensions[j]);
      return false;
    }
  }

  return true;
}

VkShaderModule VK_LoadShaderModule(vk_rend_t *rend, const char *path) {
  FILE *f = fopen(path, "rb");

  if (!f) {
    printf("`%s` file doesn't seem to exist.\n", path);
    return NULL;
  }

  fseek(f, 0, SEEK_END);
  size_t size = ftell(f);
  fseek(f, 0, SEEK_SET);

  void *shader = malloc(size);

  fread(shader, size, 1, f);
  fclose(f);

  VkShaderModuleCreateInfo shader_info = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = size,
      .pCode = shader,
  };

  VkShaderModule module;

  if (vkCreateShaderModule(rend->device, &shader_info, NULL, &module) !=
      VK_SUCCESS) {
    printf("`%s` doesn't seem to be a valid shader.\n", path);
    free(shader);
    return NULL;
  }

  free(shader);

  return module;
}

vk_rend_t *VK_CreateRend(client_t *client, unsigned width, unsigned height) {
  vk_rend_t *rend = calloc(1, sizeof(vk_rend_t));

  rend->width = width;
  rend->height = height;
  rend->current_frame = 0;

  // INSTANCE CREATION
  {
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .apiVersion = VK_MAKE_VERSION(1, 3, 0),
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "Maidenless",
        .pApplicationName = "Maidenless",
    };

    unsigned instance_extension_count = 0;
    char *instance_extensions[10];
    SDL_Vulkan_GetInstanceExtensions(CL_GetWindow(client),
                                     &instance_extension_count, NULL);
    SDL_Vulkan_GetInstanceExtensions(CL_GetWindow(client),
                                     &instance_extension_count,
                                     (const char **)instance_extensions);

    for (unsigned h = 0; h < vk_instance_extension_count; h++) {
      instance_extensions[h + instance_extension_count] =
          (char *)vk_instance_extensions[h];
    }

    instance_extension_count += vk_instance_extension_count;

    for (unsigned h = 0; h < instance_extension_count; h++) {
      printf("-> %s\n", instance_extensions[h]);
    }

    VkInstanceCreateInfo instance_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledLayerCount = vk_instance_layer_count,
        .ppEnabledLayerNames = &vk_instance_layers[0],
        .enabledExtensionCount = instance_extension_count,
        .ppEnabledExtensionNames = (const char *const *)&instance_extensions[0],
    };

    VK_CHECK_R(vkCreateInstance(&instance_info, NULL, &rend->instance));
  }

  // Debug messenger creation, i swear to God i didn't have to create one before
  {
    VkDebugUtilsMessengerCreateInfoEXT debug_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = VK_DebugCallBack,
        .pUserData = rend,
    };

    PFN_vkCreateDebugUtilsMessengerEXT func =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            rend->instance, "vkCreateDebugUtilsMessengerEXT");
    func(rend->instance, &debug_info, NULL, &rend->debug_messenger);
  }

  // Surface creation
  {
    VkSurfaceKHR surface;
    if (!SDL_Vulkan_CreateSurface(CL_GetWindow(client), rend->instance,
                                  &surface)) {
      VK_PUSH_ERROR("Couldn't create `VkSurfaceKHR`.");
    }

    rend->surface = surface;
  }

  // Device creation
  {
    VkPhysicalDevice physical_devices[10];
    unsigned physical_device_count = 0;
    vkEnumeratePhysicalDevices(rend->instance, &physical_device_count, NULL);
    vkEnumeratePhysicalDevices(rend->instance, &physical_device_count,
                               physical_devices);

    if (physical_device_count == 0) {
      VK_PUSH_ERROR("No GPU supporting vulkan.");
    }

    VkPhysicalDevice physical_device;

    bool found_suitable = false;

    // Dummy choice, just get the first one
    for (unsigned i = 0; i < physical_device_count; i++) {
      VkPhysicalDeviceProperties property;
      VkPhysicalDeviceFeatures features;

      vkGetPhysicalDeviceProperties(physical_devices[i], &property);
      vkGetPhysicalDeviceFeatures(physical_devices[i], &features);

      unsigned extension_count;
      vkEnumerateDeviceExtensionProperties(physical_devices[i], NULL,
                                           &extension_count, NULL);
      VkExtensionProperties *extensions =
          malloc(sizeof(VkExtensionProperties) * extension_count);
      vkEnumerateDeviceExtensionProperties(physical_devices[i], NULL,
                                           &extension_count, extensions);

      unsigned layer_count;
      vkEnumerateDeviceLayerProperties(physical_devices[i], &layer_count, NULL);
      VkLayerProperties *layers =
          malloc(sizeof(VkLayerProperties) * layer_count);
      vkEnumerateDeviceLayerProperties(physical_devices[i], &layer_count,
                                       layers);

      VkSurfaceCapabilitiesKHR surface_cap;
      vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_devices[i],
                                                rend->surface, &surface_cap);

      unsigned format_count;
      vkGetPhysicalDeviceSurfaceFormatsKHR(physical_devices[i], rend->surface,
                                           &format_count, NULL);
      VkSurfaceFormatKHR *formats =
          malloc(sizeof(VkSurfaceFormatKHR) * format_count);
      vkGetPhysicalDeviceSurfaceFormatsKHR(physical_devices[i], rend->surface,
                                           &format_count, formats);

      bool all_extensions_ok =
          VK_CheckDeviceFeatures(extensions, extension_count);
      if (!all_extensions_ok) {
        printf("`%s` doesn't support all needed extensions.\n",
               property.deviceName);
        continue;
      }

      if (format_count == 0) {
        // Exit, no suitable format for this combinaison of physical device and
        // surface
        free(extensions);
        free(formats);
        free(layers);
        printf("skipping because no format\n");
        continue;
      } else {
        VkFormat desired_format = VK_FORMAT_B8G8R8A8_SRGB;
        VkColorSpaceKHR desired_color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

        bool found = false;
        for (unsigned f = 0; f < format_count; f++) {
          if (formats[f].colorSpace == desired_color_space &&
              formats[f].format == desired_format) {
            found = true;
          }
        }

        if (!found) {
          free(extensions);
          free(formats);
          free(layers);
          printf("skipping because no desired format\n");
          continue;
        }
      }

      // TODO: Check if ray-tracing is there
      // For now, we only take the first integrated hehe
      if (found_suitable &&
          property.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        physical_device = physical_devices[i];
        found_suitable = true;
        free(extensions);
        free(formats);
        free(layers);
      } else if (!found_suitable) {
        physical_device = physical_devices[i];
        found_suitable = true;
        free(extensions);
        free(formats);
        free(layers);
        break;
      }

      free(extensions);
      free(formats);
      free(layers);
    }

    if (!found_suitable) {
      VK_PUSH_ERROR("No suitable physical device found.");
    }

    rend->physical_device = physical_device;
  }

  // Logical device
  {
    unsigned queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(rend->physical_device,
                                             &queue_family_count, NULL);
    VkQueueFamilyProperties *queue_families =
        malloc(sizeof(VkQueueFamilyProperties) * queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(
        rend->physical_device, &queue_family_count, queue_families);

    unsigned queue_family_graphics_index = 0;
    // unsigned queue_family_transfer_index = 0;
    bool queue_family_graphics_found = false;
    // bool queue_family_transfer_found = false;
    for (unsigned i = 0; i < queue_family_count; i++) {
      if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT &&
          queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
        queue_family_graphics_found = true;
        queue_family_graphics_index = i;
      }
      // else if (queue_families[i].queueFlags & VK_QUEUE_TRANSFER_BIT &&
      //            !(queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
      //   queue_family_transfer_found = true;
      //   queue_family_transfer_index = i;
      // }
    }

    if (!queue_family_graphics_found) {
      VK_PUSH_ERROR("Didn't not find a queue that fits the requirement. "
                    "(graphics & compute).");
    }
    // if (!queue_family_transfer_found) {
    //   VK_PUSH_ERROR("Didn't not find a queue that fits the requirement. "
    //                 "(transfer).");
    // }

    rend->queue_family_graphics_index = queue_family_graphics_index;
    // rend->queue_family_transfer_index = queue_family_transfer_index;

    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_graphics_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = queue_family_graphics_index,
        .pQueuePriorities = &queue_priority,
        .queueCount = 1,
    };
    // VkDeviceQueueCreateInfo queue_transfer_info = {
    //     .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
    //     .queueFamilyIndex = queue_family_transfer_index,
    //     .pQueuePriorities = &queue_priority,
    //     .queueCount = 1,
    // };

    // VkDeviceQueueCreateInfo queue_infos[] = {queue_graphics_info,
    //                                          queue_transfer_info};

    VkPhysicalDeviceRobustness2FeaturesEXT robustness2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT,
        .nullDescriptor = VK_TRUE,
    };

    VkPhysicalDeviceFeatures vulkan = {
        .samplerAnisotropy = VK_TRUE,
        .shaderInt64 = VK_TRUE,
    };

    VkPhysicalDeviceVulkan11Features vulkan_11 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        .pNext = &robustness2,
    };

    VkPhysicalDeviceVulkan12Features vulkan_12 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .descriptorBindingPartiallyBound = VK_TRUE,
        .descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,
        .descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE,
        .descriptorBindingVariableDescriptorCount = VK_TRUE,
        .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
        .runtimeDescriptorArray = VK_TRUE,
        .descriptorIndexing = VK_TRUE,
        .bufferDeviceAddress = VK_TRUE,
        .shaderInt8 = VK_TRUE,
        .pNext = &vulkan_11,
    };

    VkPhysicalDeviceVulkan13Features vulkan_13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .dynamicRendering = VK_TRUE,
        .synchronization2 = VK_TRUE,
        .pNext = &vulkan_12,
    };

    VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pQueueCreateInfos = &queue_graphics_info,
        .queueCreateInfoCount = 1,
        .pNext = &vulkan_13,
        .enabledExtensionCount = vk_device_extension_count,
        .ppEnabledExtensionNames = vk_device_extensions,
        .pEnabledFeatures = &vulkan,
    };

    for (unsigned j = 0; j < vk_device_extension_count; j++) {
      printf("-> %s\n", vk_device_extensions[j]);
    }

    for (unsigned j = 0; j < vk_device_layer_count; j++) {
      printf("-> %s\n", vk_device_layers[j]);
    }

    VK_CHECK_R(vkCreateDevice(rend->physical_device, &device_info, NULL,
                              &rend->device));

    vkGetDeviceQueue(rend->device, queue_family_graphics_index, 0,
                     &rend->graphics_queue);
    // vkGetDeviceQueue(rend->device, queue_family_transfer_index, 0,
    //                  &rend->transfer_queue);

    free(queue_families);
  }

  rend->vkSetDebugUtilsObjectName =
      (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(
          rend->instance, "vkSetDebugUtilsObjectNameEXT");

  // Create swapchain and corresponding images
  {
    VkExtent2D image_extent = {
        .width = width,
        .height = height,
    };

    VkSwapchainCreateInfoKHR swapchain_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = rend->surface,
        .minImageCount = 3,
        .imageFormat = VK_FORMAT_B8G8R8A8_SRGB,
        .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .imageExtent = image_extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                      VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                      VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE};

    VK_CHECK_R(vkCreateSwapchainKHR(rend->device, &swapchain_info, NULL,
                                    &rend->swapchain));

    rend->swapchain_format = VK_FORMAT_B8G8R8A8_SRGB;

    unsigned image_count = 0;
    vkGetSwapchainImagesKHR(rend->device, rend->swapchain, &image_count, NULL);
    VkImage *images = malloc(sizeof(VkImage) * image_count);
    VkImageView *image_views = malloc(sizeof(VkImageView) * image_count);
    vkGetSwapchainImagesKHR(rend->device, rend->swapchain, &image_count,
                            images);

    // Name swapchain images
    // Transition them to the present layout
    for (unsigned i = 0; i < image_count; i++) {
      char name[256];
      sprintf(&name[0], "Swapchain Images[%d]", i);
      VkDebugUtilsObjectNameInfoEXT swapchain_image_name = {
          .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
          .objectType = VK_OBJECT_TYPE_IMAGE,
          .objectHandle = (uint64_t)images[i],
          .pObjectName = name,
      };
      rend->vkSetDebugUtilsObjectName(rend->device, &swapchain_image_name);
    }

    VkImageViewCreateInfo image_view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_B8G8R8A8_SRGB,
        .components =
            {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    for (unsigned i = 0; i < image_count; i++) {
      image_view_info.image = images[i];
      VK_CHECK_R(vkCreateImageView(rend->device, &image_view_info, NULL,
                                   &image_views[i]));
    }

    rend->swapchain_images = images;
    rend->swapchain_image_views = image_views;
    rend->swapchain_image_count = image_count;
  }

  // Create as many command buffers as needed
  // 3, one for each concurrent frame
  {
    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = rend->queue_family_graphics_index,
    };
    VK_CHECK_R(vkCreateCommandPool(rend->device, &pool_info, NULL,
                                   &rend->graphics_command_pool));
    VkCommandBufferAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = rend->graphics_command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 3,
    };

    vkAllocateCommandBuffers(rend->device, &allocate_info,
                             &rend->graphics_command_buffer[0]);
    vkAllocateCommandBuffers(rend->device, &allocate_info,
                             &rend->compute_command_buffer[0]);
  }

  // Same operations, but only one command buffer for the transfer pool
  // {
  //   VkCommandPoolCreateInfo pool_info = {
  //       .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
  //       .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
  //       .queueFamilyIndex = rend->queue_family_transfer_index,
  //   };
  //   VK_CHECK_R(vkCreateCommandPool(rend->device, &pool_info, NULL,
  //                                  &rend->transfer_command_pool));
  VkCommandBufferAllocateInfo allocate_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = rend->graphics_command_pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
  };

  vkAllocateCommandBuffers(rend->device, &allocate_info,
                           &rend->transfer_command_buffer);
  // }

  // Create default samplers
  {
    VkSamplerCreateInfo nearest_sampler_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        // .anisotropyEnable = VK_TRUE,
        // .maxAnisotropy = 16,
    };

    vkCreateSampler(rend->device, &nearest_sampler_info, NULL,
                    &rend->nearest_sampler);

    nearest_sampler_info.magFilter = VK_FILTER_LINEAR;
    nearest_sampler_info.minFilter = VK_FILTER_LINEAR;
    nearest_sampler_info.anisotropyEnable = VK_TRUE;
    nearest_sampler_info.maxAnisotropy = 16;

    vkCreateSampler(rend->device, &nearest_sampler_info, NULL,
                    &rend->linear_sampler);

    nearest_sampler_info.anisotropyEnable = VK_TRUE,
    nearest_sampler_info.magFilter = VK_FILTER_LINEAR;
    nearest_sampler_info.minFilter = VK_FILTER_LINEAR;
    nearest_sampler_info.anisotropyEnable = VK_TRUE;
    nearest_sampler_info.maxAnisotropy = 16;

    vkCreateSampler(rend->device, &nearest_sampler_info, NULL,
                    &rend->anisotropy_sampler);
  }

  // Create semaphores
  {
    VkSemaphoreCreateInfo semaphore_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    };

    for (int i = 0; i < 3; i++) {
      vkCreateSemaphore(rend->device, &semaphore_info, NULL,
                        &rend->swapchain_present_semaphore[i]);
      vkCreateSemaphore(rend->device, &semaphore_info, NULL,
                        &rend->swapchain_render_semaphore[i]);

      vkCreateFence(rend->device, &fence_info, NULL, &rend->rend_fence[i]);
      vkCreateFence(rend->device, &fence_info, NULL, &rend->logic_fence[i]);
    }
    // The transfer queue is by default free to be used
    // So create the fence with a SIGNALED state
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    vkCreateFence(rend->device, &fence_info, NULL, &rend->transfer_fence);
  }

  // Create descriptor pool
  {
    // Dummy allocating, i dont even know if it's important
    VkDescriptorPoolSize pool_sizes[] = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 50},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 50},
        {VK_DESCRIPTOR_TYPE_SAMPLER, 50},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 50},
    };

    // We allocate 50 uniforms buffers
    // We allocate

    VkDescriptorPoolCreateInfo desc_pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 200,
        .poolSizeCount = 4,
        .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
        .pPoolSizes = &pool_sizes[0],
    };

    vkCreateDescriptorPool(rend->device, &desc_pool_info, NULL,
                           &rend->descriptor_pool);
  }

  VmaAllocatorCreateInfo allocator_info = {
      .physicalDevice = rend->physical_device,
      .device = rend->device,
      .instance = rend->instance,
  };

  vmaCreateAllocator(&allocator_info, &rend->allocator);

  // Create global descriptor set layout and descriptor set (that contains map
  // and general infos). Create global ubo and map buffer too.
  {
    VkDescriptorSetLayoutBinding global_ubo_binding = {
        .binding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
    };

    VkDescriptorSetLayoutBinding bindings[] = {global_ubo_binding};

    VkDescriptorSetLayoutCreateInfo desc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &bindings[0],
    };

    vkCreateDescriptorSetLayout(rend->device, &desc_info, NULL,
                                &rend->global_ubo_desc_set_layout);

    VkBufferCreateInfo global_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(vk_global_ubo_t),
        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    };

    VmaAllocationCreateInfo global_alloc_info = {
        .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    };

    VkDescriptorSetAllocateInfo global_desc_set_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = rend->descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &rend->global_ubo_desc_set_layout,
    };

    VkDescriptorBufferInfo global_desc_buffer_info = {
        .range = sizeof(vk_global_ubo_t),
    };

    VkWriteDescriptorSet global_desc_write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pBufferInfo = &global_desc_buffer_info,
    };

    for (int i = 0; i < 3; i++) {
      vmaCreateBuffer(rend->allocator, &global_buffer_info, &global_alloc_info,
                      &rend->global_buffers[i], &rend->global_allocs[i], NULL);

      vkAllocateDescriptorSets(rend->device, &global_desc_set_info,
                               &rend->global_ubo_desc_set[i]);

      global_desc_buffer_info.buffer = rend->global_buffers[i];
      global_desc_write.dstSet = rend->global_ubo_desc_set[i];

      vkUpdateDescriptorSets(rend->device, 1, &global_desc_write, 0, NULL);
    }
  }

  // Create the descriptor set holding all freaking textures
  {
    unsigned max_bindless_resources = 2048;
    // Create bindless descriptor pool
    VkDescriptorPoolSize pool_sizes_bindless[] = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, max_bindless_resources},
    };

    // Update after bind is needed here, for each binding and in the descriptor
    // set layout creation.
    VkDescriptorPoolCreateInfo desc_pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT,
        .maxSets = max_bindless_resources,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_sizes_bindless[0],
    };

    vkCreateDescriptorPool(rend->device, &desc_pool_info, NULL,
                           &rend->descriptor_bindless_pool);

    // YO!
    VkDescriptorBindingFlags bindless_flags =
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
        VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT |
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

    VkDescriptorSetLayoutBinding vk_binding = {
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = max_bindless_resources,
        .binding = 0,
        .stageFlags = VK_SHADER_STAGE_ALL,
        .pImmutableSamplers = NULL,
    };

    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &vk_binding,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
    };

    VkDescriptorSetLayoutBindingFlagsCreateInfo extended_info = {
        .sType =
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount = 1,
        .pBindingFlags = &bindless_flags,
    };

    layout_info.pNext = &extended_info;

    vkCreateDescriptorSetLayout(rend->device, &layout_info, NULL,
                                &rend->global_textures_desc_set_layout);

    unsigned max_binding = 1024;
    VkDescriptorSetVariableDescriptorCountAllocateInfo count_info = {
        .sType =
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
        .descriptorSetCount = 1,
        .pDescriptorCounts = &max_binding,
    };

    VkDescriptorSetAllocateInfo global_textures_desc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = rend->descriptor_bindless_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &rend->global_textures_desc_set_layout,
        .pNext = &count_info,
    };

    VkResult r = vkAllocateDescriptorSets(
        rend->device, &global_textures_desc_info, &rend->map_textures_desc_set);
    if (r != VK_SUCCESS) {
      printf("[ERROR] vkAllocateDescriptorSets(rend->map_textures_desc_set)"
             "failed. Error code: %d\n",
             r);
      VK_PUSH_ERROR("oh no\n");
    }

    r = vkAllocateDescriptorSets(rend->device, &global_textures_desc_info,
                                 &rend->font_textures_desc_set);
    if (r != VK_SUCCESS) {
      printf("[ERROR] vkAllocateDescriptorSets(rend->font_textures_desc_set) "
             "failed. Error code: %d\n",
             r);
      VK_PUSH_ERROR("oh no\n");
    }
  }

  if (!VK_InitECS(rend, 3000)) {
    VK_PUSH_ERROR("Couldn't create ECS subsystem.\n");
  }

  // Initialize other parts of the renderer
  if (!VK_InitGBuffer(rend)) {
    VK_PUSH_ERROR("Couldn't create a specific pipeline: GBuffer.");
  }

  if (!VK_InitImmediate(rend)) {
    VK_PUSH_ERROR("Couldn't create a specific pipeline: Immediate.");
  }

  rend->immediate_handle_capacity = 16;
  rend->immediate_handles = calloc(16, sizeof(vk_texture_handle_t));

  return rend;
}

void VK_Present(vk_rend_t *rend, unsigned image_index) {
  VkPresentInfoKHR present_info = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .pSwapchains = &rend->swapchain,
      .swapchainCount = 1,
      .pWaitSemaphores =
          &rend->swapchain_render_semaphore[rend->current_frame % 3],
      .waitSemaphoreCount = 1,
      .pImageIndices = &image_index,
  };

  vkQueuePresentKHR(rend->graphics_queue, &present_info);
  rend->current_frame++;
}

void VK_Draw(vk_rend_t *rend, game_state_t *game) {
  // VK_Draw waits on rend->logic_fence[i]
  // VK_TickSystems waits on rend->rend_fence[i-1]
  vkWaitForFences(rend->device, 1,
                  &rend->logic_fence[(rend->current_frame) % 3], true,
                  1000000000);
  vkResetFences(rend->device, 1, &rend->logic_fence[(rend->current_frame) % 3]);

  VkCommandBuffer cmd = rend->graphics_command_buffer[rend->current_frame % 3];

  vkResetCommandBuffer(cmd, 0);

  unsigned image_index = 0;
  vkAcquireNextImageKHR(
      rend->device, rend->swapchain, UINT64_MAX,
      rend->swapchain_present_semaphore[rend->current_frame % 3], NULL,
      &image_index);

  // !TODO: should be done in a compute shader maybe...
  // Order entities by depth

  // !TODO: should find a way to do otherwise lmao, it assumes no entity can be
  // destroyed
  depth_entry_t tmps[3000];

  // Populate tmp, find minimum/maximum to put depth in correct range
  float min_depth = FLT_MAX;
  float max_depth = FLT_MIN;
  for (unsigned i = 0; i < rend->ecs->entity_count; i++) {
    tmps[i].entity = i;
    tmps[i].depth =
        ((struct Transform *)rend->ecs->transforms)[i].position[1] * -1.0f;
    tmps[i].right = ((struct Transform *)rend->ecs->transforms)[i].position[0];

    if (tmps[i].depth > max_depth) {
      max_depth = tmps[i].depth;
    }

    if (tmps[i].depth < min_depth) {
      min_depth = tmps[i].depth;
    }
  }

  qsort(tmps, rend->ecs->entity_count, sizeof(depth_entry_t), VK_OrderDepth);

  for (unsigned j = 0; j < rend->ecs->entity_count; j++) {
    ((unsigned *)rend->ecs->instances)[j] = tmps[j].entity;
  }

  vmaFlushAllocation(rend->allocator, rend->ecs->instance_alloc, 0,
                     VK_WHOLE_SIZE);

  // Copy game state first person data
  rend->global_ubo.min_depth = min_depth;
  rend->global_ubo.max_depth = max_depth;
  memcpy(&rend->global_ubo, &game->fps, sizeof(game->fps));

  rend->global_ubo.view_dim[0] = rend->width;
  rend->global_ubo.view_dim[1] = rend->height;
  rend->global_ubo.entity_count = rend->ecs->entity_count;

  void *data;
  vmaMapMemory(rend->allocator, rend->global_allocs[rend->current_frame % 3],
               &data);

  memcpy(data, &rend->global_ubo, sizeof(vk_global_ubo_t));

  vmaUnmapMemory(rend->allocator, rend->global_allocs[rend->current_frame % 3]);
  vmaFlushAllocation(rend->allocator,
                     rend->global_allocs[rend->current_frame % 3], 0,
                     VK_WHOLE_SIZE);

  VkCommandBufferBeginInfo begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };

  vkBeginCommandBuffer(cmd, &begin_info);

  VK_DrawGBuffer(rend);

  VK_DrawImmediate(rend, game);

  // Before rendering, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR ->
  // VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL.
  {
    VkImageLayout old_layout;
    if (rend->current_frame < 3) {
      old_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    } else {
      old_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    }

    VkImageMemoryBarrier2 swapchain_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .image = rend->swapchain_images[image_index],
        .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .oldLayout = old_layout,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        }};

    VkImageMemoryBarrier2 gbuffer_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .image = rend->gbuffer->albedo_target.image,
        .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
        .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        }};

    VkImageMemoryBarrier2 barriers[] = {swapchain_barrier, gbuffer_barrier};

    VkDependencyInfo dependency = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 2,
        .pImageMemoryBarriers = &barriers[0],
    };

    vkCmdPipelineBarrier2(cmd, &dependency);
  }

  VkImageBlit blit_region = {
      .srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .srcSubresource.layerCount = 1,
      .srcOffsets[1].x = rend->width,
      .srcOffsets[1].y = rend->height,
      .srcOffsets[1].z = 1,
      .dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .dstSubresource.layerCount = 1,
      .dstOffsets[1].x = rend->width,
      .dstOffsets[1].y = rend->height,
      .dstOffsets[1].z = 1,
  };

  vkCmdBlitImage(
      cmd, rend->gbuffer->albedo_target.image,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, rend->swapchain_images[image_index],
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit_region, VK_FILTER_NEAREST);

  {
    VkImageMemoryBarrier2 swapchain_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .image = rend->swapchain_images[image_index],
        .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT, /// HEREEE
        .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        }};

    VkImageMemoryBarrier2 gbuffer_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .image = rend->gbuffer->albedo_target.image,
        .srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
        .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        }};

    VkImageMemoryBarrier2 barriers[] = {swapchain_barrier, gbuffer_barrier};

    VkDependencyInfo dependency = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 2,
        .pImageMemoryBarriers = &barriers[0],
    };

    vkCmdPipelineBarrier2(cmd, &dependency);
  }

  vkEndCommandBuffer(cmd);

  VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;

  VkSubmitInfo submit_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pWaitDstStageMask = &wait_stage,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores =
          &rend->swapchain_present_semaphore[rend->current_frame % 3],
      .signalSemaphoreCount = 1,
      .pSignalSemaphores =
          &rend->swapchain_render_semaphore[rend->current_frame % 3],
      .commandBufferCount = 1,
      .pCommandBuffers =
          &rend->graphics_command_buffer[rend->current_frame % 3],
  };

  vkQueueSubmit(rend->graphics_queue, 1, &submit_info,
                rend->rend_fence[rend->current_frame % 3]);

  VK_Present(rend, image_index);
}

void VK_DestroyRend(vk_rend_t *rend) {
  vkDeviceWaitIdle(rend->device);

  VK_DestroyImmediate(rend);
  VK_DestroyGBuffer(rend);
  VK_DestroyECS(rend);

  for (unsigned h = 0; h < rend->immediate_handle_count; h++) {
    vmaDestroyBuffer(rend->allocator, rend->immediate_handles[h].staging,
                     rend->immediate_handles[h].staging_alloc);

    vkDestroyImageView(rend->device, rend->immediate_handles[h].image_view,
                       NULL);

    vmaDestroyImage(rend->allocator, rend->immediate_handles[h].image,
                    rend->immediate_handles[h].image_alloc);
  }

  for (unsigned i = 0; i < rend->map_assets.texture_count; i++) {
    vmaDestroyBuffer(rend->allocator, rend->map_assets.textures_staging[i],
                     rend->map_assets.textures_staging_allocs[i]);

    vkDestroyImageView(rend->device, rend->map_assets.texture_views[i], NULL);

    vmaDestroyImage(rend->allocator, rend->map_assets.textures[i],
                    rend->map_assets.textures_allocs[i]);
  }

  for (unsigned i = 0; i < rend->font_assets.texture_count; i++) {
    vmaDestroyBuffer(rend->allocator, rend->font_assets.textures_staging[i],
                     rend->font_assets.textures_staging_allocs[i]);

    vkDestroyImageView(rend->device, rend->font_assets.texture_views[i], NULL);

    vmaDestroyImage(rend->allocator, rend->font_assets.textures[i],
                    rend->font_assets.textures_allocs[i]);
  }

  for (int i = 0; i < 3; i++) {
    vmaDestroyBuffer(rend->allocator, rend->global_buffers[i],
                     rend->global_allocs[i]);
  }

  free(rend->map_assets.texture_views);
  free(rend->map_assets.textures);
  free(rend->map_assets.textures_allocs);
  free(rend->map_assets.textures_staging);
  free(rend->map_assets.textures_staging_allocs);

  free(rend->font_assets.texture_views);
  free(rend->font_assets.textures);
  free(rend->font_assets.textures_allocs);
  free(rend->font_assets.textures_staging);
  free(rend->font_assets.textures_staging_allocs);

  vkDestroySampler(rend->device, rend->nearest_sampler, NULL);
  vkDestroySampler(rend->device, rend->linear_sampler, NULL);
  vkDestroySampler(rend->device, rend->anisotropy_sampler, NULL);

  vkDestroyDescriptorSetLayout(rend->device, rend->global_ubo_desc_set_layout,
                               NULL);
  vkDestroyDescriptorSetLayout(rend->device,
                               rend->global_textures_desc_set_layout, NULL);

  vkDestroyDescriptorPool(rend->device, rend->descriptor_pool, NULL);
  vkDestroyDescriptorPool(rend->device, rend->descriptor_bindless_pool, NULL);

  vmaDestroyAllocator(rend->allocator);

  vkDestroyFence(rend->device, rend->transfer_fence, NULL);

  for (int i = 0; i < 3; i++) {
    vkDestroyFence(rend->device, rend->rend_fence[i], NULL);
    vkDestroyFence(rend->device, rend->logic_fence[i], NULL);

    vkDestroySemaphore(rend->device, rend->swapchain_present_semaphore[i],
                       NULL);
    vkDestroySemaphore(rend->device, rend->swapchain_render_semaphore[i], NULL);
  }
  vkDestroyCommandPool(rend->device, rend->graphics_command_pool, NULL);
  // vkDestroyCommandPool(rend->device, rend->transfer_command_pool, NULL);
  for (unsigned i = 0; i < rend->swapchain_image_count; i++) {
    vkDestroyImageView(rend->device, rend->swapchain_image_views[i], NULL);
  }
  vkDestroySwapchainKHR(rend->device, rend->swapchain, NULL);
  vkDestroyDevice(rend->device, NULL);
  vkDestroySurfaceKHR(rend->instance, rend->surface, NULL);

  PFN_vkDestroyDebugUtilsMessengerEXT func =
      (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
          rend->instance, "vkDestroyDebugUtilsMessengerEXT");
  if (func != NULL) {
    func(rend->instance, rend->debug_messenger, NULL);
  }

  vkDestroyInstance(rend->instance, NULL);

  free(rend->immediate_handles);
  free(rend->swapchain_images);
  free(rend->swapchain_image_views);

  free(rend->gbuffer);
  free(rend);
}

const char *VK_GetError() { return (const char *)vk_error; }

void VK_CreateTexturesDescriptor(vk_rend_t *rend, vk_assets_t *assets,
                                 VkDescriptorSet dst_set) {
  // Should be "UpdateTexturesDescriptor", but how god vulkan is complicated
  // Add dynamically too
  VkWriteDescriptorSet *writes =
      calloc(1, sizeof(VkWriteDescriptorSet) * assets->texture_count);
  VkDescriptorImageInfo *image_infos =
      calloc(1, sizeof(VkDescriptorImageInfo) * assets->texture_count);

  for (unsigned t = 0; t < assets->texture_count; t++) {
    VkImageView texture = assets->texture_views[t];

    image_infos[t].sampler = rend->linear_sampler;
    image_infos[t].imageView = texture;
    image_infos[t].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    writes[t].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[t].descriptorCount = 1;
    writes[t].dstArrayElement = t; // should be something else
    writes[t].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[t].dstSet = dst_set;
    writes[t].dstBinding = 0;
    writes[t].pImageInfo = &image_infos[t];
  }

  vkUpdateDescriptorSets(rend->device, assets->texture_count, writes, 0, NULL);

  free(writes);
  free(image_infos);
}

void VK_UploadMapTextures(vk_rend_t *rend, texture_t *textures,
                          unsigned count) {
  vkWaitForFences(rend->device, 1, &rend->transfer_fence, true, UINT64_MAX);

  vkResetFences(rend->device, 1, &rend->transfer_fence);

  VkCommandBuffer cmd = rend->transfer_command_buffer;
  VkCommandBufferBeginInfo begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };

  vkBeginCommandBuffer(cmd, &begin_info);

  // Work with all textures and the related global descriptor
  {
    VkImage *vk_textures = malloc(sizeof(VkImage) * count);
    VmaAllocation *texture_allocs = malloc(sizeof(VmaAllocation) * count);

    VkImageView *texture_views = malloc(sizeof(VkImageView) * count);

    VkBuffer *stagings = malloc(sizeof(VkBuffer) * count);
    VmaAllocation *staging_allocs = malloc(sizeof(VmaAllocation) * count);

    for (size_t t = 0; t < count; t++) {
      texture_t *texture = &textures[t];
      VkExtent3D extent = {
          .width = texture->width,
          .height = texture->height,
          .depth = 1,
      };
      VkImageCreateInfo tex_info = {
          .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
          .imageType = VK_IMAGE_TYPE_2D,
          .format = VK_FORMAT_R8G8B8A8_SRGB,
          .extent = extent,
          .mipLevels = 1,
          .arrayLayers = 1,
          .samples = VK_SAMPLE_COUNT_1_BIT,
          .tiling = VK_IMAGE_TILING_OPTIMAL,
          .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
      };
      VmaAllocationCreateInfo tex_alloc_info = {.usage =
                                                    VMA_MEMORY_USAGE_GPU_ONLY};

      vmaCreateImage(rend->allocator, &tex_info, &tex_alloc_info,
                     &vk_textures[t], &texture_allocs[t], NULL);

      // Change layout of this image
      VkImageSubresourceRange range;
      range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      range.baseMipLevel = 0;
      range.levelCount = 1;
      range.baseArrayLayer = 0;
      range.layerCount = 1;

      VkImageMemoryBarrier image_barrier_1 = {
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
          .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
          .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          .image = vk_textures[t],
          .subresourceRange = range,
          .srcAccessMask = 0,
          .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
      };

      vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL,
                           1, &image_barrier_1);

      // CREATE, MAP
      VkBufferCreateInfo staging_info = {
          .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
          .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
          .size = texture->width * texture->height * 4,
      };

      VmaAllocationCreateInfo staging_alloc_info = {
          .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
      };

      vmaCreateBuffer(rend->allocator, &staging_info, &staging_alloc_info,
                      &stagings[t], &staging_allocs[t], NULL);

      void *data;
      vmaMapMemory(rend->allocator, staging_allocs[t], &data);
      memcpy(data, texture->data, texture->width * texture->height * 4);
      vmaUnmapMemory(rend->allocator, staging_allocs[t]);

      // COPY
      VkBufferImageCopy copy_region = {
          .bufferOffset = 0,
          .bufferRowLength = 0,
          .bufferImageHeight = 0,
          .imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
          .imageSubresource.mipLevel = 0,
          .imageSubresource.baseArrayLayer = 0,
          .imageSubresource.layerCount = 1,
          .imageExtent = extent,
      };

      vkCmdCopyBufferToImage(cmd, stagings[t], vk_textures[t],
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                             &copy_region);

      image_barrier_1.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      image_barrier_1.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      image_barrier_1.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      image_barrier_1.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

      vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0,
                           NULL, 1, &image_barrier_1);

      // Create image view and sampler
      // Almost there
      VkImageViewCreateInfo image_view_info = {
          .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
          .viewType = VK_IMAGE_VIEW_TYPE_2D,
          .format = VK_FORMAT_R8G8B8A8_SRGB,
          .components =
              {
                  .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                  .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                  .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                  .a = VK_COMPONENT_SWIZZLE_IDENTITY,
              },
          .subresourceRange =
              {
                  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                  .baseMipLevel = 0,
                  .levelCount = 1,
                  .baseArrayLayer = 0,
                  .layerCount = 1,
              },
          .image = vk_textures[t],
      };

      vkCreateImageView(rend->device, &image_view_info, NULL,
                        &texture_views[t]);

      if (texture->label) {
        VkDebugUtilsObjectNameInfoEXT image_view_name = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .objectType = VK_OBJECT_TYPE_IMAGE_VIEW,
            .objectHandle = (uint64_t)texture_views[t],
            .pObjectName = texture->label,
        };

        rend->vkSetDebugUtilsObjectName(rend->device, &image_view_name);
      }
    }
    rend->map_assets.textures = vk_textures;
    rend->map_assets.textures_allocs = texture_allocs;
    rend->map_assets.textures_staging = stagings;
    rend->map_assets.textures_staging_allocs = staging_allocs;
    rend->map_assets.texture_count = count;
    rend->map_assets.texture_views = texture_views;
  }
  vkEndCommandBuffer(cmd);

  VkSubmitInfo submit_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &rend->transfer_command_buffer,
  };

  vkQueueSubmit(rend->graphics_queue, 1, &submit_info, rend->transfer_fence);

  VK_CreateTexturesDescriptor(rend, &rend->map_assets,
                              rend->map_textures_desc_set);
}

void VK_UploadFontTextures(vk_rend_t *rend, texture_t *textures,
                           unsigned count) {
  vkWaitForFences(rend->device, 1, &rend->transfer_fence, true, UINT64_MAX);

  vkResetFences(rend->device, 1, &rend->transfer_fence);

  VkCommandBuffer cmd = rend->transfer_command_buffer;
  VkCommandBufferBeginInfo begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };

  vkBeginCommandBuffer(cmd, &begin_info);

  // Work with all textures and the related global descriptor
  {
    VkImage *vk_textures = malloc(sizeof(VkImage) * count);
    VmaAllocation *texture_allocs = malloc(sizeof(VmaAllocation) * count);

    VkImageView *texture_views = malloc(sizeof(VkImageView) * count);

    VkBuffer *stagings = malloc(sizeof(VkBuffer) * count);
    VmaAllocation *staging_allocs = malloc(sizeof(VmaAllocation) * count);

    for (size_t t = 0; t < count; t++) {
      texture_t *texture = &textures[t];
      VkExtent3D extent = {
          .width = texture->width,
          .height = texture->height,
          .depth = 1,
      };
      VkImageCreateInfo tex_info = {
          .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
          .imageType = VK_IMAGE_TYPE_2D,
          .format = VK_FORMAT_R8_UINT,
          .extent = extent,
          .mipLevels = 1,
          .arrayLayers = 1,
          .samples = VK_SAMPLE_COUNT_1_BIT,
          .tiling = VK_IMAGE_TILING_OPTIMAL,
          .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
      };
      VmaAllocationCreateInfo tex_alloc_info = {.usage =
                                                    VMA_MEMORY_USAGE_GPU_ONLY};

      vmaCreateImage(rend->allocator, &tex_info, &tex_alloc_info,
                     &vk_textures[t], &texture_allocs[t], NULL);

      // Change layout of this image
      VkImageSubresourceRange range;
      range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      range.baseMipLevel = 0;
      range.levelCount = 1;
      range.baseArrayLayer = 0;
      range.layerCount = 1;

      VkImageMemoryBarrier image_barrier_1 = {
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
          .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
          .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          .image = vk_textures[t],
          .subresourceRange = range,
          .srcAccessMask = 0,
          .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
      };

      vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL,
                           1, &image_barrier_1);

      // CREATE, MAP
      VkBufferCreateInfo staging_info = {
          .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
          .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
          .size = texture->width * texture->height,
      };

      VmaAllocationCreateInfo staging_alloc_info = {
          .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
      };

      vmaCreateBuffer(rend->allocator, &staging_info, &staging_alloc_info,
                      &stagings[t], &staging_allocs[t], NULL);

      void *data;
      vmaMapMemory(rend->allocator, staging_allocs[t], &data);
      memcpy(data, texture->data, texture->width * texture->height);
      vmaUnmapMemory(rend->allocator, staging_allocs[t]);

      // COPY
      VkBufferImageCopy copy_region = {
          .bufferOffset = 0,
          .bufferRowLength = 0,
          .bufferImageHeight = 0,
          .imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
          .imageSubresource.mipLevel = 0,
          .imageSubresource.baseArrayLayer = 0,
          .imageSubresource.layerCount = 1,
          .imageExtent = extent,
      };

      vkCmdCopyBufferToImage(cmd, stagings[t], vk_textures[t],
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                             &copy_region);

      image_barrier_1.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      image_barrier_1.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      image_barrier_1.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      image_barrier_1.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

      vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0,
                           NULL, 1, &image_barrier_1);

      // Create image view and sampler
      // Almost there
      VkImageViewCreateInfo image_view_info = {
          .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
          .viewType = VK_IMAGE_VIEW_TYPE_2D,
          .format = VK_FORMAT_R8_UINT,
          .components =
              {
                  .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                  .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                  .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                  .a = VK_COMPONENT_SWIZZLE_IDENTITY,
              },
          .subresourceRange =
              {
                  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                  .baseMipLevel = 0,
                  .levelCount = 1,
                  .baseArrayLayer = 0,
                  .layerCount = 1,
              },
          .image = vk_textures[t],
      };

      vkCreateImageView(rend->device, &image_view_info, NULL,
                        &texture_views[t]);
    }
    rend->font_assets.textures = vk_textures;
    rend->font_assets.textures_allocs = texture_allocs;
    rend->font_assets.textures_staging = stagings;
    rend->font_assets.textures_staging_allocs = staging_allocs;
    rend->font_assets.texture_count = count;
    rend->font_assets.texture_views = texture_views;
  }
  vkEndCommandBuffer(cmd);

  VkSubmitInfo submit_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &rend->transfer_command_buffer,
  };

  vkQueueSubmit(rend->graphics_queue, 1, &submit_info, rend->transfer_fence);

  VK_CreateTexturesDescriptor(rend, &rend->font_assets,
                              rend->font_textures_desc_set);
}

void VK_UpdateFontTextures(vk_rend_t *rend, texture_t *textures,
                           unsigned count) {
  vkWaitForFences(rend->device, 1, &rend->transfer_fence, true, UINT64_MAX);

  vkResetFences(rend->device, 1, &rend->transfer_fence);

  VkCommandBuffer cmd = rend->transfer_command_buffer;
  VkCommandBufferBeginInfo begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };

  vkBeginCommandBuffer(cmd, &begin_info);

  // Work with all textures and the related global descriptor
  {
    VkImage *vk_textures = malloc(sizeof(VkImage) * count);
    VmaAllocation *texture_allocs = malloc(sizeof(VmaAllocation) * count);

    VkImageView *texture_views = malloc(sizeof(VkImageView) * count);

    VkBuffer *stagings = malloc(sizeof(VkBuffer) * count);
    VmaAllocation *staging_allocs = malloc(sizeof(VmaAllocation) * count);

    for (size_t t = 0; t < count; t++) {
      texture_t *texture = &textures[t];
      VkExtent3D extent = {
          .width = texture->width,
          .height = texture->height,
          .depth = 1,
      };

      VkImageCreateInfo tex_info = {
          .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
          .imageType = VK_IMAGE_TYPE_2D,
          .format = VK_FORMAT_R8_UINT,
          .extent = extent,
          .mipLevels = 1,
          .arrayLayers = 1,
          .samples = VK_SAMPLE_COUNT_1_BIT,
          .tiling = VK_IMAGE_TILING_OPTIMAL,
          .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
      };
      VmaAllocationCreateInfo tex_alloc_info = {.usage =
                                                    VMA_MEMORY_USAGE_GPU_ONLY};

      vmaCreateImage(rend->allocator, &tex_info, &tex_alloc_info,
                     &vk_textures[t], &texture_allocs[t], NULL);

      // Change layout of this image
      VkImageSubresourceRange range;
      range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      range.baseMipLevel = 0;
      range.levelCount = 1;
      range.baseArrayLayer = 0;
      range.layerCount = 1;

      VkImageMemoryBarrier image_barrier_1 = {
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
          .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
          .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          .image = vk_textures[t],
          .subresourceRange = range,
          .srcAccessMask = 0,
          .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
      };

      vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL,
                           1, &image_barrier_1);

      // CREATE, MAP
      VkBufferCreateInfo staging_info = {
          .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
          .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
          .size = texture->width * texture->height,
      };

      VmaAllocationCreateInfo staging_alloc_info = {
          .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
      };

      vmaCreateBuffer(rend->allocator, &staging_info, &staging_alloc_info,
                      &stagings[t], &staging_allocs[t], NULL);

      void *data;
      vmaMapMemory(rend->allocator, staging_allocs[t], &data);
      memcpy(data, texture->data, texture->width * texture->height);
      vmaUnmapMemory(rend->allocator, staging_allocs[t]);

      // COPY
      VkBufferImageCopy copy_region = {
          .bufferOffset = 0,
          .bufferRowLength = 0,
          .bufferImageHeight = 0,
          .imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
          .imageSubresource.mipLevel = 0,
          .imageSubresource.baseArrayLayer = 0,
          .imageSubresource.layerCount = 1,
          .imageExtent = extent,
      };

      vkCmdCopyBufferToImage(cmd, stagings[t], vk_textures[t],
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                             &copy_region);

      image_barrier_1.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      image_barrier_1.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      image_barrier_1.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      image_barrier_1.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

      vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                           VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0,
                           NULL, 1, &image_barrier_1);

      // Create image view and sampler
      // Almost there
      VkImageViewCreateInfo image_view_info = {
          .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
          .viewType = VK_IMAGE_VIEW_TYPE_2D,
          .format = VK_FORMAT_R8_UINT,
          .components =
              {
                  .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                  .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                  .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                  .a = VK_COMPONENT_SWIZZLE_IDENTITY,
              },
          .subresourceRange =
              {
                  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                  .baseMipLevel = 0,
                  .levelCount = 1,
                  .baseArrayLayer = 0,
                  .layerCount = 1,
              },
          .image = vk_textures[t],
      };

      vkCreateImageView(rend->device, &image_view_info, NULL,
                        &texture_views[t]);
    }

    unsigned off = rend->font_assets.texture_count;

    rend->font_assets.textures =
        realloc(rend->font_assets.textures, sizeof(VkImage) * (off + count));
    rend->font_assets.textures_allocs =
        realloc(rend->font_assets.textures_allocs,
                sizeof(VmaAllocation) * (off + count));
    rend->font_assets.textures_staging = realloc(
        rend->font_assets.textures_staging, sizeof(VkBuffer) * (off + count));
    rend->font_assets.textures_staging_allocs =
        realloc(rend->font_assets.textures_staging_allocs,
                sizeof(VmaAllocation) * (off + count));
    rend->font_assets.texture_views = realloc(
        rend->font_assets.texture_views, sizeof(VkImageView) * (off + count));

    for (unsigned pp = 0; pp < count; pp++) {
      rend->font_assets.textures[off + pp] = vk_textures[pp];
      rend->font_assets.textures_allocs[off + pp] = texture_allocs[pp];
      rend->font_assets.textures_staging[off + pp] = stagings[pp];
      rend->font_assets.textures_staging_allocs[off + pp] = staging_allocs[pp];
      rend->font_assets.texture_views[off + pp] = texture_views[pp];
    }

    free(vk_textures);
    free(texture_allocs);
    free(stagings);
    free(staging_allocs);
    free(texture_views);

    rend->font_assets.texture_count += count;
  }
  vkEndCommandBuffer(cmd);

  VkSubmitInfo submit_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &rend->transfer_command_buffer,
  };

  vkQueueSubmit(rend->graphics_queue, 1, &submit_info, rend->transfer_fence);

  VkWriteDescriptorSet *writes =
      calloc(1, sizeof(VkWriteDescriptorSet) * count);
  VkDescriptorImageInfo *image_infos =
      calloc(1, sizeof(VkDescriptorImageInfo) * count);

  unsigned i = 0;
  for (unsigned t = rend->font_assets.texture_count - count;
       t < rend->font_assets.texture_count; t++) {
    VkImageView texture = rend->font_assets.texture_views[t];

    image_infos[i].sampler = rend->linear_sampler;
    image_infos[i].imageView = texture;
    image_infos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[i].descriptorCount = 1;
    writes[i].dstArrayElement = t; // should be something else
    writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[i].dstSet = rend->font_textures_desc_set;
    writes[i].dstBinding = 0;
    writes[i].pImageInfo = &image_infos[i];
    i++;
  }

  vkUpdateDescriptorSets(rend->device, count, writes, 0, NULL);

  free(writes);
  free(image_infos);
}

void VK_UploadSingleTexture(vk_rend_t *rend, texture_t *texture) {
  vkWaitForFences(rend->device, 1, &rend->transfer_fence, true, UINT64_MAX);

  vkResetFences(rend->device, 1, &rend->transfer_fence);

  VkCommandBuffer cmd = rend->transfer_command_buffer;
  VkCommandBufferBeginInfo begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };

  vkBeginCommandBuffer(cmd, &begin_info);

  VkImage vk_image;
  VmaAllocation vk_image_alloc;
  VkImageView vk_image_view;

  VkBuffer vk_staging;
  VmaAllocation vk_staging_alloc;

  VkExtent3D extent = {
      .width = texture->width,
      .height = texture->height,
      .depth = 1,
  };
  VkImageCreateInfo tex_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = VK_FORMAT_R8G8B8A8_SRGB,
      .extent = extent,
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
  };
  VmaAllocationCreateInfo tex_alloc_info = {.usage = VMA_MEMORY_USAGE_GPU_ONLY};

  vmaCreateImage(rend->allocator, &tex_info, &tex_alloc_info, &vk_image,
                 &vk_image_alloc, NULL);

  // Change layout of this image
  VkImageSubresourceRange range;
  range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  range.baseMipLevel = 0;
  range.levelCount = 1;
  range.baseArrayLayer = 0;
  range.layerCount = 1;

  VkImageMemoryBarrier image_barrier_1 = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      .image = vk_image,
      .subresourceRange = range,
      .srcAccessMask = 0,
      .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
  };

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1,
                       &image_barrier_1);

  // CREATE, MAP
  VkBufferCreateInfo staging_info = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
      .size = texture->width * texture->height * 4,
  };

  VmaAllocationCreateInfo staging_alloc_info = {
      .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
  };

  vmaCreateBuffer(rend->allocator, &staging_info, &staging_alloc_info,
                  &vk_staging, &vk_staging_alloc, NULL);

  void *data;
  vmaMapMemory(rend->allocator, vk_staging_alloc, &data);
  memcpy(data, texture->data, texture->width * texture->height * 4);
  vmaUnmapMemory(rend->allocator, vk_staging_alloc);

  // COPY
  VkBufferImageCopy copy_region = {
      .bufferOffset = 0,
      .bufferRowLength = 0,
      .bufferImageHeight = 0,
      .imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .imageSubresource.mipLevel = 0,
      .imageSubresource.baseArrayLayer = 0,
      .imageSubresource.layerCount = 1,
      .imageExtent = extent,
  };

  vkCmdCopyBufferToImage(cmd, vk_staging, vk_image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

  image_barrier_1.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  image_barrier_1.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  image_barrier_1.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  image_barrier_1.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0,
                       NULL, 1, &image_barrier_1);

  // Create image view and sampler
  // Almost there
  VkImageViewCreateInfo image_view_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = VK_FORMAT_R8G8B8A8_SRGB,
      .components =
          {
              .r = VK_COMPONENT_SWIZZLE_IDENTITY,
              .g = VK_COMPONENT_SWIZZLE_IDENTITY,
              .b = VK_COMPONENT_SWIZZLE_IDENTITY,
              .a = VK_COMPONENT_SWIZZLE_IDENTITY,
          },
      .subresourceRange =
          {
              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
              .baseMipLevel = 0,
              .levelCount = 1,
              .baseArrayLayer = 0,
              .layerCount = 1,
          },
      .image = vk_image,
  };

  vkCreateImageView(rend->device, &image_view_info, NULL, &vk_image_view);

  if (texture->label) {
    VkDebugUtilsObjectNameInfoEXT image_view_name = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_IMAGE_VIEW,
        .objectHandle = (uint64_t)vk_image_view,
        .pObjectName = texture->label,
    };

    rend->vkSetDebugUtilsObjectName(rend->device, &image_view_name);
  }

  vkEndCommandBuffer(cmd);

  VkSubmitInfo submit_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &rend->transfer_command_buffer,
  };

  vkQueueSubmit(rend->graphics_queue, 1, &submit_info, rend->transfer_fence);

  if (rend->immediate_handle_count >= rend->immediate_handle_capacity) {
    rend->immediate_handles =
        realloc(rend->immediate_handles, sizeof(vk_texture_handle_t) * 2 *
                                             rend->immediate_handle_capacity);

    rend->immediate_handle_capacity *= 2;
  }

  texture->handle = rend->immediate_handle_count;
  rend->immediate_handle_count++;

  rend->immediate_handles[texture->handle] = (vk_texture_handle_t){
      .image = vk_image,
      .image_alloc = vk_image_alloc,
      .image_view = vk_image_view,
      .staging = vk_staging,
      .staging_alloc = vk_staging_alloc,
  };
}

void *VK_GetAgents(vk_rend_t *rend) { return rend->ecs->agents; }

void *VK_GetTransforms(vk_rend_t *rend) { return rend->ecs->transforms; }

void *VK_GetEntities(vk_rend_t *rend) { return rend->ecs->entities; }
