#include <imgui/imgui.h>
#include <imgui/imgui_impl_sdl2.h>
#include <imgui/imgui_impl_vulkan.h>
#include <stdio.h>
#include <vulkan/vulkan_core.h>

extern "C" {

#include <client/cl_client.h>
#include <vk/vk_private.h>

void VK_InitUI(client_t *client, vk_rend_t *rend) {
  ImGui::CreateContext();

  ImGui_ImplSDL2_InitForVulkan(static_cast<SDL_Window *>(CL_GetWindow(client)));

  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.Instance = rend->instance;
  init_info.PhysicalDevice = rend->physical_device;
  init_info.Device = rend->device;
  init_info.Queue = rend->graphics_queue;
  init_info.DescriptorPool = rend->descriptor_imgui_pool;
  init_info.MinImageCount = 3;
  init_info.ImageCount = 3;
  init_info.ColorAttachmentFormat = VK_FORMAT_R16G16B16A16_SFLOAT,
  init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  init_info.UseDynamicRendering = true;

  ImGui_ImplVulkan_Init(&init_info, NULL);

  ImGui_ImplVulkan_CreateFontsTexture();
}

void VK_BeginUI(client_t *client) {
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplSDL2_NewFrame(static_cast<SDL_Window*>(CL_GetWindow(client)));

  ImGui::NewFrame();
}

void VK_DrawUI(vk_rend_t *rend, VkCommandBuffer cmd) {
  ImGui::ShowAboutWindow();

  ImGui::Render();

  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

void ImGui_ProcessEvent(SDL_Event *event) {
  ImGui_ImplSDL2_ProcessEvent(event);
}

void VK_DestroyUI() {
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplSDL2_Shutdown();

  ImGui::DestroyContext();
}
}
