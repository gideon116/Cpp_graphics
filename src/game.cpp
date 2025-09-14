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
#include <thread>
#include <cstdint>

#include "tensor.h"
#include "matrix_operations.h"
#include "mnist.h"
#include "layers.h"
#include "model.h"
#include <implot.h>
#include <mutex>


static int n_test = 100;
static int n_train = 1000;

static Tensor train_im = load_mnist_images("../../learnn/mnist/train-images-idx3-ubyte", n_train);
static Tensor train_l = load_mnist_labels("../../learnn/mnist/train-labels-idx1-ubyte", n_train);

static Tensor test_im = load_mnist_images("../../learnn/mnist/t10k-images-idx3-ubyte", n_test);
static Tensor test_l = load_mnist_labels("../../learnn/mnist/t10k-labels-idx1-ubyte", n_test);

constexpr int WINDOW = 10;
const char* layer_names[6] = {"Linear", "Conv2D", "MaxPool2D", "HMA", "RelU", "Flatten"};

struct LayerGUIParams
{
    LayerGUIParams(ImVec2 size, ImVec2 pos, ImU32 color, const char* layer_name)
        : size(size), pos(pos), color(color), layer_name(layer_name) {}

    ImVec2 size;
    ImVec2 pos;
    ImU32 color;
    int units = 16, k_h = 3, k_w = 3, rand = 3, axis = 0, num_heads = 1;
    bool use_bias = true, training = true, use_gpu = false, keep_dims = false, use_mask = false, self_attention = false;
    std::unique_ptr<Layer> nn;
    
    const char* layer_name;

    void radio_options(const char* names[], std::initializer_list<bool*> settings)
    {
        int i = 0;
        for (bool* option : settings)
            if (ImGui::RadioButton(names[i++], *option))
                    *option = !(*option);
    }

    void settings()
    {
        if (!strcmp(layer_name, "Linear"))
        {
            const char* names[3] = { "Use Bias", "Training", "Use GPU" };
            ImGui::InputInt("Units", &units);
            radio_options(names, {&use_bias, &training, &use_gpu});
        }
        else if (!strcmp(layer_name, "Conv2D"))
        {
            const char* names[3] = { "Use Bias", "Training", "Use GPU" };
            ImGui::InputInt("Units", &units);
            ImGui::InputInt("Kernel H", &k_h);
            ImGui::InputInt("Kernel W", &k_w);
            radio_options(names, {&use_bias, &training, &use_gpu});
        }
        else if (!strcmp(layer_name, "MaxPool2D"))
        {
            const char* names[2] = { "Training", "Use GPU" };
            ImGui::InputInt("Kernel H", &k_h);
            ImGui::InputInt("Kernel W", &k_w);
            radio_options(names, {&training, &use_gpu});
        }
        else if (!strcmp(layer_name, "MHA"))
        {
            const char* names[4] = { "Self Attention", "Use Mask", "Training", "Use GPU" };
            ImGui::InputInt("D model", &units);
            ImGui::InputInt("Num Heads", &num_heads);
            radio_options(names, {&self_attention, &use_mask, &training, &use_gpu});
        }
        else if (!strcmp(layer_name, "RelU"))
        {
            const char* names[2] = { "Training", "Use GPU" };
            radio_options(names, {&training, &use_gpu});
        }
        else if (!strcmp(layer_name, "Flatten"))
        {
            const char* names[2] = { "Training", "Use GPU" };
            radio_options(names, {&training, &use_gpu});
        }
    }

    Layer* make_nn()
    {
        if (!strcmp(layer_name, "Linear"))
            nn = std::make_unique<Linear>(units, use_bias, rand);
        
        else if (!strcmp(layer_name, "Conv2D"))
            nn = std::make_unique<Conv2D>(k_h, k_w, units, use_bias, rand);
        
        else if (!strcmp(layer_name, "MaxPool2D"))
            nn = std::make_unique<MaxPool2D>(k_h, k_w);

        else if (!strcmp(layer_name, "MHA"))
            nn = std::make_unique<MHA>(units, self_attention, num_heads, use_bias, use_mask, use_gpu);
        
        else if (!strcmp(layer_name, "RelU"))
            nn = std::make_unique<ReLU>();
        
        else if (!strcmp(layer_name, "Flatten"))
            nn = std::make_unique<Flatten>();

        return nn.get(); // TODO: is this safe?
    }
};

