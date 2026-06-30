#pragma once

#include "device.hpp"
#include "swap_chain.hpp"
#include "window.hpp"

#include <cassert>
#include <memory>
#include <vector>
#include <string>

namespace cpe {
    class Renderer {
        public:
            Renderer(Window &window, Device &device);
            ~Renderer();

            Renderer(const Renderer &) = delete;
            Renderer &operator=(const Renderer &) = delete;

            VkRenderPass getSwapChainRenderPass() const { return m_SwapChain->getRenderPass(); };
            float getAspectRatio() { return m_SwapChain->extentAspectRatio(); };
            bool isFrameInProgress() const { return isFrameStarted; };

            VkCommandBuffer getCurrentCommandBuffer() const {
                assert(isFrameStarted && "Cannot get command budfer when the frame is not in progress");
                return commandBuffers[currentFrameIndex];
            };

            int getFrameIndex() {
                assert(isFrameStarted && "Cannot get frame index when the frame is not in progress");
                return currentFrameIndex;
            }

            VkCommandBuffer beginFrame();
            void endFrame();
            void beginSwapChainRenderpass(VkCommandBuffer commandBuffer);
            void endSwapChainRenderpass(VkCommandBuffer commandBuffer);
        private:
            void createCommandBuffers();
            void freeCommandBuffers();
            void recreateSwapChain();
            Window& m_Window;
            Device& m_Device;
            std::unique_ptr<SwapChain> m_SwapChain;
            std::vector<VkCommandBuffer> commandBuffers;

            uint32_t currentImageIndex;
            int currentFrameIndex = 0;
            bool isFrameStarted = false;
    };
}