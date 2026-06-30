#pragma once

#include <string>
#include <vector>

#include "device.hpp"

namespace cpe {
    class RayTracingPipeline {
        public:
            RayTracingPipeline(
                Device &device,
                const std::string &rgenPath,
                const std::string &missPath,
                const std::string &chitPath,
                VkRayTracingPipelineCreateInfoKHR &pielineInfo
            );
            ~RayTracingPipeline();


            // Helper
            void bind(VkCommandBuffer commandBuffer);
            VkPipeline getPipeline() { return m_Pipeline; };
        private:
            void createPipeline(
                const std::string &rgenPath,
                const std::string &missPath,
                const std::string &chitPath,
                VkRayTracingPipelineCreateInfoKHR &pipelineInfo
            );

            // Helpers
            std::vector<char> readFile(const std::string& filepath);
            void createShaderModule(const std::vector<char>& code, VkShaderModule* shaderModule);

            Device &m_Device;
            VkPipeline m_Pipeline;
            VkShaderModule rgenModule;
            VkShaderModule missModule;
            VkShaderModule chitModule;
    };
} // namespace cpe