#include "VulkanContext.h"
#include <iostream>

VulkanContext::VulkanContext() {
    createInstance();
    pickPhysicalDevice();
    createDeviceAndQueue();
    createCommandPool();
}
VulkanContext::~VulkanContext() {
    vkDeviceWaitIdle(m_device);
    vkDestroyCommandPool(m_device, m_cmdPool, nullptr);
    vkDestroyDevice(m_device, nullptr);
    vkDestroyInstance(m_instance, nullptr);
}

void VulkanContext::createInstance() {
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "ComputeOnly";
    app.apiVersion = VK_API_VERSION_1_1;

    const char* exts[] = {
        VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
        "VK_KHR_portability_enumeration",
        "VK_KHR_get_physical_device_properties2"
    };

    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ci.pApplicationInfo = &app;
    ci.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    ci.enabledExtensionCount = 3;
    ci.ppEnabledExtensionNames = exts;

    if (vkCreateInstance(&ci, nullptr, &m_instance) != VK_SUCCESS)
        throw std::runtime_error("vkCreateInstance failed");
}

void VulkanContext::pickPhysicalDevice() {
    uint32_t n=0; vkEnumeratePhysicalDevices(m_instance, &n, nullptr);
    if (!n) throw std::runtime_error("no GPUs");
    std::vector<VkPhysicalDevice> devs(n);
    vkEnumeratePhysicalDevices(m_instance, &n, devs.data());

    for (auto d : devs) {
        uint32_t qCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(d, &qCount, nullptr);
        std::vector<VkQueueFamilyProperties> qprops(qCount);
        vkGetPhysicalDeviceQueueFamilyProperties(d, &qCount, qprops.data());
        for (uint32_t i = 0; i < qCount; ++i) {
            if (qprops[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                m_phys = d;
   
                VkPhysicalDeviceProperties props{};
                vkGetPhysicalDeviceProperties(d, &props);
                std::cout << "[INFO] Using GPU: " << props.deviceName
                            << " (compute queue family " << i << ")\n";
                std::cout << "maxComputeWorkGroupCount: ["
                        << props.limits.maxComputeWorkGroupCount[0] << ", "
                        << props.limits.maxComputeWorkGroupCount[1] << ", "
                        << props.limits.maxComputeWorkGroupCount[2] << "]\n";
                std::cout << "maxComputeWorkGroupSize: ["
                        << props.limits.maxComputeWorkGroupSize[0] << ", "
                        << props.limits.maxComputeWorkGroupSize[1] << ", "
                        << props.limits.maxComputeWorkGroupSize[2] << "]\n";
                std::cout << "maxComputeWorkGroupInvocations: "
                        << props.limits.maxComputeWorkGroupInvocations << "\n";
                
                return;
            }
        }
    }
    throw std::runtime_error("no compute-capable queue found");
}

void VulkanContext::createDeviceAndQueue() {
    uint32_t n=0; vkGetPhysicalDeviceQueueFamilyProperties(m_phys, &n, nullptr);
    std::vector<VkQueueFamilyProperties> qfp(n);
    vkGetPhysicalDeviceQueueFamilyProperties(m_phys, &n, qfp.data());
    for (uint32_t i=0;i<n;i++) if (qfp[i].queueFlags & VK_QUEUE_COMPUTE_BIT) { m_queueFamily = i; break; }

    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = m_queueFamily; qci.queueCount = 1; qci.pQueuePriorities = &prio;

    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = 1; dci.pQueueCreateInfos = &qci;

    if (vkCreateDevice(m_phys, &dci, nullptr, &m_device) != VK_SUCCESS)
        throw std::runtime_error("vkCreateDevice failed");
    vkGetDeviceQueue(m_device, m_queueFamily, 0, &m_queue);
}

void VulkanContext::createCommandPool() {
    VkCommandPoolCreateInfo ci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    ci.queueFamilyIndex = m_queueFamily;
    if (vkCreateCommandPool(m_device, &ci, nullptr, &m_cmdPool) != VK_SUCCESS)
        throw std::runtime_error("cmd pool fail");
}

uint32_t VulkanContext::findMemoryType(uint32_t bits, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties mp; vkGetPhysicalDeviceMemoryProperties(m_phys, &mp);
    for (uint32_t i=0;i<mp.memoryTypeCount;i++)
        if ((bits & (1u<<i)) && ((mp.memoryTypes[i].propertyFlags & props) == props))
            return i;
    throw std::runtime_error("no mem type");
}

VulkanContext::Buffer VulkanContext::createDeviceLocalBuffer(VkDeviceSize size, VkBufferUsageFlags usage) {
    Buffer b; b.size = size;
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = size;
    bi.usage = usage | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(m_device, &bi, nullptr, &b.buf) != VK_SUCCESS) throw std::runtime_error("buf fail");
    VkMemoryRequirements mr; vkGetBufferMemoryRequirements(m_device, b.buf, &mr);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = mr.size;
    ai.memoryTypeIndex = findMemoryType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(m_device, &ai, nullptr, &b.mem) != VK_SUCCESS) throw std::runtime_error("mem fail");
    vkBindBufferMemory(m_device, b.buf, b.mem, 0);
    return b;
}

