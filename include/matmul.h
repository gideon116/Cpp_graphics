#include <vulkan/vulkan.h>
#include <iostream>
#include <vector>
#include <memory>
#include <stdexcept>
#include <fstream>
#include <cstring>
#include <cstdint>

constexpr bool verbose = true;

// ----------------------------------------------
// Helpers
// ----------------------------------------------
static std::vector<char> readFile(const char* filename) {
    std::ifstream f(filename, std::ios::ate | std::ios::binary);
    if (!f.is_open()) throw std::runtime_error("failed to open file");
    size_t sz = (size_t)f.tellg();
    std::vector<char> buf(sz);
    f.seekg(0);
    f.read(buf.data(), sz);
    return buf;
}
static uint32_t ceilDiv(uint32_t a, uint32_t b) { return (a + b - 1) / b; }

// ----------------------------------------------
// MakeVulkan (compute-only)
// ----------------------------------------------
class MakeVulkan {
public:
    MakeVulkan(float* a, float* b, size_t a_size, size_t b_size, size_t c_size)
        : m_matA(a), m_matB(b), m_A_size(a_size), m_B_size(b_size), m_C_size(c_size) {}

    // Matrix sizes (row-major):
    // A: MxN, B: NxP, C: MxP
    uint32_t mM = 4, mN = 4, mP = 4;

    // Workgroup size: must match the shader's local_size_x/y
    uint32_t Lx = 16, Ly = 16;

    // Output
    std::unique_ptr<float[]> m_result;

    // API
    void run();

    // Inputs (host)
    float* m_matA;
    float* m_matB;
    size_t m_A_size, m_B_size, m_C_size;

private:
    // Vulkan bootstrapping
    void initVulkan();
    void cleanup();
    void createInstance();
    void pickPhysicalDevice();
    void createDevice();
    void createDescriptorSetLayout();
    void createPipeline();
    void createCommandPool();
    void createCommandBuffer();

    // Work submission
    void runCommandBuffer();

    // Resources
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags props, VkBuffer& buffer, VkDeviceMemory& memory);
    uint32_t findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags props);

    // Device-local + staging helpers
    void createDeviceLocalBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                 VkBuffer& buf, VkDeviceMemory& mem);
    VkCommandBuffer beginOneShot();
    void endOneShot(VkCommandBuffer cmd);
    void uploadWithStaging(const void* src, VkDeviceSize bytes, VkBuffer dstDeviceLocal);
    void downloadWithStaging(VkBuffer srcDeviceLocal, VkDeviceSize bytes, void* dst);

private:
    VkInstance m_instance{};
    VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
    VkDevice m_device{};
    VkQueue m_queue{};
    uint32_t m_queueFamilyIndex{0};

    VkDescriptorSetLayout m_descriptorSetLayout{};
    VkPipelineLayout m_pipelineLayout{};
    VkPipeline m_pipeline{};

    VkCommandPool m_commandPool{};
    VkCommandBuffer m_commandBuffer{};
    VkFence m_fence{};

    // Device-local buffers
    VkBuffer m_bufferA{};
    VkDeviceMemory m_memoryA{};
    VkBuffer m_bufferB{};
    VkDeviceMemory m_memoryB{};
    VkBuffer m_bufferC{};
    VkDeviceMemory m_memoryC{};
};

// ----------------------------------------------
// Implementation
// ----------------------------------------------
void MakeVulkan::createInstance() {
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "cpp_ml";
    app.applicationVersion = VK_MAKE_VERSION(1,0,0);
    app.pEngineName = "NoEngine";
    app.engineVersion = VK_MAKE_VERSION(1,0,0);
    app.apiVersion = VK_API_VERSION_1_1;

    // MoltenVK / macOS: portability enumeration
    const char* instExts[] = {
        VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
        "VK_KHR_portability_enumeration",
        "VK_KHR_get_physical_device_properties2",
    };

    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ci.pApplicationInfo = &app;
    ci.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    ci.enabledExtensionCount = (uint32_t)(sizeof(instExts)/sizeof(instExts[0]));
    ci.ppEnabledExtensionNames = instExts;

    if (vkCreateInstance(&ci, nullptr, &m_instance) != VK_SUCCESS)
        throw std::runtime_error("vkCreateInstance failed");
}

