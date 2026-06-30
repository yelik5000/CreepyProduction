#pragma once

#include "camera.hpp"
#include "device.hpp"
#include "game_object.hpp"
#include "ray_tracing_pipeline.hpp"
#include "frame_info.hpp"

#include <memory>
#include <vector>
#include <string>

namespace cpe {
    class RayTracingSystem {
        public:
            RayTracingSystem(Device& device, VkDescriptorSetLayout globalSetLayout);
            ~RayTracingSystem();

            RayTracingSystem(const RayTracingSystem &) = delete;
            RayTracingSystem &operator=(const RayTracingSystem &) = delete;

            void renderGameObjects(FrameInfo &frameInfo);
        private:
            void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
            void createPipeline();
            Device& m_Device;
            std::unique_ptr<RayTracingPipeline> m_Pipeline;
            VkPipelineLayout m_Layout;
    };
}