void VulkanContext::destroy(Buffer b) {
    vkDestroyBuffer(m_device, b.buf, nullptr);
    vkFreeMemory(m_device, b.mem, nullptr);
}

VkCommandBuffer VulkanContext::beginOneShot() {
    VkCommandBufferAllocateInfo a{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    a.commandPool = m_cmdPool;
    a.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    a.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(m_device, &a, &cmd);
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);
    return cmd;
}

void VulkanContext::endOneShot(VkCommandBuffer cmd) {
    vkEndCommandBuffer(cmd);
    VkSubmitInfo s{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    s.commandBufferCount = 1; s.pCommandBuffers = &cmd;
    vkQueueSubmit(m_queue, 1, &s, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_queue);
    vkFreeCommandBuffers(m_device, m_cmdPool, 1, &cmd);
}

void VulkanContext::uploadWithStaging(const void* src, VkDeviceSize bytes, const Buffer& dst) {
    Buffer staging;
    // host-visible staging
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = bytes; bi.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(m_device, &bi, nullptr, &staging.buf) != VK_SUCCESS) throw std::runtime_error("buf fail");
    VkMemoryRequirements mr; vkGetBufferMemoryRequirements(m_device, staging.buf, &mr);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = mr.size;
    ai.memoryTypeIndex = findMemoryType(mr.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(m_device, &ai, nullptr, &staging.mem) != VK_SUCCESS) throw std::runtime_error("mem fail");
    vkBindBufferMemory(m_device, staging.buf, staging.mem, 0);

    void* map=nullptr; vkMapMemory(m_device, staging.mem, 0, bytes, 0, &map);
    std::memcpy(map, src, (size_t)bytes);
    vkUnmapMemory(m_device, staging.mem);

    VkCommandBuffer cmd = beginOneShot();
    VkBufferCopy c{0,0,bytes};
    vkCmdCopyBuffer(cmd, staging.buf, dst.buf, 1, &c);
    endOneShot(cmd);

    destroy(staging);
}

void VulkanContext::downloadWithStaging(const Buffer& src, void* dst, VkDeviceSize bytes) {
    Buffer staging;
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = bytes; bi.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(m_device, &bi, nullptr, &staging.buf) != VK_SUCCESS) throw std::runtime_error("buf fail");
    VkMemoryRequirements mr; vkGetBufferMemoryRequirements(m_device, staging.buf, &mr);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = mr.size;
    ai.memoryTypeIndex = findMemoryType(mr.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(m_device, &ai, nullptr, &staging.mem) != VK_SUCCESS) throw std::runtime_error("mem fail");
    vkBindBufferMemory(m_device, staging.buf, staging.mem, 0);

    VkCommandBuffer cmd = beginOneShot();
    VkBufferCopy c{0,0,bytes};
    vkCmdCopyBuffer(cmd, src.buf, staging.buf, 1, &c);
    endOneShot(cmd);

    void* map=nullptr; vkMapMemory(m_device, staging.mem, 0, bytes, 0, &map);
    std::memcpy(dst, map, (size_t)bytes);
    vkUnmapMemory(m_device, staging.mem);

    destroy(staging);
}

VkDescriptorSetLayout VulkanContext::createDescriptorSetLayout(const std::vector<VkDescriptorSetLayoutBinding>& bindings) {
    VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    ci.bindingCount = (uint32_t)bindings.size();
    ci.pBindings = bindings.data();
    VkDescriptorSetLayout layout{};
    if (vkCreateDescriptorSetLayout(m_device, &ci, nullptr, &layout) != VK_SUCCESS)
        throw std::runtime_error("dsl fail");
    return layout;
}

VkDescriptorPool VulkanContext::createDescriptorPool(const std::vector<VkDescriptorPoolSize>& sizes, uint32_t maxSets) {
    VkDescriptorPoolCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    ci.poolSizeCount = (uint32_t)sizes.size();
    ci.pPoolSizes = sizes.data();
    ci.maxSets = maxSets;
    VkDescriptorPool pool{};
    if (vkCreateDescriptorPool(m_device, &ci, nullptr, &pool) != VK_SUCCESS)
        throw std::runtime_error("pool fail");
    return pool;
}