class LayerGUI
{
    public:

        size_t b_h=25, b_w=175, b_xi=40, b_yi=100;
        size_t t_border=100, b_border=650, l_border=280, r_border=480;
        ImU32 line_color = IM_COL32(0, 0, 0, 255);

        std::vector<LayerGUIParams> gui_layers;
        LayerGUIParams a{ImVec2(b_w, b_h), ImVec2(b_xi, b_yi), IM_COL32(255, 100, 100, 200), "Linear"};
        LayerGUIParams b{ImVec2(b_w, b_h), ImVec2(b_xi, b_yi + b_h), IM_COL32(255, 255, 100, 200), "Conv2D"};
        LayerGUIParams c{ImVec2(b_w, b_h), ImVec2(b_xi, b_yi + b_h*2), IM_COL32(100, 255, 100, 200), "MaxPool2D"};
        LayerGUIParams d{ImVec2(b_w, b_h), ImVec2(b_xi, b_yi + b_h*3), IM_COL32(255, 100, 255, 200), "RelU"};
        LayerGUIParams e{ImVec2(b_w, b_h), ImVec2(b_xi, b_yi + b_h*4), IM_COL32(100, 100, 255, 200), "Flatten"};
        LayerGUIParams f{ImVec2(b_w, b_h), ImVec2(b_xi, b_yi + b_h*5), IM_COL32(255, 100, 100, 200), "MHA"};
        
        std::array<LayerGUIParams*, 6> base_layers= {&a, &b, &c, &d, &e, &f};
        
        size_t added = 650;
        bool toadd = true;
        

        void reset() 
        {
        }

        LayerGUI()
        {
        }

        void draw(ImDrawList* draw_list, LayerGUIParams& object, const float custom_scale)
        {
            draw_list->AddRectFilled(object.pos, ImVec2(object.pos.x + object.size.x, object.pos.y + object.size.y), object.color);
            draw_list->AddRect(object.pos, ImVec2(object.pos.x + object.size.x, object.pos.y + object.size.y), IM_COL32(0, 0, 0, 255));
            draw_list->AddText(ImVec2(object.pos.x + object.size.x*0.1, object.pos.y + object.size.y*0.1), IM_COL32(0, 0, 0, 255), object.layer_name);
        }

        bool dragging(const LayerGUIParams& object)
        {
            ImGui::SetCursorScreenPos(object.pos);
            ImGui::InvisibleButton(object.layer_name, object.size);
            return ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left);
        }

        void add_LayerGUI(const LayerGUIParams& object, bool& toadd)
        {
            if (toadd)
            {
                added -= b_h;
                gui_layers.emplace_back(object.size, object.pos, object.color, object.layer_name);
                toadd = false;
            }
            gui_layers.back().pos.x += ImGui::GetIO().MouseDelta.x;
            gui_layers.back().pos.y += ImGui::GetIO().MouseDelta.y;
        }

        void make(ImDrawList* draw_list, const ImVec2& displaysize, const float custom_scale)
        {
            // draw layers in the model
            for (int i = 0; i < gui_layers.size(); i++)
                draw(draw_list, gui_layers[i], custom_scale);
            
            // draw base layers
            bool dragged = false;
            for (auto& i : base_layers)
            {
                draw(draw_list, *i, custom_scale);
                if (dragging(*i))
                {
                    add_LayerGUI(*i, toadd);
                    dragged = true;
                }
            }
            
            if (!dragged && !gui_layers.empty())
            {
                if (((l_border - b_w) < gui_layers.back().pos.x && r_border > gui_layers.back().pos.x) &&
                    ((t_border - b_h) < gui_layers.back().pos.y && b_border > gui_layers.back().pos.y))
                {
                    gui_layers.back().pos.x = l_border + (r_border - l_border)/2 - b_w/2;
                    gui_layers.back().pos.y = added;
                    toadd = true;
                }
                if (t_border > gui_layers.back().pos.y)
                {
                    gui_layers.pop_back();
                    added += b_h;
                    toadd = true;
                }
                else if (!toadd)
                {
                    gui_layers.pop_back();
                    added += b_h;
                    toadd = true;
                }
            }
            
            // square where the LayerGUIs are placed
            draw_list->AddRect(ImVec2(l_border, t_border), ImVec2(r_border, b_border), line_color);
            draw_list->AddRect(ImVec2(r_border*1.2, t_border*4.5), ImVec2(r_border*2.5, b_border), line_color);

            // separator
            // TODO : *0.3 is hard coded, change this
            draw_list->AddLine(ImVec2(r_border*0.5, 0), ImVec2(r_border*0.5, displaysize.y), line_color);
            draw_list->AddLine(ImVec2(r_border*1.1, 0), ImVec2(r_border*1.1, displaysize.y), line_color);

            // dragging the gui_layers
            for (auto it = gui_layers.begin(); it != gui_layers.end(); )
            {

                // expand layer settings
                ImGui::PushID(&(*it));
                ImGui::SetCursorScreenPos(it->pos);

                if (ImGui::InvisibleButton("##settings_ib", ImVec2(it->size.x - 20, it->size.y)))
                    ImGui::OpenPopup("settings");
                if (ImGui::BeginPopup("settings", ImGuiWindowFlags_Popup))
                {
                    it->settings();
                    ImGui::EndPopup();
                }

                ImGui::PopID();

                // remove layer
                ImGui::PushID(&(*it));
                ImGui::SetCursorScreenPos(ImVec2(it->pos.x + it->size.x - 20, it->pos.y + 2));
                if (ImGui::SmallButton("X"))
                {
                    added += b_h;
                    gui_layers.erase(it);
                    for (auto& LayerGUI : gui_layers)
                        if (LayerGUI.pos.y <= it->pos.y)
                            LayerGUI.pos.y += b_h;
                }
                else
                    it++;

                ImGui::PopID();
            }
            
        }
};

