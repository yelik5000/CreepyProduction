#pragma once

#include "camera.hpp"
#include "device.hpp"
#include "game_object.hpp"
#include "pipeline.hpp"
#include "frame_info.hpp"

#include <memory>
#include <vector>
#include <string>

namespace cpe {
    class SimpleRenderSystem {
        public:
            SimpleRenderSystem(Device& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
            ~SimpleRenderSystem();

            SimpleRenderSystem(const SimpleRenderSystem &) = delete;
            SimpleRenderSystem &operator=(const SimpleRenderSystem &) = delete;

            void renderGameObjects(FrameInfo &frameInfo);
        private:
            void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
            void createPipeline(VkRenderPass renderPass);
            Device& m_Device;
            std::unique_ptr<Pipeline> m_Pipeline;
            VkPipelineLayout pipelineLayout;
    };
}