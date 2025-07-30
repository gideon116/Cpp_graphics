#include "imgui_vulkan_init.h"
#include "image_loader.h"
#include <iostream>
#include <cstring>
#include <cstdio>
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <random>

class Player
{
    
    public:
        float m_x, m_y, m_size;
        ImGuiLoadedImage m_tex;
    private:
        float o_x, o_y;
    public:
        Player(float x, float y, float s) : m_x(x), m_y(y), o_x(x), o_y(y), m_size(s) {}
        void position(ImDrawList* draw_list, const ImVec2& display)
        {
            float size = m_size * display.y;
            draw_list->AddImage((ImTextureID)m_tex.descriptor, ImVec2(m_x - size, m_y - size), ImVec2(m_x + size, m_y + size));
            // draw_list->AddRectFilled(ImVec2(m_x - size, m_y - size), ImVec2(m_x + size, m_y + size), IM_COL32(50, 150, 255, 255));

        }

        void loadTex(const char* path="../resources/textures/viking_room.png")
        {
            LoadImGuiImage(path, m_tex);
        }
        
        
        
        void reset()
        {
            m_x = o_x;
            m_y = o_y;
        }

        void moveRight(const ImVec2& display, const float& delta)
        {
            m_x += (m_x < display.x - delta) ? delta : -display.x;
        }
        void moveLeft(const ImVec2& display, const float& delta)
        {
            m_x -= (m_x > delta) ? delta : -display.x;
        }
        void moveUp(const ImVec2& display, const float& delta)
        {
            m_y -= (m_y > delta) ? delta : -display.y;
        }
        void moveDown(const ImVec2& display, const float& delta)
        {
            m_y += (m_y < display.y - delta) ? delta : -display.y;
        }
};

ImVec2 fracCoords(float x, float y)
{
    ImVec2 disp = ImGui::GetIO().DisplaySize;
    return ImVec2(x * disp.x, y * disp.y);
    
}

int main(int, char**)
{
    InitOutput init = initilize();
    auto [window, err, wd] = init;

    const ImVec4 kLoginColor = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    const ImVec4 kHomeColor  = ImVec4(0.12f, 0.18f, 0.12f, 1.00f);
    
    ImVec4 clear_color = kHomeColor;

    ImVec2 disp = ImGui::GetIO().DisplaySize;
    Player bob(100.0, 100.0, 0.05);
    bob.loadTex();

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

    
        {

            ImVec2 disp = ImGui::GetIO().DisplaySize;
            ImGui::SetNextWindowPos({0,0});
            ImGui::SetNextWindowSize(disp);

            ImGui::Begin("##overlay", nullptr, 
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus);

            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            
            ImVec2 frac_xy1 = {0.01f, 0.01f};
            
            ImVec2 size = ImVec2(frac_xy1.x * disp.x, frac_xy1.y * disp.y);
            
            float delta = 100;
            
            bob.position(draw_list, disp);

            if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
                bob.moveRight(disp, delta);

            if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
               bob.moveLeft(disp, delta);

            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
               bob.moveUp(disp, delta);

            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
                bob.moveDown(disp, delta);


            ImGui::SetCursorScreenPos(ImVec2(0.8 * disp.x, 0.8 * disp.y));
            ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32( 50, 150, 255, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32( 30, 120, 220, 255));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32( 20, 100, 200, 255));
            
            frac_xy1 = {0.1f, 0.05f};
            size = ImVec2(frac_xy1.x * disp.x, frac_xy1.y * disp.y);

            if (ImGui::Button("Reset Position", size))
            {
                bob.reset();

            }
            ImGui::PopStyleColor(3);
            ImGui::End();

        }
        

        // Rendering
        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();
        const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
        if (!is_minimized)
        {
            wd->ClearValue.color.float32[0] = clear_color.x * clear_color.w;
            wd->ClearValue.color.float32[1] = clear_color.y * clear_color.w;
            wd->ClearValue.color.float32[2] = clear_color.z * clear_color.w;
            wd->ClearValue.color.float32[3] = clear_color.w;
            FrameRender(wd, draw_data);
            FramePresent(wd);
        }
    }

    // Cleanup
    err = vkDeviceWaitIdle(g_Device);
    check_vk_result(err);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    CleanupVulkanWindow();
    CleanupVulkan();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