class PhyBox
{
    public:
        
        ImVec2 m_box_size = ImVec2(75, 75);
        ImU32 m_box_col = IM_COL32(255, 100, 100, 200);
        ImVec2 m_box_pos_queue[WINDOW];
        
        ImGuiLoadedImage m_tex;

        void reset() 
        {
            for (int i = 0; i < WINDOW; i++)
                m_box_pos_queue[i] = ImVec2(200, 200);
        }

        PhyBox()
        { 
            for (int i = 0; i < WINDOW; i++)
                m_box_pos_queue[i] = ImVec2(200, 200); 
            
            LoadImGuiImage(test_im.m_tensor, 28, 28, 1, m_tex);
        }

        void make(ImDrawList* draw_list)
        {
            // grid lines
            for (int i = 0; i < 10; i++) draw_list->AddLine(ImVec2(i*100, 0), ImVec2(i*100, 800), m_box_col);
            for (int i = 0; i < 10; i++) draw_list->AddLine(ImVec2(0, i*100), ImVec2(900, i*100), m_box_col);

            // separator
            draw_list->AddRectFilled(ImVec2(900, 0), ImVec2(925, 800), IM_COL32(100, 100, 100, 200));

            size_t t_x, t_y, a, b;
            size_t start = 100;
            size_t end = 370;

            
            draw_list->AddImage((ImTextureID)m_tex.descriptor, ImVec2(200, 200), ImVec2(300, 300));

            // draw_list->AddRectFilled(ImVec2(a, b), ImVec2(a + 10, b + 10), IM_COL32(200* test_im.tensor[i], 200, 200, 200));

            // box
            draw_list->AddRectFilled(
                                    m_box_pos_queue[WINDOW-1],
                                    ImVec2(
                                        m_box_pos_queue[WINDOW-1].x + m_box_size.x,
                                        m_box_pos_queue[WINDOW-1].y + m_box_size.y),
                                    m_box_col);

            ImGui::SetCursorScreenPos(m_box_pos_queue[WINDOW-1]);
            ImGui::InvisibleButton("drag_box", m_box_size);

            for (int i = 0; i < WINDOW-1; i++)
            {
                m_box_pos_queue[i] = m_box_pos_queue[i + 1];
            }

            // is being dragged
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            {
                ImVec2 temp;
                temp.x = m_box_pos_queue[WINDOW-1].x + ImGui::GetIO().MouseDelta.x;
                temp.y = m_box_pos_queue[WINDOW-1].y + ImGui::GetIO().MouseDelta.y;
                
                m_box_pos_queue[WINDOW-1] = temp;
            }
            else
            {
                ImVec2 temp;
                if (m_box_pos_queue[WINDOW-1].x >= 800)
                    temp.x = 800;
                else
                    temp.x = m_box_pos_queue[WINDOW-1].x + (m_box_pos_queue[WINDOW-1].x - m_box_pos_queue[0].x)/9.5;
                temp.y = m_box_pos_queue[WINDOW-1].y + (m_box_pos_queue[WINDOW-1].y - m_box_pos_queue[0].y)/9.5;

                m_box_pos_queue[WINDOW-1] = temp;
            }
        }

};

