#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <stdexcept>
#include <fstream>
#include <cstring>

class VulkanContext {
public:
    VulkanContext();
    ~VulkanContext();

    struct Buffer { VkBuffer buf{}; VkDeviceMemory mem{}; VkDeviceSize size{}; };
    struct Pipeline { VkPipelineLayout layout{}; VkPipeline pipe{}; };

    Buffer createDeviceLocalBuffer(VkDeviceSize size, VkBufferUsageFlags usage);
    void destroy(Buffer b);

    void uploadWithStaging(const void* src, VkDeviceSize bytes, const Buffer& dst);
    void downloadWithStaging(const Buffer& src, void* dst, VkDeviceSize bytes);

    // descriptors
    VkDescriptorSetLayout createDescriptorSetLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings);
    VkDescriptorPool createDescriptorPool(const std::vector<VkDescriptorPoolSize>& sizes, uint32_t maxSets);
    VkDescriptorSet allocDescriptorSet(VkDescriptorPool pool, VkDescriptorSetLayout layout);

    // pipeline
    Pipeline createComputePipeline(const char* spv_path,
                                   const std::vector<VkPushConstantRange>& pushRanges,
                                   const std::vector<VkDescriptorSetLayout>& setLayouts);

    // command + dispatch helpers
    VkCommandBuffer beginOneShot();
    void endOneShot(VkCommandBuffer cmd);

    // generic dispatch (bind, push, dispatch, submit wait)
    void dispatch(const Pipeline& p,
                  VkDescriptorSet set,
                  uint32_t gx, uint32_t gy, uint32_t gz,
                  const void* pushData, uint32_t pushBytes);

    void program(VkDeviceSize sizeA, VkDeviceSize sizeB, VkDeviceSize sizeC, 
    const float* hostA, const float* hostB, float* hostC, const char* spv_path, void* pc, uint32_t pc_size, 
    uint32_t gx, uint32_t gy, uint32_t gz);

    static uint32_t ceilDiv(uint32_t a, uint32_t b) { return (a + b - 1) / b; }


private:
    // Init bits
    void createInstance();
    void pickPhysicalDevice();
    void createDeviceAndQueue();
    void createCommandPool();

    uint32_t findMemoryType(uint32_t bits, VkMemoryPropertyFlags props);
    static std::vector<char> readFile(const char* path) {
        std::ifstream f(path, std::ios::binary | std::ios::ate);
        if (!f) throw std::runtime_error("cannot open spv");
        size_t sz = (size_t)f.tellg(); std::vector<char> buf(sz);
        f.seekg(0); f.read(buf.data(), sz); return buf;
    }

private:
    VkInstance m_instance{};
    VkPhysicalDevice m_phys{};
    VkDevice m_device{};
    VkQueue m_queue{};
    uint32_t m_queueFamily{};
    VkCommandPool m_cmdPool{};
};