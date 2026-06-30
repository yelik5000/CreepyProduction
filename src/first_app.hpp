#pragma once

#include "descriptors.hpp"
#include "device.hpp"
#include "game_object.hpp"
#include "renderer.hpp"
#include "window.hpp"
#include "frame_info.hpp"
#include "ray_tracing_pipeline.hpp"

#include <memory>
#include <vector>
#include <string>
#include <map>

namespace cpe {
    struct AccelerationStructureResources {
        VkAccelerationStructureKHR handle{VK_NULL_HANDLE};
        VkBuffer buffer{VK_NULL_HANDLE};
        VkDeviceMemory memory{VK_NULL_HANDLE};
        VkBuffer scratchBuffer{VK_NULL_HANDLE};
        VkDeviceMemory scratchMemory{VK_NULL_HANDLE};
    };

    class FirstApp {
        public:
            static constexpr int WIDTH = 800;
            static constexpr int HEIGHT = 600;

            FirstApp();
            ~FirstApp();

            FirstApp(const FirstApp &) = delete;
            FirstApp &operator=(const FirstApp &) = delete;

            void run();
        private:
            void loadGameObjects();
            void buildBLAS(GameObject &gameObject, VkCommandBuffer commandBuffer);
            void buildTLAS(VkCommandBuffer commandBuffer);
            void cleanupAccelerationStructures();
            
            Window m_Window{WIDTH, HEIGHT, std::string("Hello Vulkan!")};
            Device m_Device{m_Window};
            Renderer m_Renderer{m_Window, m_Device};

            std::unique_ptr<DescriptorPool> globalPool{};
            GameObject::Map gameObjects;
            
            std::map<uint32_t, AccelerationStructureResources> accelerationStructures;

            AccelerationStructureResources m_TlasResources{};
    };
}