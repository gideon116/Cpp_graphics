#define GLFW_INCLUDE_VULKAN
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <fstream>

template <typename T>
class num_ptr_arr
{
    public:
        T* ptr = nullptr;
        uint32_t len = 0;

        num_ptr_arr() = default;
        explicit num_ptr_arr(uint32_t num) : ptr(new T[num]), len(num) {}
        num_ptr_arr(uint32_t num, T fill) : ptr(new T[num]), len(num)
        {
            for (uint32_t i = 0; i < num; i++)
                ptr[i] = fill;
        }
        num_ptr_arr(std::initializer_list<T> list) : len((uint32_t)list.size())
        {
            ptr = new T[len];
            uint32_t index = 0;
            for (auto& i : list)
            {
                ptr[index++] = i;
            }
        }
        uint32_t tot_size() const
        { 
            return (uint32_t)sizeof(T) * len; 
        
        }

        void resize(const uint32_t new_len)
        {
            if (new_len == len) return;

            if (!new_len) std::cout << "[WARNING]: the resize length is 0\n";
            
            T* new_ptr = new_len ? new T[new_len] : nullptr;
            uint32_t copy_len = new_len > len ? len : new_len;

            if (new_len < len) std::cout << "[WARNING]: the resize length is smaller than the original length, this will cause data loss\n";

            for (uint32_t i = 0; i < copy_len; i++)
                new_ptr[i] = ptr[i];

            len = new_len;
            delete[] ptr;
            ptr = new_ptr;
            new_ptr = nullptr;
        }

        // rule of 5
        num_ptr_arr(const num_ptr_arr&) = delete;
        num_ptr_arr& operator=(const num_ptr_arr&) = delete;
        num_ptr_arr(num_ptr_arr&& other) noexcept : ptr(other.ptr), len(other.len) { other.ptr = nullptr; other.len = 0; }
        num_ptr_arr& operator=(num_ptr_arr&& other) noexcept
        {
            if (this != &other)
            {
                delete[] ptr;
                ptr = other.ptr;
                len = other.len;
                other.ptr = nullptr;
                other.len = 0;
            }
            return *this;
        }
        ~num_ptr_arr() { delete[] ptr; }
        
};

// read helper
static num_ptr_arr<char> readFile(const char* filename) {
    
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open())
        throw std::runtime_error("failed to open file!");

    size_t fileSize = (size_t) file.tellg();
    num_ptr_arr<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.ptr, fileSize);

    file.close();

    return buffer;
}

struct QueueFamilyIndices
{
    uint32_t graphicsFamily = UINT32_MAX;
    uint32_t graphicsFamily_value() { return graphicsFamily % UINT32_MAX; }

    uint32_t presentFamily = UINT32_MAX;
    uint32_t presentFamily_value() { return presentFamily % UINT32_MAX; }
    
    bool hasvalue()
    {
        if (   (graphicsFamily != UINT32_MAX) && 
                (presentFamily != UINT32_MAX)   )
                return true;

        return false;
    }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    num_ptr_arr<VkSurfaceFormatKHR> formats;
    num_ptr_arr<VkPresentModeKHR> presentModes;

};

struct Vertex
{
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    bool operator==(const Vertex& other) const 
    {
        return pos == other.pos && color == other.color && texCoord == other.texCoord;
    }

    static VkVertexInputBindingDescription getBindingDescription()
    {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions()
    {
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

        return attributeDescriptions;
    }

};

namespace std {
    template<> struct hash<Vertex>
    {
        size_t operator()(Vertex const& vertex) const
        {
            return ((hash<glm::vec3>()(vertex.pos) ^
                   (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
                   (hash<glm::vec2>()(vertex.texCoord) << 1);
        }
    };
}


struct Transform {
    glm::vec3 pos{0,0,0};
    glm::vec3 rot{0,0,0};
    glm::vec3 scale{1,1,1};

    glm::mat4 model() const {
        glm::mat4 M(1.0f);
        M = glm::translate(M, pos);
        M = glm::rotate(M, rot.y, {0,1,0});
        M = glm::rotate(M, rot.x, {1,0,0});
        M = glm::rotate(M, rot.z, {0,0,1});
        M = glm::scale(M, scale);
        return M;
    }
};

struct GameObject {
    Transform xf;
    VkBuffer vertexBuffer{};
    VkBuffer indexBuffer{};
    uint32_t indexCount{};
};