void MakeVulkan::pickPhysicalDevice() {
    uint32_t n = 0;
    vkEnumeratePhysicalDevices(m_instance, &n, nullptr);
    if (!n) throw std::runtime_error("no Vulkan devices");
    std::vector<VkPhysicalDevice> devs(n);
    vkEnumeratePhysicalDevices(m_instance, &n, devs.data());

    for (auto d : devs) {
        uint32_t qCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(d, &qCount, nullptr);
        std::vector<VkQueueFamilyProperties> qprops(qCount);
        vkGetPhysicalDeviceQueueFamilyProperties(d, &qCount, qprops.data());
        for (uint32_t i = 0; i < qCount; ++i) {
            if (qprops[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                m_physicalDevice = d;
                m_queueFamilyIndex = i;
                if (verbose) {
                    VkPhysicalDeviceProperties props{};
                    vkGetPhysicalDeviceProperties(d, &props);
                    std::cout << "[INFO] Using GPU: " << props.deviceName
                              << " (compute queue family " << i << ")\n";
                }
                return;
            }
        }
    }
    throw std::runtime_error("no compute-capable queue found");
}

void MakeVulkan::createDevice() {
    float prio = 1.0f;
    VkDeviceQueueCreateInfo q{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    q.queueFamilyIndex = m_queueFamilyIndex;
    q.queueCount = 1;
    q.pQueuePriorities = &prio;

    // On macOS, many drivers expose VK_KHR_portability_subset as a *device* extension.
    // Enable it if available (won’t hurt elsewhere).
    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &extCount, nullptr);
    std::vector<VkExtensionProperties> exts(extCount);
    vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &extCount, exts.data());
    std::vector<const char*> devExts;
    for (auto& e : exts) {
        if (std::strcmp(e.extensionName, "VK_KHR_portability_subset") == 0) {
            devExts.push_back("VK_KHR_portability_subset");
        }
    }

    VkDeviceCreateInfo ci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    ci.queueCreateInfoCount = 1;
    ci.pQueueCreateInfos = &q;
    ci.enabledExtensionCount = (uint32_t)devExts.size();
    ci.ppEnabledExtensionNames = devExts.empty() ? nullptr : devExts.data();

    if (vkCreateDevice(m_physicalDevice, &ci, nullptr, &m_device) != VK_SUCCESS)
        throw std::runtime_error("vkCreateDevice failed");

    vkGetDeviceQueue(m_device, m_queueFamilyIndex, 0, &m_queue);
}

uint32_t MakeVulkan::findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties mp{};
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
        if ((typeBits & (1u << i)) &&
            (mp.memoryTypes[i].propertyFlags & props) == props)
            return i;
    }
    throw std::runtime_error("no suitable memory type");
}

void MakeVulkan::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                              VkMemoryPropertyFlags props,
                              VkBuffer& buffer, VkDeviceMemory& memory) {
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = size;
    bi.usage = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(m_device, &bi, nullptr, &buffer) != VK_SUCCESS)
        throw std::runtime_error("vkCreateBuffer failed");

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(m_device, buffer, &req);

    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = findMemoryType(req.memoryTypeBits, props);

    if (vkAllocateMemory(m_device, &ai, nullptr, &memory) != VK_SUCCESS)
        throw std::runtime_error("vkAllocateMemory failed");

    vkBindBufferMemory(m_device, buffer, memory, 0);
}

void MakeVulkan::createDeviceLocalBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                         VkBuffer& buf, VkDeviceMemory& mem) {
    createBuffer(size,
        usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, buf, mem);
}

