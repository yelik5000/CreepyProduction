#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "device.hpp"
#include "ray_tracing_pipeline.hpp"

namespace cpe {
    class ShaderBindingTable {
        public:
            ShaderBindingTable(Device &device, RayTracingPipeline &pipeline);
            ~ShaderBindingTable();

            VkStridedDeviceAddressRegionKHR getRaygenRegion() { return raygenRegion; };
            VkStridedDeviceAddressRegionKHR getMissRegion() { return missRegion; };
            VkStridedDeviceAddressRegionKHR getHitRegion() { return hitRegion; };
            VkStridedDeviceAddressRegionKHR getCallableRegion() { return callableRegion; };

        private:
            void createShaderBindingTable(const VkPipeline &rayTracingPipeline);

            inline uint32_t allignBinding(uint32_t size, uint32_t allignment) {
                return (size + allignment - 1) & ~(allignment - 1);
            };

            Device &m_Device;
            RayTracingPipeline &m_Pipeline;
            VkBuffer sbtBuffer;
            VkDeviceMemory sbtBufferMemory;

            VkStridedDeviceAddressRegionKHR raygenRegion{};
            VkStridedDeviceAddressRegionKHR missRegion{};
            VkStridedDeviceAddressRegionKHR hitRegion{};
            VkStridedDeviceAddressRegionKHR callableRegion{};
    };
} // namespace cpe