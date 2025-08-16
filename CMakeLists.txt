cmake_minimum_required(VERSION 3.14)
project(wef)

add_subdirectory(dependencies/glfw-3.4)
add_executable(
    
    wef #MACOSX_BUNDLE
    # src/game.cpp
    # src/vulkan_render_model.cpp
    # src/matmul_compute.cpp
    src/example.cpp
    ../learnn/src/useGPU.cpp
    ../learnn/src/layers_multithreaded.cpp
    ../learnn/src/layers.cpp
    ../learnn/src/matrix_operations.cpp
    ../learnn/src/tensor.cpp
    ../learnn/src/model.cpp
    ../learnn/src/layers_gpu.cpp

    dependencies/imgui/imgui.cpp
    dependencies/imgui/imgui_demo.cpp
    dependencies/imgui/imgui_draw.cpp
    dependencies/imgui/imgui_tables.cpp
    dependencies/imgui/imgui_widgets.cpp
    dependencies/imgui/imgui_demo.cpp
    dependencies/imgui/imgui_impl_vulkan.cpp
    dependencies/imgui/imgui_impl_glfw.cpp
)

target_link_libraries(
    wef
    PRIVATE 
        glfw
        ${CMAKE_SOURCE_DIR}/dependencies/vulkan/macOS/lib/libvulkan.1.4.321.dylib
    )

target_include_directories(
    wef
    
    PRIVATE
        include
        dependencies/glfw-3.4/include 
        dependencies/glfw-3.4/deps 
        dependencies/vulkan/macOS/include 
        dependencies/imgui
        dependencies/texture_loaders

        ../learnn/include
    )