VkCommandBuffer MakeVulkan::beginOneShot() {
    VkCommandBufferAllocateInfo a{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    a.commandPool = m_commandPool;
    a.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    a.commandBufferCount = 1;
    VkCommandBuffer cmd{};
    if (vkAllocateCommandBuffers(m_device, &a, &cmd) != VK_SUCCESS)
        throw std::runtime_error("vkAllocateCommandBuffers failed");

    VkCommandBufferBeginInfo b{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    b.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(cmd, &b) != VK_SUCCESS)
        throw std::runtime_error("vkBeginCommandBuffer failed");
    return cmd;
}

void MakeVulkan::endOneShot(VkCommandBuffer cmd) {
    vkEndCommandBuffer(cmd);
    VkSubmitInfo s{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    s.commandBufferCount = 1;
    s.pCommandBuffers = &cmd;
    vkQueueSubmit(m_queue, 1, &s, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_queue);
    vkFreeCommandBuffers(m_device, m_commandPool, 1, &cmd);
}

void MakeVulkan::uploadWithStaging(const void* src, VkDeviceSize bytes, VkBuffer dstDeviceLocal) {
    VkBuffer staging{};
    VkDeviceMemory stagingMem{};
    createBuffer(bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 staging, stagingMem);

    void* map = nullptr;
    vkMapMemory(m_device, stagingMem, 0, bytes, 0, &map);
    std::memcpy(map, src, (size_t)bytes);
    vkUnmapMemory(m_device, stagingMem);

    VkCommandBuffer cmd = beginOneShot();
    VkBufferCopy copy{0, 0, bytes};
    vkCmdCopyBuffer(cmd, staging, dstDeviceLocal, 1, &copy);
    endOneShot(cmd);

    vkDestroyBuffer(m_device, staging, nullptr);
    vkFreeMemory(m_device, stagingMem, nullptr);
}

void MakeVulkan::downloadWithStaging(VkBuffer srcDeviceLocal, VkDeviceSize bytes, void* dst) {
    VkBuffer staging{};
    VkDeviceMemory stagingMem{};
    createBuffer(bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 staging, stagingMem);

    VkCommandBuffer cmd = beginOneShot();
    VkBufferCopy copy{0, 0, bytes};
    vkCmdCopyBuffer(cmd, srcDeviceLocal, staging, 1, &copy);
    endOneShot(cmd);

    void* map = nullptr;
    vkMapMemory(m_device, stagingMem, 0, bytes, 0, &map);
    std::memcpy(dst, map, (size_t)bytes);
    vkUnmapMemory(m_device, stagingMem);

    vkDestroyBuffer(m_device, staging, nullptr);
    vkFreeMemory(m_device, stagingMem, nullptr);
}

void MakeVulkan::createDescriptorSetLayout() {
    // binding 0: A, 1: B, 2: C (all storage buffers)
    VkDescriptorSetLayoutBinding b[3]{};
    for (uint32_t i = 0; i < 3; ++i) {
        b[i].binding = i;
        b[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        b[i].descriptorCount = 1;
        b[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    ci.bindingCount = 3;
    ci.pBindings = b;
    if (vkCreateDescriptorSetLayout(m_device, &ci, nullptr, &m_descriptorSetLayout) != VK_SUCCESS)
        throw std::runtime_error("vkCreateDescriptorSetLayout failed");
}

VkShaderModule makeModule(VkDevice dev, const char* path) {
    auto code = readFile(path);
    VkShaderModuleCreateInfo mi{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    mi.codeSize = code.size();
    mi.pCode = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule mod{};
    if (vkCreateShaderModule(dev, &mi, nullptr, &mod) != VK_SUCCESS)
        throw std::runtime_error("vkCreateShaderModule failed");
    return mod;
}

void MakeVulkan::createPipeline() {
    // expects comp.spv with local_size_x=Lx, local_size_y=Ly and push constants {uint M,N,P}
    VkShaderModule cs = makeModule(m_device, "../resources/shaders/comp.spv");

    VkPipelineShaderStageCreateInfo stage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = cs;
    stage.pName = "main";

    VkPushConstantRange pcr{};
    pcr.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pcr.offset = 0;
    pcr.size = 3 * sizeof(uint32_t);

    VkPipelineLayoutCreateInfo pli{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pli.setLayoutCount = 1;
    pli.pSetLayouts = &m_descriptorSetLayout;
    pli.pushConstantRangeCount = 1;
    pli.pPushConstantRanges = &pcr;

    if (vkCreatePipelineLayout(m_device, &pli, nullptr, &m_pipelineLayout) != VK_SUCCESS)
        throw std::runtime_error("vkCreatePipelineLayout failed");

    VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    ci.stage = stage;
    ci.layout = m_pipelineLayout;

    if (vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &ci, nullptr, &m_pipeline) != VK_SUCCESS)
        throw std::runtime_error("vkCreateComputePipelines failed");

    vkDestroyShaderModule(m_device, cs, nullptr);
}

void MakeVulkan::createCommandPool() {
    VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pi.queueFamilyIndex = m_queueFamilyIndex;
    pi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(m_device, &pi, nullptr, &m_commandPool) != VK_SUCCESS)
        throw std::runtime_error("vkCreateCommandPool failed");
}

void MakeVulkan::createCommandBuffer() {
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = m_commandPool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(m_device, &ai, &m_commandBuffer) != VK_SUCCESS)
        throw std::runtime_error("vkAllocateCommandBuffers failed");
}

void MakeVulkan::runCommandBuffer() {
    // Descriptor pool for this one dispatch
    VkDescriptorPoolSize ps{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3};
    VkDescriptorPoolCreateInfo dpci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    dpci.poolSizeCount = 1;
    dpci.pPoolSizes = &ps;
    dpci.maxSets = 1;
    VkDescriptorPool dpool{};
    if (vkCreateDescriptorPool(m_device, &dpci, nullptr, &dpool) != VK_SUCCESS)
        throw std::runtime_error("vkCreateDescriptorPool failed");

    VkDescriptorSetAllocateInfo dsai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dsai.descriptorPool = dpool;
    dsai.descriptorSetCount = 1;
    dsai.pSetLayouts = &m_descriptorSetLayout;
    VkDescriptorSet dset{};
    if (vkAllocateDescriptorSets(m_device, &dsai, &dset) != VK_SUCCESS)
        throw std::runtime_error("vkAllocateDescriptorSets failed");

    VkDescriptorBufferInfo infos[3]{};
    infos[0].buffer = m_bufferA; infos[0].range = VK_WHOLE_SIZE;
    infos[1].buffer = m_bufferB; infos[1].range = VK_WHOLE_SIZE;
    infos[2].buffer = m_bufferC; infos[2].range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet writes[3]{};
    for (uint32_t i = 0; i < 3; ++i) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = dset;
        writes[i].dstBinding = i;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].descriptorCount = 1;
        writes[i].pBufferInfo = &infos[i];
    }
    vkUpdateDescriptorSets(m_device, 3, writes, 0, nullptr);

    // Record
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    if (vkBeginCommandBuffer(m_commandBuffer, &bi) != VK_SUCCESS)
        throw std::runtime_error("vkBeginCommandBuffer failed");

    vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
    vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            m_pipelineLayout, 0, 1, &dset, 0, nullptr);

    struct { uint32_t M,N,P; } push{ mM, mN, mP };
    vkCmdPushConstants(m_commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                       0, sizeof(push), &push);

    const uint32_t groupsX = ceilDiv(mP, Lx);
    const uint32_t groupsY = ceilDiv(mM, Ly);
    vkCmdDispatch(m_commandBuffer, groupsX, groupsY, 1);

    if (vkEndCommandBuffer(m_commandBuffer) != VK_SUCCESS)
        throw std::runtime_error("vkEndCommandBuffer failed");

    // Submit + wait
    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    if (vkCreateFence(m_device, &fci, nullptr, &m_fence) != VK_SUCCESS)
        throw std::runtime_error("vkCreateFence failed");

    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1;
    si.pCommandBuffers = &m_commandBuffer;
    if (vkQueueSubmit(m_queue, 1, &si, m_fence) != VK_SUCCESS)
        throw std::runtime_error("vkQueueSubmit failed");
    vkWaitForFences(m_device, 1, &m_fence, VK_TRUE, UINT64_MAX);

    vkDestroyFence(m_device, m_fence, nullptr);
    m_fence = VK_NULL_HANDLE;

    vkDestroyDescriptorPool(m_device, dpool, nullptr);
}

void MakeVulkan::initVulkan() {
    createInstance();
    pickPhysicalDevice();
    createDevice();
    createDescriptorSetLayout();
    createPipeline();
    createCommandPool();
    createCommandBuffer();

    // Device-local buffers for A, B, C
    createDeviceLocalBuffer(m_A_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, m_bufferA, m_memoryA);
    createDeviceLocalBuffer(m_B_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, m_bufferB, m_memoryB);
    createDeviceLocalBuffer(m_C_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, m_bufferC, m_memoryC);

    // Upload A and B via staging
    uploadWithStaging(m_matA, m_A_size, m_bufferA);
    uploadWithStaging(m_matB, m_B_size, m_bufferB);
}

void MakeVulkan::cleanup() {
    vkDestroyBuffer(m_device, m_bufferC, nullptr);
    vkFreeMemory(m_device, m_memoryC, nullptr);
    vkDestroyBuffer(m_device, m_bufferB, nullptr);
    vkFreeMemory(m_device, m_memoryB, nullptr);
    vkDestroyBuffer(m_device, m_bufferA, nullptr);
    vkFreeMemory(m_device, m_memoryA, nullptr);

    if (m_commandPool) vkDestroyCommandPool(m_device, m_commandPool, nullptr);
    if (m_pipeline) vkDestroyPipeline(m_device, m_pipeline, nullptr);
    if (m_pipelineLayout) vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
    if (m_descriptorSetLayout) vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
    if (m_device) vkDestroyDevice(m_device, nullptr);
    if (m_instance) vkDestroyInstance(m_instance, nullptr);

    m_commandPool = VK_NULL_HANDLE;
    m_pipeline = VK_NULL_HANDLE;
    m_pipelineLayout = VK_NULL_HANDLE;
    m_descriptorSetLayout = VK_NULL_HANDLE;
    m_device = VK_NULL_HANDLE;
    m_instance = VK_NULL_HANDLE;
}

void MakeVulkan::run() {
    initVulkan();
    runCommandBuffer();

    // Read C back
    const size_t elems = m_C_size / sizeof(float);
    m_result = std::make_unique<float[]>(elems);
    downloadWithStaging(m_bufferC, m_C_size, m_result.get());

    cleanup();
}