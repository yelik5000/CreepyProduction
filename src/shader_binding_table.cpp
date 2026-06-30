#include "shader_binding_table.hpp"
#include <vector>

namespace cpe {
    ShaderBindingTable::ShaderBindingTable(Device &device, RayTracingPipeline &pipeline) : m_Device{device}, m_Pipeline{pipeline}{
        createShaderBindingTable(m_Pipeline.getPipeline());
    }


    void ShaderBindingTable::createShaderBindingTable(const VkPipeline &rayTracingPipeline) {
        PFN_vkGetRayTracingShaderGroupHandlesKHR pfnVkGetRayTracingShaderGroupHandlesKHR = 
            reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(
                vkGetDeviceProcAddr(m_Device.device(), "vkGetRayTracingShaderGroupHandlesKHR"));

        VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
        };
        VkPhysicalDeviceProperties2 prop2{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR,
            &rtProperties
        };

        vkGetPhysicalDeviceProperties2(m_Device.getPhysicalDevice(), &prop2);

        uint32_t handleSize = rtProperties.shaderGroupHandleSize;
        uint32_t handleAllignment = rtProperties.shaderGroupHandleAlignment;
        uint32_t baseAllignment = rtProperties.shaderGroupBaseAlignment;

        uint32_t handleSizeAlligned = allignBinding(handleSize, handleAllignment);

        uint32_t groupCount = 3; //temporary hardcode
        uint32_t dataSize = groupCount * handleSize;

        std::vector<uint8_t> shaderHandleStorage(dataSize);

        pfnVkGetRayTracingShaderGroupHandlesKHR(
            m_Device.device(),
            rayTracingPipeline,
            0,
            groupCount,
            dataSize,
            shaderHandleStorage.data()
        );
    };
} // namespace cpe