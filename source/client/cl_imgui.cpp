#include <imgui/imgui.h>
#include <imgui/imgui_impl_sdl2.h>
#include <imgui/imgui_impl_vulkan.h>
#include <stdio.h>
#include <vulkan/vulkan_core.h>

extern "C" {

#include <client/cl_client.h>
#include <vk/vk_private.h>

void SetupImGuiStyle() {
  // Fork of Quick minimal look style from ImThemes
  ImGuiStyle &style = ImGui::GetStyle();

  style.Alpha = 1.0f;
  style.DisabledAlpha = 0.300000011920929f;
  style.WindowPadding = ImVec2(6.5f, 2.700000047683716f);
  style.WindowRounding = 0.0f;
  style.WindowBorderSize = 1.0f;
  style.WindowMinSize = ImVec2(20.0f, 32.0f);
  style.WindowTitleAlign = ImVec2(0.0f, 0.6000000238418579f);
  style.WindowMenuButtonPosition = ImGuiDir_None;
  style.ChildRounding = 0.0f;
  style.ChildBorderSize = 1.0f;
  style.PopupRounding = 10.10000038146973f;
  style.PopupBorderSize = 1.0f;
  style.FramePadding = ImVec2(20.0f, 3.5f);
  style.FrameRounding = 0.0f;
  style.FrameBorderSize = 0.0f;
  style.ItemSpacing = ImVec2(4.400000095367432f, 4.0f);
  style.ItemInnerSpacing = ImVec2(4.599999904632568f, 3.599999904632568f);
  style.CellPadding = ImVec2(3.099999904632568f, 6.300000190734863f);
  style.IndentSpacing = 4.400000095367432f;
  style.ColumnsMinSpacing = 5.400000095367432f;
  style.ScrollbarSize = 8.800000190734863f;
  style.ScrollbarRounding = 9.0f;
  style.GrabMinSize = 9.399999618530273f;
  style.GrabRounding = 0.0f;
  style.TabRounding = 0.0f;
  style.TabBorderSize = 0.0f;
  style.TabMinWidthForCloseButton = 0.0f;
  style.ColorButtonPosition = ImGuiDir_Right;
  style.ButtonTextAlign = ImVec2(0.0f, 0.5f);
  style.SelectableTextAlign = ImVec2(0.0f, 0.0f);

  style.Colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
  style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.4980392158031464f, 0.4980392158031464f, 0.4980392158031464f, 1.0f);
  style.Colors[ImGuiCol_WindowBg] = ImVec4(0.05098039284348488f, 0.03529411926865578f, 0.03921568766236305f, 1.0f);
  style.Colors[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
  style.Colors[ImGuiCol_PopupBg] = ImVec4(0.0784313753247261f, 0.0784313753247261f, 0.0784313753247261f, 0.9399999976158142f);
  style.Colors[ImGuiCol_Border] = ImVec4(0.1019607856869698f, 0.1019607856869698f, 0.1019607856869698f, 0.5f);
  style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
  style.Colors[ImGuiCol_FrameBg] = ImVec4(0.1843137294054031f, 0.2823529541492462f, 0.3450980484485626f, 1.0f);
  style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.2666666805744171f, 0.3058823645114899f, 0.4470588266849518f, 1.0f);
  style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.4509803950786591f, 0.3019607961177826f, 0.4862745106220245f, 1.0f);
  style.Colors[ImGuiCol_TitleBg] = ImVec4(0.2618025541305542f, 0.2460719048976898f, 0.1966328173875809f, 1.0f);
  style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.3605149984359741f, 0.3348443806171417f, 0.2475639432668686f, 1.0f);
  style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.0f, 0.0f, 0.0f, 0.5099999904632568f);
  style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.1372549086809158f, 0.1372549086809158f, 0.1372549086809158f, 1.0f);
  style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.01960784383118153f, 0.01960784383118153f, 0.01960784383118153f, 0.5299999713897705f);
  style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.3098039329051971f, 0.3098039329051971f, 0.3098039329051971f, 1.0f);
  style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.407843142747879f, 0.407843142747879f, 0.407843142747879f, 1.0f);
  style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.5098039507865906f, 0.5098039507865906f, 0.5098039507865906f, 1.0f);
  style.Colors[ImGuiCol_CheckMark] = ImVec4(1.0f, 0.8901960849761963f, 0.501960813999176f, 1.0f);
  style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.6352941393852234f, 0.2784313857555389f, 0.4196078479290009f, 1.0f);
  style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.2666666805744171f, 0.3058823645114899f, 0.4470588266849518f, 1.0f);
  style.Colors[ImGuiCol_Button] = ImVec4(0.7019608020782471f, 0.4235294163227081f, 0.06666667014360428f, 1.0f);
  style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.7333333492279053f, 0.3098039329051971f, 0.2705882489681244f, 1.0f);
  style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.6352941393852234f, 0.2784313857555389f, 0.4196078479290009f, 1.0f);
  style.Colors[ImGuiCol_Header] = ImVec4(0.3176470696926117f, 0.2784313857555389f, 0.407843142747879f, 1.0f);
  style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.4156862795352936f, 0.364705890417099f, 0.529411792755127f, 1.0f);
  style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.4039215743541718f, 0.3529411852359772f, 0.5098039507865906f, 1.0f);
  style.Colors[ImGuiCol_Separator] = ImVec4(0.4274509847164154f, 0.4274509847164154f, 0.4980392158031464f, 0.5f);
  style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.2784313857555389f, 0.250980406999588f, 0.3372549116611481f, 1.0f);
  style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.2784313857555389f, 0.250980406999588f, 0.3372549116611481f, 1.0f);
  style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.4509803950786591f, 0.3019607961177826f, 0.4862745106220245f, 1.0f);
  style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.2666666805744171f, 0.3058823645114899f, 0.4470588266849518f, 1.0f);
  style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.1843137294054031f, 0.2823529541492462f, 0.3450980484485626f, 1.0f);
  style.Colors[ImGuiCol_Tab] = ImVec4(0.7019608020782471f, 0.4235294163227081f, 0.06666667014360428f, 1.0f);
  style.Colors[ImGuiCol_TabHovered] = ImVec4(0.7333333492279053f, 0.3098039329051971f, 0.2705882489681244f, 1.0f);
  style.Colors[ImGuiCol_TabActive] = ImVec4(0.1843137294054031f, 0.2823529541492462f, 0.3450980484485626f, 1.0f);
  style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.2666666805744171f, 0.3058823645114899f, 0.4470588266849518f, 1.0f);
  style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.4509803950786591f, 0.3019607961177826f, 0.4862745106220245f, 1.0f);
  style.Colors[ImGuiCol_PlotLines] = ImVec4(0.6078431606292725f, 0.6078431606292725f, 0.6078431606292725f, 1.0f);
  style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.0f, 0.4274509847164154f, 0.3490196168422699f, 1.0f);
  style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.8980392217636108f, 0.6980392336845398f, 0.0f, 1.0f);
  style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.0f, 0.6000000238418579f, 0.0f, 1.0f);
  style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.1882352977991104f, 0.1882352977991104f, 0.2000000029802322f, 1.0f);
  style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.3098039329051971f, 0.3098039329051971f, 0.3490196168422699f, 1.0f);
  style.Colors[ImGuiCol_TableBorderLight] = ImVec4(0.2274509817361832f, 0.2274509817361832f, 0.2470588237047195f, 1.0f);
  style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
  style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.05999999865889549f);
  style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.2588235437870026f, 0.5882353186607361f, 0.9764705896377563f, 0.3499999940395355f);
  style.Colors[ImGuiCol_DragDropTarget] = ImVec4(1.0f, 1.0f, 0.0f, 0.8999999761581421f);
  style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.7333333492279053f, 0.3098039329051971f, 0.2705882489681244f, 1.0f);
  style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.0f, 1.0f, 1.0f, 0.699999988079071f);
  style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.800000011920929f, 0.800000011920929f, 0.800000011920929f, 0.2000000029802322f);
  style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.800000011920929f, 0.800000011920929f, 0.800000011920929f, 0.3499999940395355f);
}

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

  auto &io = ImGui::GetIO();
  io.WantSaveIniSettings = false;

  io.Fonts->AddFontFromFileTTF("../source/resources/InterVariable.ttf", 24.0f, NULL, io.Fonts->GetGlyphRangesJapanese());

  SetupImGuiStyle();

  ImGui_ImplVulkan_Init(&init_info, NULL);

  ImGui_ImplVulkan_CreateFontsTexture();

  ImGui_ImplVulkan_DestroyFontsTexture();
}

void VK_BeginUI(client_t *client) {
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplSDL2_NewFrame(static_cast<SDL_Window *>(CL_GetWindow(client)));

  unsigned screen_width, screen_height;
  unsigned view_width, view_height;

  CL_GetViewDim(client, &screen_width, &screen_height);
  CL_GetScreenDim(client, &view_width, &view_height);

  float ratio_screen_view_x = ((float)screen_width / (float)view_width);
  float ratio_screen_view_y = ((float)screen_height / (float)view_height);

  auto &io = ImGui::GetIO();
  io.DisplayFramebufferScale.x = ratio_screen_view_x;
  io.DisplayFramebufferScale.y = ratio_screen_view_y;

  ImGui::NewFrame();
}

void VK_DrawUI(vk_rend_t *rend, VkCommandBuffer cmd) {
  ImGui::Render();

  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}

void ImGui_ProcessEvent(client_t *client, SDL_Event *event) {
  ImGui_ImplSDL2_ProcessEvent(event);
}

void VK_DestroyUI(vk_rend_t* rend) {
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplSDL2_Shutdown();

  ImGui::DestroyContext();
}
}
