#include "point_light_system.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FROCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"
#include <glm/gtc/constants.hpp>

#include <cassert>
#include <stdexcept>
#include <array>

namespace cpe {
    struct PointLightPushConstant {
        glm::vec4 position{};
        glm::vec4 color{};
        float radius;
    };

    PointLightSystem::PointLightSystem(Device& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout) : m_Device{device}, pipelineLayout{VK_NULL_HANDLE}  {
        createPipelineLayout(globalSetLayout);
        try {
            createPipeline(renderPass);
        } catch (...) {
            if (pipelineLayout != VK_NULL_HANDLE) {
                vkDestroyPipelineLayout(m_Device.device(), pipelineLayout, nullptr);
                pipelineLayout = VK_NULL_HANDLE;
            }
            throw;
        }
    };

    PointLightSystem::~PointLightSystem() {
        m_Pipeline.reset();
        vkDestroyPipelineLayout(m_Device.device(), pipelineLayout, nullptr);
    };

    void PointLightSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout) {
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(PointLightPushConstant);

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
            nullptr, &pipelineLayout
        ) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        };
    };

    void PointLightSystem::createPipeline(VkRenderPass renderPass) {
        assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

        PipelineConfigInfo pipelineConfig{};
        Pipeline::defaultPipelineConfigInfo(pipelineConfig);
        pipelineConfig.bindingDescriptions.clear();
        pipelineConfig.attributeDescriptions.clear();
        pipelineConfig.renderPass = renderPass;
        pipelineConfig.pipelineLayout = pipelineLayout;
        m_Pipeline = std::make_unique<Pipeline>(
            m_Device,
            "shaders/point_light.vert.spv",
            "shaders/point_light.frag.spv",
            pipelineConfig
        );
    };

    void PointLightSystem::update(FrameInfo &frameInfo, GlobalUbo &ubo) {
        int lightIndex = 0;
        for(auto& kv: frameInfo.gameObjects) {
            auto& obj = kv.second;
            if(obj.pointLight == nullptr) continue;

            ubo.pointLights[lightIndex].position = glm::vec4(obj.transform.translation, 1.f);
            ubo.pointLights[lightIndex].color = glm::vec4(obj.color, obj.pointLight->lightIntensity);
            lightIndex += 1;
        };
        ubo.numLights = lightIndex;
    };

    void PointLightSystem::render(FrameInfo &frameInfo) {
        m_Pipeline->bind(frameInfo.commandBuffer);

        assert(frameInfo.globalDescriptorSet != VK_NULL_HANDLE && "global descriptor set is null");

        vkCmdBindDescriptorSets(
            frameInfo.commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout,
            0, 1,
            &frameInfo.globalDescriptorSet,
            0, nullptr
        );

        for(auto& kv: frameInfo.gameObjects) {
            auto& obj = kv.second;
            if(obj.pointLight == nullptr) continue;

            PointLightPushConstant push{};
            push.position = glm::vec4(obj.transform.translation, 1.f);
            push.color = glm::vec4(obj.color, obj.pointLight->lightIntensity);
            push.radius = obj.transform.scale.x;

            vkCmdPushConstants(
                frameInfo.commandBuffer,
                pipelineLayout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(PointLightPushConstant),
                &push
            );
            vkCmdDraw(frameInfo.commandBuffer, 6, 1, 0, 0);
        };
    };
}