class PLayerGUI
{
    
    public:
        float m_x, m_y, m_size;
        ImGuiLoadedImage m_tex;
    private:
        float o_x, o_y;
    public:
        PLayerGUI(float x, float y, float s) : m_x(x), m_y(y), o_x(x), o_y(y), m_size(s) {}
        
        void position(ImDrawList* draw_list, const ImVec2& display)
        {
            float sizeX = m_size * display.x * 0.6;
            float sizeY = m_size * display.y;
            draw_list->AddImage((ImTextureID)m_tex.descriptor, ImVec2(m_x - sizeX, m_y - sizeY), ImVec2(m_x + sizeX, m_y + sizeY));
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

int main(int, char**)
{
    InitOutput init = initilize();
    ImPlot::CreateContext();
    auto [window, err, wd] = init;

    
    ImVec4 clear_color = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);

    // PLayerGUI bob(100.0, 100.0, 0.05);
    // bob.loadTex();
    // PhyBox l;

    LayerGUI layer_gui;
    float lr = 0.05f;
    std::unique_ptr<Model> model;
    std::thread training_thread;
    bool training = false;
    int epochs = 10;
    int mini_batch_size = 0;
    std::vector<Layer*> network;
    std::vector<float> loss_hist;
    std::vector<float> val_loss_hist;

    ImGuiLoadedImage m_tex;
    ImGuiLoadedImage m_tex_2;

    std::mutex m_;
    
    float *x_data = nullptr, *y_data = nullptr, *val_y_data = nullptr;
    int data_size = 0;

    LoadImGuiImage(test_im.m_tensor, 28, 28, 1, m_tex);
    LoadImGuiImage(test_im.m_tensor + 28 * 28, 28, 28, 1, m_tex_2);
    bool display_test = false;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // Resize swap chain?
        int fb_width, fb_height;
        glfwGetFramebufferSize(window, &fb_width, &fb_height);
        if (fb_width > 0 && fb_height > 0 && (g_SwapChainRebuild || g_MainWindowData.Width != fb_width || g_MainWindowData.Height != fb_height))
        {
            ImGui_ImplVulkan_SetMinImageCount(g_MinImageCount);
            ImGui_ImplVulkanH_CreateOrResizeWindow(g_Instance, g_PhysicalDevice, g_Device, &g_MainWindowData, g_QueueFamily, g_Allocator, fb_width, fb_height, g_MinImageCount);
            g_MainWindowData.FrameIndex = 0;
            g_SwapChainRebuild = false;
        }
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
        {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        // Start the Dear ImGui frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
#if 1
        {
            ImVec2 display_size = ImGui::GetIO().DisplaySize;
            
            ImGui::SetNextWindowPos({0,0});
            ImGui::SetNextWindowSize(display_size);
            ImGui::Begin("window", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground);

            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            
            ImGuiStyle& style = ImGui::GetStyle();
            float custom_scale = 1.6f;
            ImGui::PushFont(NULL, style.FontSizeBase * custom_scale);
            layer_gui.make(draw_list, display_size, custom_scale);
            ImGui::PopFont();

            

            ImGui::SetCursorScreenPos(ImVec2(40, 650-240));
            ImGui::SetNextItemWidth(150.0f);
            ImGui::Text("Mini Batch Size");
            ImGui::SetNextItemWidth(150.0f);
            ImGui::SetCursorScreenPos(ImVec2(40, 650-220));
            ImGui::SliderInt("##Mini Batch Size", &mini_batch_size, 0, 10000, "%d", ImGuiSliderFlags_Logarithmic);

            ImGui::SetCursorScreenPos(ImVec2(40, 650-180));
            ImGui::SetNextItemWidth(150.0f);
            ImGui::Text("Epochs");
            ImGui::SetNextItemWidth(150.0f);
            ImGui::SetCursorScreenPos(ImVec2(40, 650-160));
            ImGui::SliderInt("##Epochs", &epochs, 0, 10000, "%d", ImGuiSliderFlags_Logarithmic);

            

            ImGui::SetCursorScreenPos(ImVec2(40, 650-120));
            ImGui::SetNextItemWidth(150.0f);
            ImGui::Text("Learning Rate");
            ImGui::SetNextItemWidth(150.0f);
            ImGui::SetCursorScreenPos(ImVec2(40, 650-100));
            ImGui::SliderFloat("##Learning Rate", &lr, 0.0f, 1.0f, "%.4f", ImGuiSliderFlags_Logarithmic);

            ImGui::SetCursorScreenPos(ImVec2(40, 650-40));
            if (ImGui::Button("Begin Training", ImVec2(150, 40)))
            {
                if (!training)
                {
                    if (training_thread.joinable()) 
                        training_thread.join();

                    network.clear();
                    for (auto& i : layer_gui.gui_layers)
                        network.push_back(i.make_nn());
                        
                    training = true;
                    model = std::make_unique<Model>(network);

                    training_thread = std::thread([&]()
                    {
                        m_.lock();
                        y_data = nullptr;
                        val_y_data = nullptr;
                        data_size = 0;

                        loss_hist.clear();
                        val_loss_hist.clear();
                        m_.unlock();
                        
                        model->fit(train_l, train_im, test_l, test_im, epochs, lr, mini_batch_size, &loss_hist, &val_loss_hist, &m_);
                        
                        training = false;
                        
                        Tensor pred = model->predict(test_im);  
                        display_test = true;

                    });
                }
            }
            
            
            
            
            float l_min=10000.0f, l_max=0.0f;
            std::vector<float> xs;
            xs.reserve(loss_hist.size());

            for (size_t i = 0; i < loss_hist.size(); i++)
            {
                xs.push_back((float)i);
                
                l_min = std::min(std::min(l_min, loss_hist[i]), val_loss_hist[i]);
                l_max = std::max(std::max(l_max, loss_hist[i]), val_loss_hist[i]);
            }

            ImGui::SetCursorScreenPos(ImVec2(580, 100));
            ImVec2 plot_size(620, 300);

            m_.lock();

            x_data = xs.data();
            y_data = loss_hist.data();
            val_y_data = val_loss_hist.data();
            data_size = (int)loss_hist.size();


            if (ImPlot::BeginPlot("Losses", plot_size))
            {
                ImPlot::SetupAxes("epochs", "loss");

                if (!loss_hist.empty())
                {
                    ImPlot::SetupAxisLimits(ImAxis_X1, 0, x_data[data_size-1], ImPlotCond_Always);
                    ImPlot::SetupAxisLimits(ImAxis_Y1, l_min, l_max, ImPlotCond_Always);
                    ImPlot::PlotLine("Training Loss", x_data, y_data, data_size);
                    ImPlot::PlotLine("Validation Loss", x_data, val_y_data, data_size);
                    ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle);
                }
                
                ImPlot::EndPlot();
            }
            m_.unlock();
            

            if (display_test)
            {
                draw_list->AddImage((ImTextureID)m_tex.descriptor, ImVec2(580, 425), ImVec2(780, 625), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(0,0,0,255));
                draw_list->AddImage((ImTextureID)m_tex_2.descriptor, ImVec2(580 + 200, 425), ImVec2(780 + 200, 625), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(0,0,0,255));
            }
            
            

            ImGui::End();

        }
        {
            // ImVec2 disp = ImGui::GetIO().DisplaySize;
            // ImGui::SetNextWindowPos({0,0});
            // ImGui::SetNextWindowSize(disp);

            // ImGui::Begin("##overlay", nullptr);
            // ImDrawList* draw_list = ImGui::GetWindowDrawList();
            

            // ImGui::End();
        }
#endif
#if 0
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
#endif

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
    ImPlot::DestroyContext();

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
