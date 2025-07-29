#define GLFW_INCLUDE_VULKAN
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"


#include <chrono>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <cstdint>
#include <limits>
#include <array>
#include <unordered_map>

#include "application.h"


#ifdef NDEBUG
    const bool enableValidationLayers = true;
#else
    const bool enableValidationLayers = true;
#endif

const bool verbose = false;
const int MAX_FRAMES_IN_FLIGHT = 2;
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 800;
constexpr uint32_t num_infos = 2;

// device extensions
constexpr uint32_t num_req_deviceExtensions = 2;
const char* req_deviceExtensions[num_req_deviceExtensions] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME, 
                                                                "VK_KHR_portability_subset" // macOS
                                                            };

// validations -- optional
constexpr uint32_t num_req_validationLayers = 1;
const char* req_validationLayers[num_req_validationLayers] = {"VK_LAYER_KHRONOS_validation"};

const char* VER_SHADER_PATH = "../resources/shaders/vert.spv";
const char* FRAG_SHADER_PATH = "../resources/shaders/frag.spv";
const char* TEXTURE_PATH = "../resources/textures/viking_room.png";
const char* MODEL_PATH = "../resources/models/viking_room.obj";

// vertices
// const num_ptr_arr<Vertex> vertices = {
//     {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
//     {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
//     {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
//     {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},

//     {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
//     {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
//     {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
//     {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
// };

// const num_ptr_arr<uint16_t> indices = {
//     0, 1, 2, 2, 3, 0,
//     4, 5, 6, 6, 7, 4
// };

struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::vec4 texmod;
};



class HelloWorld
{
    public:
        void run()
        {
            initWindow();
            initVulkan();
            mainLoop();
            cleanup();
        }

