#include "ray_tracing_system.hpp"

#include "shader_binding_table.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"
#include <glm/gtc/constants.hpp>

#include <cassert>
#include <stdexcept>
#include <array>

namespace cpe {
    struct SimplePushConstantData {
        glm::mat4 modelMatrix{1.f};
        glm::mat4 normalMatrix{1.f};
    };

    RayTracingSystem::RayTracingSystem(Device& device, VkDescriptorSetLayout globalSetLayout) : m_Device{device}, m_Layout{VK_NULL_HANDLE} {
        createPipelineLayout(globalSetLayout);
        try {
            createPipeline();
            m_ShaderBindingTable = std::make_unique<ShaderBindingTable>(m_Device, *m_Pipeline);
        } catch (...) {
            if (m_Layout != VK_NULL_HANDLE) {
                vkDestroyPipelineLayout(m_Device.device(), m_Layout, nullptr);
                m_Layout = VK_NULL_HANDLE;
            }
            throw;
        }
    };

    RayTracingSystem::~RayTracingSystem() {
        m_Pipeline.reset();
        m_ShaderBindingTable.reset();
        vkDestroyPipelineLayout(m_Device.device(), m_Layout, nullptr);
    };

    void RayTracingSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout) {
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags =
            VK_SHADER_STAGE_RAYGEN_BIT_KHR |
            VK_SHADER_STAGE_MISS_BIT_KHR |
            VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(SimplePushConstantData);

        std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout};

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
        pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        if (vkCreatePipelineLayout(
            m_Device.device(), 
            &pipelineLayoutInfo, 
            nullptr, &m_Layout
        ) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        };
    };

    void RayTracingSystem::createPipeline() {
        assert(m_Layout != nullptr && "Cannot create pipeline before pipeline layout");
        VkRayTracingPipelineCreateInfoKHR pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
        pipelineInfo.maxPipelineRayRecursionDepth = 2; // temporary hardcode
        pipelineInfo.layout = m_Layout;
        
        m_Pipeline = std::make_unique<RayTracingPipeline>(
            m_Device,
            "shaders/ray_tracing_shader.rgen.spv",
            "shaders/ray_tracing_shader.miss.spv",
            "shaders/ray_tracing_shader.chit.spv",
            pipelineInfo
        );
    };

    void RayTracingSystem::renderGameObjects(FrameInfo &frameInfo) {
        m_Pipeline->bind(frameInfo.commandBuffer);

        assert(frameInfo.globalDescriptorSet != VK_NULL_HANDLE && "global descriptor set is null");

        vkCmdBindDescriptorSets(
            frameInfo.commandBuffer,
            VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
            m_Layout,
            0, 1,
            &frameInfo.globalDescriptorSet,
            0, nullptr
        );

        //vkCmdTraceRaysKHR(
        //    frameInfo.commandBuffer,
        //    &m_ShaderBindingTable->getRaygenRegion(),
        //    &m_ShaderBindingTable->getMissRegion(),
        //    &m_ShaderBindingTable->getHitRegion(),
        //    &m_ShaderBindingTable->getCallableRegion(),
        //    frameInfo.width,
        //    frameInfo.height,
        //    1                   // temporary hardcode
        //);

        // Not tracing right now, since there are no shaders
    };
}