VkDescriptorSet VulkanContext::allocDescriptorSet(VkDescriptorPool pool, VkDescriptorSetLayout layout) {
    VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    ai.descriptorPool = pool; ai.descriptorSetCount = 1; ai.pSetLayouts = &layout;
    VkDescriptorSet set{};
    if (vkAllocateDescriptorSets(m_device, &ai, &set) != VK_SUCCESS)
        throw std::runtime_error("alloc set fail");
    return set;
}

VulkanContext::Pipeline VulkanContext::createComputePipeline(const char* spv_path, const std::vector<VkPushConstantRange>& pushRanges, const std::vector<VkDescriptorSetLayout>& setLayouts) {
    auto code = readFile(spv_path);
    VkShaderModuleCreateInfo sm{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    sm.codeSize = code.size();
    sm.pCode = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule mod;
    if (vkCreateShaderModule(m_device, &sm, nullptr, &mod) != VK_SUCCESS)
        throw std::runtime_error("shader module fail");

    VkPipelineLayoutCreateInfo lci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    lci.setLayoutCount = (uint32_t)setLayouts.size();
    lci.pSetLayouts = setLayouts.data();
    lci.pushConstantRangeCount = (uint32_t)pushRanges.size();
    lci.pPushConstantRanges = pushRanges.data();
    Pipeline p{};
    if (vkCreatePipelineLayout(m_device, &lci, nullptr, &p.layout) != VK_SUCCESS)
        throw std::runtime_error("layout fail");

    VkPipelineShaderStageCreateInfo stage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT; stage.module = mod; stage.pName = "main";

    VkComputePipelineCreateInfo pci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    pci.stage = stage; pci.layout = p.layout;
    if (vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &pci, nullptr, &p.pipe) != VK_SUCCESS)
        throw std::runtime_error("pipeline fail");

    vkDestroyShaderModule(m_device, mod, nullptr);
    return p;
}

void VulkanContext::dispatch(const Pipeline& p, VkDescriptorSet set, uint32_t gx, uint32_t gy, uint32_t gz, const void* pushData, uint32_t pushBytes) {
    VkCommandBuffer cmd = beginOneShot();
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, p.pipe);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, p.layout, 0, 1, &set, 0, nullptr);
    if (pushBytes) vkCmdPushConstants(cmd, p.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, pushBytes, pushData);
    vkCmdDispatch(cmd, gx, gy, gz);
    endOneShot(cmd);
}

void VulkanContext::program(VkDeviceSize sizeA, VkDeviceSize sizeB, VkDeviceSize sizeC, 
    const float* hostA, const float* hostB, float* hostC, const char* spv_path, void* pc, uint32_t pc_size, 
    uint32_t gx, uint32_t gy, uint32_t gz)
{
    auto A = createDeviceLocalBuffer(sizeA, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    auto B = createDeviceLocalBuffer(sizeB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    auto C = createDeviceLocalBuffer(sizeC, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    uploadWithStaging(hostA, sizeA, A);
    uploadWithStaging(hostB, sizeB, B);

    VkDescriptorSetLayoutBinding b[3]{};
    for (int i = 0; i < 3; i++)
    {
        b[i].binding = i; 
        b[i].descriptorCount = 1; 
        b[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; 
        b[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }

    auto dsl = createDescriptorSetLayout( {b, b + 3} );

    VkPushConstantRange range{};
    range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    range.offset = 0;
    range.size = pc_size;
    auto pipe = createComputePipeline(spv_path, {range}, {dsl});

    VkDescriptorPoolSize ps{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3};
    auto pool = createDescriptorPool({ps}, 1);
    auto dset = allocDescriptorSet(pool, dsl);

    VkDescriptorBufferInfo infos[3] = {
         {A.buf, 0, VK_WHOLE_SIZE},
         {B.buf, 0, VK_WHOLE_SIZE},
         {C.buf, 0, VK_WHOLE_SIZE} 
        };

    VkWriteDescriptorSet w[3]{};
    for (int i = 0; i < 3; i++)
    {
        w[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w[i].dstSet = dset;
        w[i].dstBinding = i;
        w[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        w[i].descriptorCount = 1;
        w[i].pBufferInfo = &infos[i];
    }

    vkUpdateDescriptorSets(m_device, 3, w, 0, nullptr);

    dispatch(pipe, dset, gx, gy, gz, pc, pc_size);
    downloadWithStaging(C, hostC, sizeC);
    vkDestroyDescriptorPool(m_device, pool, nullptr);
    vkDestroyDescriptorSetLayout(m_device, dsl, nullptr);
    vkDestroyPipeline(m_device, pipe.pipe, nullptr);
    vkDestroyPipelineLayout(m_device, pipe.layout, nullptr);

    destroy(A);
    destroy(B);
    destroy(C);
}