    private:
        void initWindow()
        {
            glfwInit();

            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

            m_window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "wef", nullptr, nullptr);
            glfwSetWindowUserPointer(m_window, this);
            glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallback);

        }

        void mainLoop()
        {
            while (!glfwWindowShouldClose(m_window))
            {
                glfwPollEvents();
                drawFrame();
            }
            vkDeviceWaitIdle(m_device);
        }

        void cleanup()
        {
            cleanupSwapChain();

            vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
            vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
            vkDestroyRenderPass(m_device, m_renderPass, nullptr);

            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            {
                vkDestroyBuffer(m_device, m_uniformBuffers.ptr[i], nullptr);
                vkFreeMemory(m_device, m_uniformBuffersMemory.ptr[i], nullptr);
            }
            vkDestroySampler(m_device, m_textureSampler, nullptr);
            vkDestroyImageView(m_device, m_textureImageView, nullptr);

            vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
            vkDestroyImage(m_device, m_textureImage, nullptr);
            vkFreeMemory(m_device, m_textureImageMemory, nullptr);
            vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);


            vkDestroyBuffer(m_device, m_indexBuffer, nullptr);
            vkFreeMemory(m_device, m_indexBufferMemory, nullptr);

            vkDestroyBuffer(m_device, m_vertexBuffer, nullptr);
            vkFreeMemory(m_device, m_vertexBufferMemory, nullptr);

            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            {
                vkDestroySemaphore(m_device, m_renderFinishedSemaphores.ptr[i], nullptr);
                vkDestroySemaphore(m_device, m_imageAvailableSemaphores.ptr[i], nullptr);
                vkDestroyFence(m_device, m_inFlightFences.ptr[i], nullptr);
            }

            vkDestroyCommandPool(m_device, m_commandPool, nullptr);
            vkDestroyDevice(m_device, nullptr);
            
            vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
            vkDestroyInstance(m_instance, nullptr);
            
        
            glfwDestroyWindow(m_window);
            glfwTerminate();
        }

        // vulkan stuff
        static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
        {
            auto app = reinterpret_cast<HelloWorld*>(glfwGetWindowUserPointer(window));
            app->m_framebufferResized = true;
        }

        void createInstance()
        {
            // validation in debug
            if (enableValidationLayers && !validationCheck())
                throw std::runtime_error("validation layers requested, but not available!");
            
            // begin
            VkApplicationInfo appInfo{};
            appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            appInfo.pApplicationName = "wef desktop app";
            appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
            appInfo.pEngineName = "No Engine";
            appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
            appInfo.apiVersion = VK_API_VERSION_1_0;

            VkInstanceCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            createInfo.pApplicationInfo = &appInfo;

            uint32_t glfwExtensionCount = 0;
            const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

            // if validation on in debug
            if (enableValidationLayers) 
            {
                createInfo.enabledLayerCount = num_req_validationLayers;
                createInfo.ppEnabledLayerNames = req_validationLayers;
            } 
            else createInfo.enabledLayerCount = 0;

            // macOS stuff
            createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
            const char** new_glfwExtensions = new const char*[glfwExtensionCount + 3];
            for (uint32_t i = 0; i < glfwExtensionCount; i++) new_glfwExtensions[i] = glfwExtensions[i];
            new_glfwExtensions[glfwExtensionCount++] = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
            new_glfwExtensions[glfwExtensionCount++] = "VK_KHR_portability_enumeration";
            new_glfwExtensions[glfwExtensionCount++] = "VK_KHR_get_physical_device_properties2";

            // back to main...
            createInfo.enabledExtensionCount = glfwExtensionCount;
            createInfo.ppEnabledExtensionNames = new_glfwExtensions;

            VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);
            if (result != VK_SUCCESS) throw std::runtime_error("failed to create instance!");

            delete[] new_glfwExtensions;

            // vk extensions
            uint32_t vk_extension_ct = 0;
            vkEnumerateInstanceExtensionProperties(nullptr, &vk_extension_ct, nullptr);
            VkExtensionProperties* all_vk_Extensions = new VkExtensionProperties[vk_extension_ct];
            vkEnumerateInstanceExtensionProperties(nullptr, &vk_extension_ct, all_vk_Extensions);
            
            if (verbose)
            {
                std::cout << "available extensions:\n";
                for (uint32_t i = 0; i < vk_extension_ct; i++) std::cout << '\t' << all_vk_Extensions[i].extensionName << '\n';
            }
            
            delete[] all_vk_Extensions;

        }

        bool validationCheck() // debug only
        {
            // get avaliable layers
            uint32_t num_avaliable = 0;
            vkEnumerateInstanceLayerProperties(&num_avaliable, nullptr);

            VkLayerProperties* avaliable = new VkLayerProperties[num_avaliable];
            vkEnumerateInstanceLayerProperties(&num_avaliable, avaliable);

            
            // check if req in avaliable
            for (uint32_t i = 0; i < num_req_validationLayers; i++)
            {
                bool layerFound = false;
                for (uint32_t j = 0; j < num_avaliable; j++)
                {
                    if (strcmp(req_validationLayers[i], avaliable[j].layerName) == 0)
                    { layerFound = true; break; }
                }

                if (!layerFound) return false;
            }

            delete[] avaliable;
            return true;
        }

        void pickPhysicalDevice()
        {
            // get gpus
            uint32_t num_physicalDevices = 0;
            vkEnumeratePhysicalDevices(m_instance, &num_physicalDevices, nullptr);
            if (num_physicalDevices == 0) throw std::runtime_error("failed to find GPUs with Vulkan support!");
            if (verbose) std::cout << "number of gpus: " << num_physicalDevices << '\n';

            VkPhysicalDevice* gpus = new VkPhysicalDevice[num_physicalDevices];
            vkEnumeratePhysicalDevices(m_instance, &num_physicalDevices, gpus);

            for (uint32_t i = 0; i < num_physicalDevices; i++)
                if (isGPUsuitable(gpus[i])) 
                    { 
                        m_physicalDevice = gpus[i];
                        m_msaaSamples = getMaxUsableSampleCount();
                        break;  // only get first gpu
                    }

            if (m_physicalDevice == VK_NULL_HANDLE) throw std::runtime_error("failed to find a suitable GPU!");

            delete[] gpus;
            
        }

        bool isGPUsuitable(VkPhysicalDevice gpu)
        {
            VkPhysicalDeviceProperties req_Properties;
            vkGetPhysicalDeviceProperties(gpu, &req_Properties);

            VkPhysicalDeviceFeatures opt_features;
            vkGetPhysicalDeviceFeatures(gpu, &opt_features);

            bool good = true;

            // examples
            // good &= req_Properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
            // good &= opt_features.geometryShader;
            good &= opt_features.samplerAnisotropy;

            QueueFamilyIndices indices = findQueueFamilies(gpu);
            good &= indices.hasvalue();

            bool extensionsSupported = checkDeviceExtensionSupport(gpu);
            good &= extensionsSupported;

            // verify that swap chain support is adequate
            bool swapChainAdequate = false;
            if (extensionsSupported)
            {
                SwapChainSupportDetails swapChainSupport = querySwapChainSupport(gpu);
                swapChainAdequate = swapChainSupport.formats.ptr && swapChainSupport.presentModes.ptr;
            }

            good &= swapChainAdequate;


            return good;
        }

        bool checkDeviceExtensionSupport(VkPhysicalDevice gpu)
        {

            uint32_t extensionCount = 0;
            vkEnumerateDeviceExtensionProperties(gpu, nullptr, &extensionCount, nullptr);

            VkExtensionProperties* availableExtensions = new VkExtensionProperties[extensionCount];
            vkEnumerateDeviceExtensionProperties(gpu, nullptr, &extensionCount, availableExtensions);

            // check if req in avaliable
            for (uint32_t i = 0; i < num_req_deviceExtensions; i++)
            {
                bool extensionFound = false;
                for (uint32_t j = 0; j < extensionCount; j++)
                {
                    if (strcmp(req_deviceExtensions[i], availableExtensions[j].extensionName) == 0)
                    { extensionFound = true; break; }
                }

                if (!extensionFound) return false;
            }

            delete[] availableExtensions;
            return true;

        }

        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice gpu)
        {
            QueueFamilyIndices indices;

            uint32_t num_queue_fam = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(gpu, &num_queue_fam, nullptr);
            VkQueueFamilyProperties* queue_fam = new VkQueueFamilyProperties[num_queue_fam];
            vkGetPhysicalDeviceQueueFamilyProperties(gpu, &num_queue_fam, queue_fam);

            uint32_t ct = 0;
            for (uint32_t i = 0; i < num_queue_fam; i++)
            {
                    if (queue_fam[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) 
                        indices.graphicsFamily = ct;

                    VkBool32 supported = false;
                    vkGetPhysicalDeviceSurfaceSupportKHR(gpu, ct, m_surface, &supported);
                    if (supported) indices.presentFamily = ct;

                    ct++;
            }

            delete[] queue_fam;
            return indices;
        }

        void createLogicalDevice()
        {
            QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);

            num_ptr_arr<VkDeviceQueueCreateInfo> queueCreateInfos(num_infos);
            uint32_t uniqueQueueFamilies[num_infos] = {indices.graphicsFamily_value(), indices.presentFamily_value()};

            float queuePriority = 1.0f;
            for (uint32_t i = 0; i < num_infos; i++) {
                auto queueFamily = uniqueQueueFamilies[i];
                VkDeviceQueueCreateInfo queueCreateInfo{};
                queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queueCreateInfo.queueFamilyIndex = queueFamily;
                queueCreateInfo.queueCount = 1;
                queueCreateInfo.pQueuePriorities = &queuePriority;
                queueCreateInfos.ptr[i] = queueCreateInfo;
            }

            VkPhysicalDeviceFeatures deviceFeatures{};
            deviceFeatures.samplerAnisotropy = VK_TRUE;
            deviceFeatures.sampleRateShading = VK_TRUE;

            VkDeviceCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

            createInfo.queueCreateInfoCount = queueCreateInfos.len;
            createInfo.pQueueCreateInfos = queueCreateInfos.ptr;

            createInfo.pEnabledFeatures = &deviceFeatures;

            createInfo.enabledExtensionCount = num_req_deviceExtensions;
            createInfo.ppEnabledExtensionNames = req_deviceExtensions;

            if (enableValidationLayers) {
                createInfo.enabledLayerCount = num_req_validationLayers;
                createInfo.ppEnabledLayerNames = req_validationLayers;
            } else {
                createInfo.enabledLayerCount = 0;
            }

            if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS)
                throw std::runtime_error("failed to create logical device!");
    


            vkGetDeviceQueue(m_device, indices.graphicsFamily_value(), /*queue index*/0, &m_graphicsQueue);
            vkGetDeviceQueue(m_device, indices.presentFamily_value(), /*queue index*/0, &m_presentQueue);

        }

        // swap chains
        SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice gpu)
        {
            SwapChainSupportDetails details;
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, m_surface, &details.capabilities);

            uint32_t formatCount;
            vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, m_surface, &formatCount, nullptr);
            if (formatCount != 0)
            {
                details.formats.resize(formatCount);
                vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, m_surface, &formatCount, details.formats.ptr);
            }

            uint32_t presentModeCount;
            vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, m_surface, &presentModeCount, nullptr);
            if (presentModeCount != 0) {
                details.presentModes.resize(presentModeCount);;
                vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, m_surface, &presentModeCount, details.presentModes.ptr);
            }


            return details;
        }
        
        VkSurfaceFormatKHR chooseSwapSurfaceFormat(const VkSurfaceFormatKHR availableFormats[], uint32_t num_avaliable_formats)
        {
            for (uint32_t i = 0; i < num_avaliable_formats; i++)
            {
                auto& availableFormat = availableFormats[i];
                if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) 
                    return availableFormat;
            }
            return availableFormats[0];
        }
        
        VkPresentModeKHR chooseSwapPresentMode(const VkPresentModeKHR availablePresentModes[], uint32_t num_avaliable_presentModes)
        {
            for (uint32_t i = 0; i < num_avaliable_presentModes; i++)
            {
                auto& availablePresentMode = availablePresentModes[i];
                if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) 
                    return availablePresentMode;
            }

            return VK_PRESENT_MODE_FIFO_KHR;
        }
        
        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
        {

            if (capabilities.currentExtent.width != UINT32_MAX)
                return capabilities.currentExtent;
            else {
                int width, height;
                glfwGetFramebufferSize(m_window, &width, &height);

                VkExtent2D actualExtent = {
                    static_cast<uint32_t>(width),
                    static_cast<uint32_t>(height)
                };

                actualExtent.width = (actualExtent.width < capabilities.minImageExtent.width) ? capabilities.minImageExtent.width :
                                     (actualExtent.width > capabilities.maxImageExtent.width) ? capabilities.maxImageExtent.width :
                                     actualExtent.width;

                actualExtent.height = (actualExtent.height < capabilities.minImageExtent.height) ? capabilities.minImageExtent.height :
                                      (actualExtent.height > capabilities.maxImageExtent.height) ? capabilities.maxImageExtent.height :
                                      actualExtent.height;

                return actualExtent;
            }
        }

        void createSwapChain()
        {
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(m_physicalDevice);

            VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats.ptr, swapChainSupport.formats.len);
            VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes.ptr, swapChainSupport.presentModes.len);
            VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

            // how many images we would like to have in the swap chain
            uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
            if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
                imageCount = swapChainSupport.capabilities.maxImageCount;

            // create structure
            VkSwapchainCreateInfoKHR createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
            createInfo.surface = m_surface;

            createInfo.minImageCount = imageCount;
            createInfo.imageFormat = surfaceFormat.format;
            createInfo.imageColorSpace = surfaceFormat.colorSpace;
            createInfo.imageExtent = extent;
            createInfo.imageArrayLayers = 1;
            createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

            // amount of layers each image consists of
            QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);
            uint32_t queueFamilyIndices[] = {indices.graphicsFamily_value(), indices.presentFamily_value()};

            if (indices.graphicsFamily != indices.presentFamily)
            {
                createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
                createInfo.queueFamilyIndexCount = 2;
                createInfo.pQueueFamilyIndices = queueFamilyIndices;
            } 
            else 
            {
                createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
                createInfo.queueFamilyIndexCount = 0; // optional
                createInfo.pQueueFamilyIndices = nullptr; // optional
            }

            // specify how to handle swap chain images that will be used across multiple queue families
            createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
            createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
            createInfo.presentMode = presentMode;
            createInfo.clipped = VK_TRUE;
            createInfo.oldSwapchain = VK_NULL_HANDLE;

        
            if (vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapChain) != VK_SUCCESS)
                throw std::runtime_error("failed to create swap chain!");

            
            vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, nullptr);
            m_swapChainImages.resize(imageCount);
            vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, m_swapChainImages.ptr);

            m_swapChainImageFormat = surfaceFormat.format;
            m_swapChainExtent = extent;


        }

        void createSurface()
        {
            if (glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface) != VK_SUCCESS)
                throw std::runtime_error("failed to create window surface!");

        }

        void createImageViews()
        {
            m_swapChainImageViews.resize(m_swapChainImages.len);
            for (size_t i = 0; i < m_swapChainImages.len; i++) 
            {
                m_swapChainImageViews.ptr[i] = createImageView(m_swapChainImages.ptr[i], m_swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);

            }
        }

        VkShaderModule createShaderModule(const num_ptr_arr<char>& code)
        {
            VkShaderModuleCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            createInfo.codeSize = code.len;
            // TO DO : CHECK THIS LEN IS OK?
            createInfo.pCode = reinterpret_cast<const uint32_t*>(code.ptr);

            VkShaderModule shaderModule;
            if (vkCreateShaderModule(m_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
                throw std::runtime_error("failed to create shader module!");
            
            return shaderModule;
        }

        void createGraphicsPipeline()
        {
            auto vertShaderCode = readFile(VER_SHADER_PATH);
            auto fragShaderCode = readFile(FRAG_SHADER_PATH);

            VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
            VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

            VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
            vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
            vertShaderStageInfo.module = vertShaderModule;
            vertShaderStageInfo.pName = "main";

            VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
            fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            fragShaderStageInfo.module = fragShaderModule;
            fragShaderStageInfo.pName = "main";

            VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

            VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
            vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

            auto bindingDescription = Vertex::getBindingDescription();
            auto attributeDescriptions = Vertex::getAttributeDescriptions();

            vertexInputInfo.vertexBindingDescriptionCount = 1;
            vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
            vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
            vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

            VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
            inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            inputAssembly.primitiveRestartEnable = VK_FALSE;

            VkPipelineViewportStateCreateInfo viewportState{};
            viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewportState.viewportCount = 1;
            viewportState.scissorCount = 1;

            VkPipelineRasterizationStateCreateInfo rasterizer{};
            rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            rasterizer.depthClampEnable = VK_FALSE;
            rasterizer.rasterizerDiscardEnable = VK_FALSE;
            rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
            rasterizer.lineWidth = 1.0f;
            rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
            rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            rasterizer.depthBiasEnable = VK_FALSE;

            VkPipelineMultisampleStateCreateInfo multisampling{};
            multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            multisampling.rasterizationSamples = m_msaaSamples;
            multisampling.sampleShadingEnable = VK_TRUE; // enable sample shading in the pipeline
            multisampling.minSampleShading = .2f; // min fraction for sample shading; closer to one is smoother

            VkPipelineDepthStencilStateCreateInfo depthStencil{};
            depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            depthStencil.depthTestEnable = VK_TRUE;
            depthStencil.depthWriteEnable = VK_TRUE;
            depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
            depthStencil.depthBoundsTestEnable = VK_FALSE;
            depthStencil.minDepthBounds = 0.0f; // optional
            depthStencil.maxDepthBounds = 1.0f; // optional
            depthStencil.stencilTestEnable = VK_FALSE;
            depthStencil.front = {}; // optional
            depthStencil.back = {}; // optional
            
            VkPipelineColorBlendAttachmentState colorBlendAttachment{};
            colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            colorBlendAttachment.blendEnable = VK_FALSE;

            VkPipelineColorBlendStateCreateInfo colorBlending{};
            colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            colorBlending.logicOpEnable = VK_FALSE;
            colorBlending.logicOp = VK_LOGIC_OP_COPY;
            colorBlending.attachmentCount = 1;
            colorBlending.pAttachments = &colorBlendAttachment;
            colorBlending.blendConstants[0] = 0.0f;
            colorBlending.blendConstants[1] = 0.0f;
            colorBlending.blendConstants[2] = 0.0f;
            colorBlending.blendConstants[3] = 0.0f;

            constexpr size_t num_dynamicStates = 2;
            VkDynamicState dynamicStates[num_dynamicStates] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
            VkPipelineDynamicStateCreateInfo dynamicState{};
            dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dynamicState.dynamicStateCount = num_dynamicStates;
            dynamicState.pDynamicStates = dynamicStates;

            VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
            pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = 1;
            pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;

            if (vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS)
                throw std::runtime_error("failed to create pipeline layout!");

            VkGraphicsPipelineCreateInfo pipelineInfo{};
            pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pipelineInfo.stageCount = 2;
            pipelineInfo.pStages = shaderStages;
            pipelineInfo.pVertexInputState = &vertexInputInfo;
            pipelineInfo.pInputAssemblyState = &inputAssembly;
            pipelineInfo.pViewportState = &viewportState;
            pipelineInfo.pRasterizationState = &rasterizer;
            pipelineInfo.pMultisampleState = &multisampling;
            pipelineInfo.pColorBlendState = &colorBlending;
            pipelineInfo.pDynamicState = &dynamicState;
            pipelineInfo.layout = m_pipelineLayout;
            pipelineInfo.renderPass = m_renderPass;
            pipelineInfo.subpass = 0;
            pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
            pipelineInfo.pDepthStencilState = &depthStencil;

            if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_graphicsPipeline) != VK_SUCCESS)
                throw std::runtime_error("failed to create graphics pipeline!");
        
            vkDestroyShaderModule(m_device, fragShaderModule, nullptr);
            vkDestroyShaderModule(m_device, vertShaderModule, nullptr);
            
        }

        void createRenderPass()
        {
            VkAttachmentDescription colorAttachment{};
            colorAttachment.format = m_swapChainImageFormat;
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            colorAttachment.samples = m_msaaSamples;
            colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            VkAttachmentDescription depthAttachment{};
            depthAttachment.format = findDepthFormat();
            depthAttachment.samples = m_msaaSamples;
            depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            VkAttachmentDescription colorAttachmentResolve{};
            colorAttachmentResolve.format = m_swapChainImageFormat;
            colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
            colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

            VkAttachmentReference colorAttachmentRef{};
            colorAttachmentRef.attachment = 0;
            colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            VkAttachmentReference depthAttachmentRef{};
            depthAttachmentRef.attachment = 1;
            depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            VkAttachmentReference colorAttachmentResolveRef{};
            colorAttachmentResolveRef.attachment = 2;
            colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            VkSubpassDescription subpass{};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &colorAttachmentRef;
            subpass.pDepthStencilAttachment = &depthAttachmentRef;
            subpass.pResolveAttachments = &colorAttachmentResolveRef;

            VkSubpassDependency dependency{};
            dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            dependency.dstSubpass = 0;
            dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            dependency.srcAccessMask = 0;
            dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            std::array<VkAttachmentDescription, 3> attachments = {colorAttachment, depthAttachment, colorAttachmentResolve};
            VkRenderPassCreateInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            renderPassInfo.pAttachments = attachments.data();
            renderPassInfo.subpassCount = 1;
            renderPassInfo.pSubpasses = &subpass;
            renderPassInfo.dependencyCount = 1;
            renderPassInfo.pDependencies = &dependency;

            if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS)
                throw std::runtime_error("failed to create render pass!");
        }

        void createFramebuffers()
        {
            m_swapChainFramebuffers.resize(m_swapChainImageViews.len);

            for (size_t i = 0; i < m_swapChainImageViews.len; i++) {
                std::array<VkImageView, 3> attachments = {m_colorImageView, m_depthImageView, m_swapChainImageViews.ptr[i]};

                VkFramebufferCreateInfo framebufferInfo{};
                framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                framebufferInfo.renderPass = m_renderPass;
                framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
                framebufferInfo.pAttachments = attachments.data();
                framebufferInfo.width = m_swapChainExtent.width;
                framebufferInfo.height = m_swapChainExtent.height;
                framebufferInfo.layers = 1;

                if (vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_swapChainFramebuffers.ptr[i]) != VK_SUCCESS)
                    throw std::runtime_error("failed to create framebuffer!");
            }
        }

        void createcommandPool()
        {
            QueueFamilyIndices queueFamilyIndices = findQueueFamilies(m_physicalDevice);
            VkCommandPoolCreateInfo poolInfo{};
            poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily_value();
            if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS)
                throw std::runtime_error("failed to create command pool!");
        }

        void createcommandBuffers()
        {
            m_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

            VkCommandBufferAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocInfo.commandPool = m_commandPool;
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandBufferCount = m_commandBuffers.len;

            // auto destroyed
            if (vkAllocateCommandBuffers(m_device, &allocInfo, m_commandBuffers.ptr) != VK_SUCCESS)
                throw std::runtime_error("failed to allocate command buffers!");

        }

        void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
        {
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = 0; // optional
            beginInfo.pInheritanceInfo = nullptr; // optional

            if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
                throw std::runtime_error("failed to begin recording command buffer!");

            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = m_renderPass;
            renderPassInfo.framebuffer = m_swapChainFramebuffers.ptr[imageIndex];
            renderPassInfo.renderArea.offset = {0, 0};
            renderPassInfo.renderArea.extent = m_swapChainExtent;
            std::array<VkClearValue, 2> clearValues{};
            clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
            clearValues[1].depthStencil = {1.0f, 0};

            renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
            renderPassInfo.pClearValues = clearValues.data();

            vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            // bind the graphics pipeline
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(m_swapChainExtent.width);
            viewport.height = static_cast<float>(m_swapChainExtent.height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = m_swapChainExtent;
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            VkBuffer vertexBuffers[] = {m_vertexBuffer};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

            vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);

            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSets.ptr[m_currentFrame], 0, nullptr);

            vkCmdDrawIndexed(commandBuffer, m_indices.len, 1, 0, 0, 0);
            vkCmdEndRenderPass(commandBuffer);

            if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
                throw std::runtime_error("failed to record command buffer!");
        }

        void drawFrame()
        {
            vkWaitForFences(m_device, 1, &m_inFlightFences.ptr[m_currentFrame], VK_TRUE, UINT64_MAX);

            uint32_t imageIndex;
            // suboptimal or out of date swap chain
            VkResult result = vkAcquireNextImageKHR(m_device, m_swapChain, UINT64_MAX, m_imageAvailableSemaphores.ptr[m_currentFrame], VK_NULL_HANDLE, &imageIndex);
            if (result == VK_ERROR_OUT_OF_DATE_KHR) 
            {
                recreateSwapChain();
                return;
            } 
            else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
                throw std::runtime_error("failed to acquire swap chain image!");

            updateUniformBuffer(m_currentFrame);
            
            // only reset the fence if we are submitting work
            vkResetFences(m_device, 1, &m_inFlightFences.ptr[m_currentFrame]);

            vkResetCommandBuffer(m_commandBuffers.ptr[m_currentFrame],  0);
            recordCommandBuffer(m_commandBuffers.ptr[m_currentFrame], imageIndex);


            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

            VkSemaphore waitSemaphores[] = {m_imageAvailableSemaphores.ptr[m_currentFrame]};
            VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = waitSemaphores;
            submitInfo.pWaitDstStageMask = waitStages;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &m_commandBuffers.ptr[m_currentFrame];
            VkSemaphore signalSemaphores[] = {m_renderFinishedSemaphores.ptr[m_currentFrame]};
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = signalSemaphores;
            if (vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFences.ptr[m_currentFrame]) != VK_SUCCESS)
                throw std::runtime_error("failed to submit draw command buffer!");

            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = signalSemaphores;
            VkSwapchainKHR swapChains[] = {m_swapChain};
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = swapChains;
            presentInfo.pImageIndices = &imageIndex;
            presentInfo.pResults = nullptr;

            result = vkQueuePresentKHR(m_presentQueue, &presentInfo);

            if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_framebufferResized)
            {
                m_framebufferResized = false;
                recreateSwapChain();
            } 
            else if (result != VK_SUCCESS)
                throw std::runtime_error("failed to present swap chain image!");

            m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

        }

        void updateUniformBuffer(uint32_t currentImage)
        {
            static auto startTime = std::chrono::high_resolution_clock::now();
            auto currentTime = std::chrono::high_resolution_clock::now();
            float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

            float every2sec = 1 + int(time)%2;

            UniformBufferObject ubo{};
            ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
            ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
            ubo.proj = glm::perspective(glm::radians(45.0f), m_swapChainExtent.width / (float) m_swapChainExtent.height, 0.1f, 10.0f);
            ubo.proj[1][1] *= -1;

            ubo.texmod = glm::vec4(1);


            memcpy(m_uniformBuffersMapped.ptr[currentImage], &ubo, sizeof(ubo));

        }
        
        void createSyncObjects()
        {

            m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
            m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
            m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

            VkSemaphoreCreateInfo semaphoreInfo{};
            semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

            VkFenceCreateInfo fenceInfo{};
            fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                if (vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores.ptr[i]) != VK_SUCCESS ||
                    vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores.ptr[i]) != VK_SUCCESS ||
                    vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences.ptr[i]) != VK_SUCCESS)

                    throw std::runtime_error("failed to create synchronization objects for a frame!");
                
            }

        }

        void cleanupSwapChain()
        {
            vkDestroyImageView(m_device, m_depthImageView, nullptr);
            vkDestroyImage(m_device, m_depthImage, nullptr);
            vkFreeMemory(m_device, m_depthImageMemory, nullptr);

            vkDestroyImageView(m_device, m_colorImageView, nullptr);
            vkDestroyImage(m_device, m_colorImage, nullptr);
            vkFreeMemory(m_device, m_colorImageMemory, nullptr);

            for (size_t i = 0; i < m_swapChainFramebuffers.len; i++)
                vkDestroyFramebuffer(m_device, m_swapChainFramebuffers.ptr[i], nullptr);
            
            for (size_t i = 0; i < m_swapChainImageViews.len; i++)
                vkDestroyImageView(m_device, m_swapChainImageViews.ptr[i], nullptr);
        
            vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);
        }

        void recreateSwapChain() 
        {
            int width = 0, height = 0;
            glfwGetFramebufferSize(m_window, &width, &height);
            while (width == 0 || height == 0)
            {
                glfwGetFramebufferSize(m_window, &width, &height);
                glfwWaitEvents();
            }

            vkDeviceWaitIdle(m_device);

            cleanupSwapChain();

            createSwapChain();
            createImageViews();
            createColorResources();
            createDepthResources();
            createFramebuffers();
        }

        void createVertexBuffer()
        {
            VkDeviceSize bufferSize = sizeof(m_vertices.ptr[0]) * m_vertices.len;

            VkBuffer stagingBuffer;
            VkDeviceMemory stagingBufferMemory;
            createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                stagingBuffer, stagingBufferMemory);

            void* data;
            vkMapMemory(m_device, stagingBufferMemory, 0, bufferSize, 0, &data);
            memcpy(data, m_vertices.ptr, (size_t) bufferSize);
            vkUnmapMemory(m_device, stagingBufferMemory);

            createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_vertexBuffer, m_vertexBufferMemory);
            copyBuffer(stagingBuffer, m_vertexBuffer, bufferSize);
            
            vkDestroyBuffer(m_device, stagingBuffer, nullptr);
            vkFreeMemory(m_device, stagingBufferMemory, nullptr);

        }

        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
        {
            VkPhysicalDeviceMemoryProperties memProperties;
            vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);

            for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
                if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
                    return i;
            throw std::runtime_error("failed to find suitable memory type!");

        }

        void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
        {
            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = size;
            bufferInfo.usage = usage;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            if (vkCreateBuffer(m_device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
                throw std::runtime_error("failed to create buffer!");

            VkMemoryRequirements memRequirements;
            vkGetBufferMemoryRequirements(m_device, buffer, &memRequirements);

            VkMemoryAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = memRequirements.size;
            allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

            if (vkAllocateMemory(m_device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
                throw std::runtime_error("failed to allocate buffer memory!");

            vkBindBufferMemory(m_device, buffer, bufferMemory, 0);

            
        }

        void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
        {
            VkCommandBuffer commandBuffer = beginSingleTimeCommands();

            VkBufferCopy copyRegion{};
            copyRegion.size = size;
            vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

            endSingleTimeCommands(commandBuffer);
        }

        void createIndexBuffer()
        {
            VkDeviceSize bufferSize = sizeof(m_indices.ptr[0]) * m_indices.len;

            VkBuffer stagingBuffer;
            VkDeviceMemory stagingBufferMemory;
            createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

            void* data;
            vkMapMemory(m_device, stagingBufferMemory, 0, bufferSize, 0, &data);
            memcpy(data, m_indices.ptr, (size_t) bufferSize);
            vkUnmapMemory(m_device, stagingBufferMemory);

            createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_indexBuffer, m_indexBufferMemory);

            copyBuffer(stagingBuffer, m_indexBuffer, bufferSize);

            vkDestroyBuffer(m_device, stagingBuffer, nullptr);
            vkFreeMemory(m_device, stagingBufferMemory, nullptr);
        }

        void createDescriptorSetLayout()
        {
            VkDescriptorSetLayoutBinding uboLayoutBinding{};
            uboLayoutBinding.binding = 0;
            uboLayoutBinding.descriptorCount = 1;
            uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            uboLayoutBinding.pImmutableSamplers = nullptr;
            uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

            VkDescriptorSetLayoutBinding samplerLayoutBinding{};
            samplerLayoutBinding.binding = 1;
            samplerLayoutBinding.descriptorCount = 1;
            samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            samplerLayoutBinding.pImmutableSamplers = nullptr;
            samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

            std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};
            VkDescriptorSetLayoutCreateInfo layoutInfo{};
            layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
            layoutInfo.pBindings = bindings.data();

            if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
                throw std::runtime_error("failed to create descriptor set layout!");
            }
        }
   
        void createUniformBuffers()
        {
            VkDeviceSize bufferSize = sizeof(UniformBufferObject);

            m_uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
            m_uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
            m_uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                    m_uniformBuffers.ptr[i], m_uniformBuffersMemory.ptr[i]);

                vkMapMemory(m_device, m_uniformBuffersMemory.ptr[i], 0, bufferSize, 0, &m_uniformBuffersMapped.ptr[i]);
            }
        }

        void createDescriptorSets()
        {
            num_ptr_arr<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_descriptorSetLayout);
            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = m_descriptorPool;
            allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
            allocInfo.pSetLayouts = layouts.ptr;

            m_descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
            if (vkAllocateDescriptorSets(m_device, &allocInfo, m_descriptorSets.ptr) != VK_SUCCESS)
                throw std::runtime_error("failed to allocate descriptor sets!");

            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            {
                VkDescriptorBufferInfo bufferInfo{};
                bufferInfo.buffer = m_uniformBuffers.ptr[i];
                bufferInfo.offset = 0;
                bufferInfo.range = sizeof(UniformBufferObject);

                VkDescriptorImageInfo imageInfo{};
                imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                imageInfo.imageView = m_textureImageView;
                imageInfo.sampler = m_textureSampler;

                std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

                descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptorWrites[0].dstSet = m_descriptorSets.ptr[i];
                descriptorWrites[0].dstBinding = 0;
                descriptorWrites[0].dstArrayElement = 0;
                descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                descriptorWrites[0].descriptorCount = 1;
                descriptorWrites[0].pBufferInfo = &bufferInfo;

                descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptorWrites[1].dstSet = m_descriptorSets.ptr[i];
                descriptorWrites[1].dstBinding = 1;
                descriptorWrites[1].dstArrayElement = 0;
                descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                descriptorWrites[1].descriptorCount = 1;
                descriptorWrites[1].pImageInfo = &imageInfo;

                vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        
            }
        }

        void createDescriptorPool()
        {
            std::array<VkDescriptorPoolSize, 2> poolSizes{};
            poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
            poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

            VkDescriptorPoolCreateInfo poolInfo{};
            poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
            poolInfo.pPoolSizes = poolSizes.data();
            poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

            if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS)
                throw std::runtime_error("failed to create descriptor pool!");
        

        }

        void createTextureImage()
        {
            int texWidth, texHeight, texChannels;
            stbi_uc* pixels = stbi_load(TEXTURE_PATH, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
            VkDeviceSize imageSize = texWidth * texHeight * 4;

            m_mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

            if (!pixels)
                throw std::runtime_error("failed to load texture image!");

            VkBuffer stagingBuffer;
            VkDeviceMemory stagingBufferMemory;
            createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

            void* data;
            vkMapMemory(m_device, stagingBufferMemory, 0, imageSize, 0, &data);
            memcpy(data, pixels, static_cast<size_t>(imageSize));
            vkUnmapMemory(m_device, stagingBufferMemory);

            stbi_image_free(pixels);

            createImage(texWidth, texHeight, m_mipLevels, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, 
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_textureImage, m_textureImageMemory);

            transitionImageLayout(m_textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, m_mipLevels);
        
            copyBufferToImage(stagingBuffer, m_textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

            vkDestroyBuffer(m_device, stagingBuffer, nullptr);
            vkFreeMemory(m_device, stagingBufferMemory, nullptr);

            generateMipmaps(m_textureImage, VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, m_mipLevels);
        }

        void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
        {
            VkCommandBuffer commandBuffer = beginSingleTimeCommands();

            VkBufferImageCopy region{};
            region.bufferOffset = 0;
            region.bufferRowLength = 0;
            region.bufferImageHeight = 0;
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel = 0;
            region.imageSubresource.baseArrayLayer = 0;
            region.imageSubresource.layerCount = 1;
            region.imageOffset = {0, 0, 0};
            region.imageExtent = {width, height, 1};

            vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

            endSingleTimeCommands(commandBuffer);
        }

        void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory)
        {
            VkImageCreateInfo imageInfo{};
            imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageInfo.imageType = VK_IMAGE_TYPE_2D;
            imageInfo.extent.width = width;
            imageInfo.extent.height = height;
            imageInfo.extent.depth = 1;
            imageInfo.mipLevels = mipLevels;
            imageInfo.arrayLayers = 1;
            imageInfo.format = format;
            imageInfo.tiling = tiling;
            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageInfo.usage = usage;
            imageInfo.samples = numSamples;
            imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            if (vkCreateImage(m_device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
                throw std::runtime_error("failed to create image!");
            }

            VkMemoryRequirements memRequirements;
            vkGetImageMemoryRequirements(m_device, image, &memRequirements);

            VkMemoryAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = memRequirements.size;
            allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

            if (vkAllocateMemory(m_device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS)
                throw std::runtime_error("failed to allocate image memory!");
            

            vkBindImageMemory(m_device, image, imageMemory, 0);
        }

        VkCommandBuffer beginSingleTimeCommands()
        {
            VkCommandBufferAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandPool = m_commandPool;
            allocInfo.commandBufferCount = 1;

            VkCommandBuffer commandBuffer;
            vkAllocateCommandBuffers(m_device, &allocInfo, &commandBuffer);

            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

            vkBeginCommandBuffer(commandBuffer, &beginInfo);

            return commandBuffer;
        }

        void endSingleTimeCommands(VkCommandBuffer commandBuffer) {
            vkEndCommandBuffer(commandBuffer);

            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffer;

            vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
            vkQueueWaitIdle(m_graphicsQueue);

            vkFreeCommandBuffers(m_device, m_commandPool, 1, &commandBuffer);
        }

        void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels)
        {
            VkCommandBuffer commandBuffer = beginSingleTimeCommands();

            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = oldLayout;
            barrier.newLayout = newLayout;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = image;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = mipLevels;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;

            VkPipelineStageFlags sourceStage;
            VkPipelineStageFlags destinationStage;

            if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
            {
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

                sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            }
            else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            {
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

                sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            }
            else throw std::invalid_argument("unsupported layout transition!");

            vkCmdPipelineBarrier(
                commandBuffer,
                sourceStage, destinationStage,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier
            );

            endSingleTimeCommands(commandBuffer);
        }

        VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels)
        {
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = image;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = format;
            viewInfo.subresourceRange.aspectMask = aspectFlags;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = mipLevels;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;

            VkImageView imageView;
            if (vkCreateImageView(m_device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
                throw std::runtime_error("failed to create texture image view!");
            }

            return imageView;
        }

        void createTextureImageView()
        {
            m_textureImageView = createImageView(m_textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, m_mipLevels);

        }

        void createTextureSampler()
        {
            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(m_physicalDevice, &properties);

            VkSamplerCreateInfo samplerInfo{};
            samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerInfo.magFilter = VK_FILTER_LINEAR;
            samplerInfo.minFilter = VK_FILTER_LINEAR;
            samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerInfo.anisotropyEnable = VK_TRUE;
            samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
            samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
            samplerInfo.unnormalizedCoordinates = VK_FALSE;
            samplerInfo.compareEnable = VK_FALSE;
            samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
            samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            samplerInfo.minLod = 0.0f; // try static_cast<float>(m_mipLevels / 2) to see far away version
            samplerInfo.maxLod = static_cast<float>(m_mipLevels);
            samplerInfo.mipLodBias = 0.0f; // optional

            if (vkCreateSampler(m_device, &samplerInfo, nullptr, &m_textureSampler) != VK_SUCCESS)
                throw std::runtime_error("failed to create texture sampler!");
        }

        VkFormat findSupportedFormat(const num_ptr_arr<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
        {
            for (size_t i = 0; i < candidates.len; i++)
            {
                VkFormat format = candidates.ptr[i];
                VkFormatProperties props;
                vkGetPhysicalDeviceFormatProperties(m_physicalDevice, format, &props);

                if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
                    return format;
                else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
                    return format;
            }

            throw std::runtime_error("failed to find supported format!");
        }

        VkFormat findDepthFormat()
        {
            return findSupportedFormat(
            {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
            );
        }

        bool hasStencilComponent(VkFormat format)
        {
            return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
        }

        void loadModel()
        {
            tinyobj::attrib_t attrib;
            std::vector<tinyobj::shape_t> shapes;
            std::vector<tinyobj::material_t> materials;
            std::string warn, err;

            if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, MODEL_PATH))
                throw std::runtime_error(warn + err);

            size_t shapes_size = (size_t)shapes.size();
            size_t mesh_size = (size_t)shapes[0].mesh.indices.size();
            size_t idx_v = 0;
            size_t idx_i = 0;

            m_vertices.resize(shapes_size * mesh_size);
            m_indices.resize(shapes_size * mesh_size);

            std::unordered_map<Vertex, uint32_t> uniqueVertices{};
            
            for (const auto& shape : shapes) {
                

                for (const auto& index : shape.mesh.indices) 
                {
                    Vertex vertex{};

                    vertex.pos = {
                        attrib.vertices[3 * index.vertex_index + 0],
                        attrib.vertices[3 * index.vertex_index + 1],
                        attrib.vertices[3 * index.vertex_index + 2]
                    };

                    vertex.texCoord = {
                        attrib.texcoords[2 * index.texcoord_index + 0],
                        1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                    };

                    vertex.color = {1.0f, 1.0f, 1.0f};
                    
                    if (uniqueVertices.count(vertex) == 0)
                    {
                        uniqueVertices[vertex] = idx_v;
                        m_vertices.ptr[idx_v++] = vertex;
                    }
                    m_indices.ptr[idx_i++] = uniqueVertices[vertex];
                }
            }
        }

        void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels)
        {
            // Check if image format supports linear blitting
            VkFormatProperties formatProperties;
            vkGetPhysicalDeviceFormatProperties(m_physicalDevice, imageFormat, &formatProperties);

            if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
                throw std::runtime_error("texture image format does not support linear blitting!");

            VkCommandBuffer commandBuffer = beginSingleTimeCommands();

            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.image = image;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;
            barrier.subresourceRange.levelCount = 1;

            int32_t mipWidth = texWidth;
            int32_t mipHeight = texHeight;

            for (uint32_t i = 1; i < mipLevels; i++)
            {
                barrier.subresourceRange.baseMipLevel = i - 1;
                barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

                vkCmdPipelineBarrier(commandBuffer,
                    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                    0, nullptr,
                    0, nullptr,
                    1, &barrier);

                VkImageBlit blit{};
                blit.srcOffsets[0] = {0, 0, 0};
                blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
                blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                blit.srcSubresource.mipLevel = i - 1;
                blit.srcSubresource.baseArrayLayer = 0;
                blit.srcSubresource.layerCount = 1;
                blit.dstOffsets[0] = {0, 0, 0};
                blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
                blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                blit.dstSubresource.mipLevel = i;
                blit.dstSubresource.baseArrayLayer = 0;
                blit.dstSubresource.layerCount = 1;

                vkCmdBlitImage(commandBuffer,
                    image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1, &blit,
                    VK_FILTER_LINEAR);

                barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

                vkCmdPipelineBarrier(commandBuffer,
                    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                    0, nullptr,
                    0, nullptr,
                    1, &barrier);

                if (mipWidth > 1) mipWidth /= 2;
                if (mipHeight > 1) mipHeight /= 2;
            }

            barrier.subresourceRange.baseMipLevel = mipLevels - 1;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                0, nullptr,
                0, nullptr,
                1, &barrier);

            endSingleTimeCommands(commandBuffer);
        }

        VkSampleCountFlagBits getMaxUsableSampleCount()
        {
            VkPhysicalDeviceProperties physicalDeviceProperties;
            vkGetPhysicalDeviceProperties(m_physicalDevice, &physicalDeviceProperties);

            VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
            if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
            if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
            if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
            if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
            if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
            if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

            return VK_SAMPLE_COUNT_1_BIT;
        }

        void createColorResources()
        {
            VkFormat colorFormat = m_swapChainImageFormat;
            createImage(m_swapChainExtent.width, m_swapChainExtent.height, 1, m_msaaSamples, colorFormat, 
                VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_colorImage, m_colorImageMemory);
            m_colorImageView = createImageView(m_colorImage, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
        }

        void createDepthResources()
        {
            VkFormat depthFormat = findDepthFormat();

            createImage(m_swapChainExtent.width, m_swapChainExtent.height, 1, m_msaaSamples, depthFormat, VK_IMAGE_TILING_OPTIMAL, 
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_depthImage, m_depthImageMemory);
            m_depthImageView = createImageView(m_depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);
        }

        void initVulkan()
        {
            createInstance();
            createSurface();
            pickPhysicalDevice();
            createLogicalDevice();
            createSwapChain();
            createImageViews();
            createRenderPass();
            createDescriptorSetLayout();
            createGraphicsPipeline();
            createcommandPool();
            createColorResources();
            createDepthResources();
            createFramebuffers();
            createTextureImage();
            createTextureImageView();
            createTextureSampler();
            loadModel();
            createVertexBuffer();
            createIndexBuffer();
            createUniformBuffers();
            createDescriptorPool();
            createDescriptorSets();
            createcommandBuffers();
            createSyncObjects();
        }
    
    private:
        VkInstance m_instance;
        GLFWwindow* m_window;
        VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
        VkDevice m_device;
        VkQueue m_graphicsQueue;
        VkSurfaceKHR m_surface;
        VkQueue m_presentQueue;
        VkSwapchainKHR m_swapChain;

        num_ptr_arr<VkImage> m_swapChainImages;
        VkFormat m_swapChainImageFormat;
        VkExtent2D m_swapChainExtent;

        num_ptr_arr<VkImageView> m_swapChainImageViews;

        VkRenderPass m_renderPass;
        VkDescriptorSetLayout m_descriptorSetLayout;
        VkPipelineLayout m_pipelineLayout;
        VkPipeline m_graphicsPipeline;

        num_ptr_arr<VkFramebuffer> m_swapChainFramebuffers;
        VkCommandPool m_commandPool;
        num_ptr_arr<VkCommandBuffer> m_commandBuffers;

        num_ptr_arr<VkSemaphore> m_imageAvailableSemaphores;
        num_ptr_arr<VkSemaphore> m_renderFinishedSemaphores;
        num_ptr_arr<VkFence> m_inFlightFences;

        uint32_t m_currentFrame = 0;

        bool m_framebufferResized = false;

        VkBuffer m_vertexBuffer;
        VkDeviceMemory m_vertexBufferMemory;

        VkBuffer m_indexBuffer;
        VkDeviceMemory m_indexBufferMemory;

        num_ptr_arr<VkBuffer> m_uniformBuffers;
        num_ptr_arr<VkDeviceMemory> m_uniformBuffersMemory;
        num_ptr_arr<void*> m_uniformBuffersMapped;

        VkDescriptorPool m_descriptorPool;
        num_ptr_arr<VkDescriptorSet> m_descriptorSets;

        VkImage m_textureImage;
        VkDeviceMemory m_textureImageMemory;
        VkImageView m_textureImageView;

        VkSampler m_textureSampler;

        VkImage m_depthImage;
        VkDeviceMemory m_depthImageMemory;
        VkImageView m_depthImageView;

        num_ptr_arr<Vertex> m_vertices;
        num_ptr_arr<uint32_t> m_indices;

        uint32_t m_mipLevels;

        VkSampleCountFlagBits m_msaaSamples = VK_SAMPLE_COUNT_1_BIT;

        VkImage m_colorImage;
        VkDeviceMemory m_colorImageMemory;
        VkImageView m_colorImageView;
};


int main()
{
    HelloWorld app;
    try 
    {
        app.run();
    } catch (const std::exception& e) 
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
