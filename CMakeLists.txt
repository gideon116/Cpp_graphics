cmake_minimum_required(VERSION 3.14)
project(wef)

add_subdirectory(dependencies/glfw-3.4)
add_executable(
    
    wef #MACOSX_BUNDLE
    src/ESX_app.cpp

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
        dependencie/sglfw-3.4/include 
        dependencies/glfw-3.4/deps 
        dependencies/vulkan/macOS/include 
        dependencies/imgui
        dependencies/texture_loaders
    )
