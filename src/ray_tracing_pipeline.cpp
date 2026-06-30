#include "ray_tracing_pipeline.hpp"

#include <fstream>
#include <stdexcept>
#include <iostream>

#ifndef ENGINE_DIR
#define ENGINE_DIR "./"
#endif

namespace cpe {
    RayTracingPipeline::RayTracingPipeline(
        Device &device,
        const std::string &rgenPath,
        const std::string &missPath,
        const std::string &chitPath,
        VkRayTracingPipelineCreateInfoKHR &pipelineInfo
    ) : m_Device{device} {
        createPipeline(
            rgenPath,
            missPath,
            chitPath,
            pipelineInfo
        );
    };
    RayTracingPipeline::~RayTracingPipeline() {
        vkDestroyPipeline(m_Device.device(), m_Pipeline, nullptr);
    };

    void RayTracingPipeline::createPipeline(
        const std::string &rgenPath,
        const std::string &missPath,
        const std::string &chitPath,
        VkRayTracingPipelineCreateInfoKHR &pipelineInfo
    ) {
        PFN_vkCreateRayTracingPipelinesKHR pfnVkCreateRayTracingPipelinesKHR =
            reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(
                vkGetDeviceProcAddr(m_Device.device(), "vkCreateRayTracingPipelinesKHR"));

        auto rgenShaderCode = readFile(rgenPath);
        auto missShaderCode = readFile(missPath);
        auto chitShaderCode = readFile(chitPath);

        createShaderModule(rgenShaderCode, &rgenModule);
        createShaderModule(missShaderCode, &missModule);

        std::vector<VkPipelineShaderStageCreateInfo> shaderStages{3};

        VkPipelineShaderStageCreateInfo shaderStage{};
        shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStage.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        shaderStage.module = rgenModule;
        shaderStage.pName = "main";
        shaderStages.emplace_back(shaderStage);

        shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
        shaderStage.module = missModule;
        shaderStage.pName = "main";
        shaderStages.emplace_back(shaderStage);

        shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStage.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        shaderStage.module = chitModule;
        shaderStage.pName = "main";
        shaderStages.emplace_back(shaderStage);

        std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups{3};
        VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};

        shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        shaderGroup.generalShader = 0; // the first index
        shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
        shaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
        shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
        shaderGroups.emplace_back(shaderGroup);

        shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        shaderGroup.generalShader = 1; // the second index
        shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
        shaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
        shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
        shaderGroups.emplace_back(shaderGroup);
        
        shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
        shaderGroup.generalShader = 0; // the first index
        shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
        shaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
        shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
        shaderGroups.emplace_back(shaderGroup);

        pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
        pipelineInfo.pStages = shaderStages.data();
        pipelineInfo.groupCount = static_cast<uint32_t>(shaderGroups.size());
        pipelineInfo.pGroups = shaderGroups.data();

        if (pfnVkCreateRayTracingPipelinesKHR(
            m_Device.device(), 
            VK_NULL_HANDLE,
            VK_NULL_HANDLE,
            1, 
            &pipelineInfo, 
            nullptr, 
            &m_Pipeline
        ) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics pipeline");
        };
    };

    // Helpers
    std::vector<char> RayTracingPipeline::readFile(const std::string& filepath) {
        std::string enginePath = ENGINE_DIR + filepath;
        std::ifstream file(enginePath, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            throw std::runtime_error("failed to open file: " + enginePath);
        };

        size_t fileSize = (size_t) file.tellg();
        std::vector<char> buffer(fileSize);


        file.seekg(0);
        file.read(buffer.data(), fileSize);

        file.close();
        return buffer;
    };
    void RayTracingPipeline::createShaderModule(const std::vector<char>& code, VkShaderModule* shaderModule) {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        if (vkCreateShaderModule(m_Device.device(), &createInfo, nullptr, shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("failed to create shader module");
        };
    };
    void RayTracingPipeline::bind(VkCommandBuffer commandBuffer) {
        vkCmdBindPipeline(
            commandBuffer, 
            VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, 
            m_Pipeline
        );
    };
} // namespace cpe