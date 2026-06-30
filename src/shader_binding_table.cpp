#include "shader_binding_table.hpp"
#include <vector>
#include <cstring>

namespace cpe {
    ShaderBindingTable::ShaderBindingTable(Device &device, RayTracingPipeline &pipeline) : m_Device{device}, m_Pipeline{pipeline}{
        createShaderBindingTable(m_Pipeline.getPipeline());
    };
    ShaderBindingTable::~ShaderBindingTable() {
        vkDestroyBuffer(m_Device.device(), sbtBuffer, nullptr);
        vkFreeMemory(m_Device.device(), sbtBufferMemory, nullptr);
    };

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

        uint32_t raygenRegionSize = allignBinding(handleSizeAlligned, baseAllignment);
        uint32_t missRegionSize = allignBinding(1 * handleSizeAlligned, baseAllignment); // temporary hardcode
        uint32_t hitRegionSize = allignBinding(1 * handleSizeAlligned, baseAllignment);
        uint32_t callableRegionSize = allignBinding(0 * handleSizeAlligned, baseAllignment);

        VkDeviceSize sbtBufferSize = raygenRegionSize + missRegionSize + hitRegionSize + callableRegionSize;

        VkBuffer sbtBuffer;
        VkDeviceMemory sbtBufferMemory;
        m_Device.createBuffer(
            sbtBufferSize,
            VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
            sbtBuffer,
            sbtBufferMemory
        );

        VkBufferDeviceAddressInfo sbtAddressInfo{};
        sbtAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        sbtAddressInfo.buffer = sbtBuffer;
        VkDeviceAddress sbtAddress = vkGetBufferDeviceAddress(m_Device.device(), &sbtAddressInfo);

        uint8_t* pData;
        vkMapMemory(m_Device.device(), sbtBufferMemory, 0, sbtBufferSize, 0, (void**)&pData);

        uint8_t* pHandles;
        memcpy(pData, pHandles, handleSize);
        pData += raygenRegionSize;
        pHandles += handleSize;

        memcpy(pData, pHandles, handleSize);
        pData += missRegionSize;
        pHandles += handleSize;

        memcpy(pData, pHandles, handleSize);
        pData += hitRegionSize;
        pHandles += handleSize;
        
        memcpy(pData, pHandles, handleSize);
        pData += callableRegionSize;
        pHandles += handleSize;

        vkUnmapMemory(m_Device.device(), sbtBufferMemory);

        raygenRegion.deviceAddress = sbtAddress;
        raygenRegion.stride = raygenRegionSize;
        raygenRegion.size = raygenRegionSize;

        missRegion.deviceAddress = sbtAddress + raygenRegionSize;
        missRegion.stride = handleSizeAlligned;
        missRegion.size = missRegionSize;

        hitRegion.deviceAddress = sbtAddress + raygenRegionSize + missRegionSize;
        hitRegion.stride = handleSizeAlligned;
        hitRegion.size = hitRegionSize;

        callableRegion.deviceAddress = sbtAddress + raygenRegionSize + missRegionSize + hitRegionSize;
        callableRegion.stride = handleSizeAlligned;
        callableRegion.size = callableRegionSize;
    };
} // namespace cpe