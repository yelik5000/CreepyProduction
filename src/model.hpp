#pragma once

#include "device.hpp"
#include "buffer.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <memory>
#include <vector>

namespace cpe {
    class Model{
        public:
            struct Vertex {
                glm::vec3 position{};
                glm::vec3 color{};
                glm::vec3 normal{};
                glm::vec2 uv{};

                static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
                static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();

                bool operator==(const Vertex &other) const {
                    return position == other.position && color == other.color && normal == other.normal && uv == other.uv;
                };
            };

            struct Builder {
                std::vector<Vertex> vertices{};
                std::vector<uint32_t> indices{};

                void loadModel(const std::string &filepath);
            };

            Model(Device &device, const Model::Builder &builder);
            ~Model();

            Model(const Model &) = delete;
            Model &operator=(const Model &) = delete;

            static std::unique_ptr<Model> createModelFromFile(Device &device, const std::string &filepath);

            void bind(VkCommandBuffer commandBuffer);
            void draw(VkCommandBuffer commandBuffer);

            VkBuffer getVertexBufferHandle() const { return vertexBuffer->getBuffer(); }
            VkDeviceSize getVertexBufferSize() const { return vertexBuffer->getBufferSize(); }
            uint32_t getVertexCount() const { return vertexCount; }
            VkDeviceSize getVertexStride() const { return sizeof(Vertex); }
            VkDeviceAddress getVertexBufferDeviceAddress() const { return vertexBuffer->getDeviceAddress(); }

            bool hasIndexBufferData() const { return hasIndexBuffer; }
            VkBuffer getIndexBufferHandle() const { return indexBuffer ? indexBuffer->getBuffer() : VK_NULL_HANDLE; }
            VkDeviceAddress getIndexBufferDeviceAddress() const { return indexBuffer ? indexBuffer->getDeviceAddress() : 0; }
            uint32_t getIndexCount() const { return indexCount; }

        private:
            void createVertexBuffers(const std::vector<Vertex> &vertices);
            void createIndexBuffers(const std::vector<uint32_t> &indices);
        
            Device &m_Device;

            std::unique_ptr<Buffer> vertexBuffer;

            uint32_t vertexCount;

            bool hasIndexBuffer = false;
            std::unique_ptr<Buffer> indexBuffer;
            uint32_t indexCount;
    };
}