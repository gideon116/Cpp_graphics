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
#include <iostream>

constexpr int w = 10;
class Layer
{
    public:
        
        ImVec2 m_box_size = ImVec2(75, 75);
        ImU32 m_box_col = IM_COL32(255, 100, 100, 200);
        ImVec2 m_box_pos_queue[w];

        void reset()  { for (int i = 0; i < w; i++) m_box_pos_queue[i] = ImVec2(200, 200); }

        Layer()
        { for (int i = 0; i < w; i++) m_box_pos_queue[i] = ImVec2(200, 200); }

        

        void make(ImDrawList* draw_list)
        {
            // render frame
            // for (int i = 0; i < 15; i++) draw_list->AddLine(ImVec2(i*100, 0), ImVec2(i*100, 800), m_box_col);
            draw_list->AddRectFilled(ImVec2(875, 0), ImVec2(900, 800), IM_COL32(100, 100, 100, 200));

            draw_list->AddRectFilled(m_box_pos_queue[w-1], ImVec2(m_box_pos_queue[w-1].x + m_box_size.x, m_box_pos_queue[w-1].y + m_box_size.y), m_box_col);
            ImGui::SetCursorScreenPos(m_box_pos_queue[w-1]);
            ImGui::InvisibleButton("drag_box", m_box_size);

            for (int i = 0; i < w-1; i++)
            {
                m_box_pos_queue[i] = m_box_pos_queue[i + 1];
            }

            // is being dragged
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            {
                ImVec2 temp;
                temp.x = m_box_pos_queue[w-1].x + ImGui::GetIO().MouseDelta.x;
                temp.y = m_box_pos_queue[w-1].y + ImGui::GetIO().MouseDelta.y;
                
                
                m_box_pos_queue[w-1] = temp;
                
            }
            else
            {
                ImVec2 temp;
                if (m_box_pos_queue[w-1].x >= 800)
                    temp.x = 800;
                else
                    // temp.x = m_box_pos_queue[w-1].x + (m_box_pos_queue[w-1].x - m_box_pos_queue[0].x)/9.5;
                    temp.x = m_box_pos_queue[w-1].x + 67;
                temp.y = m_box_pos_queue[w-1].y + (m_box_pos_queue[w-1].y - m_box_pos_queue[0].y)/9.5;

                

                
                m_box_pos_queue[w-1] = temp;
            }

            



            // hilight hover
            // if (ImGui::IsItemHovered())
            //     draw_list->AddRect(m_box_pos, ImVec2(m_box_pos.x + m_box_size.x, m_box_pos.y + m_box_size.y), IM_COL32(255,255,255,255), 0.0f, 0, 2.0f);
        }

};


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
            float sizeX = m_size * display.x * 0.4;
            float sizeY = m_size * display.y;
            draw_list->AddImage((ImTextureID)m_tex.descriptor, ImVec2(m_x - sizeX, m_y - sizeY), ImVec2(m_x + sizeX, m_y + sizeY));
            // draw_list->AddRectFilled(ImVec2(m_x - size, m_y - size), ImVec2(m_x + size, m_y + size), IM_COL32(50, 150, 255, 255));

        }

        void loadTex(const char* path="../resources/textures/texture.jpg")
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
    Layer l;

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
                l.reset();
            }
            ImGui::PopStyleColor(3);

            
            l.make(draw_list);
            
            
            
        
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
