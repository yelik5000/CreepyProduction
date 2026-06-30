#include "renderer.hpp"

#include <cassert>
#include <stdexcept>
#include <array>

namespace cpe {
    Renderer::Renderer(Window &window, Device &device) : m_Window{window}, m_Device{device} {
        recreateSwapChain();
        createCommandBuffers();
    };

    Renderer::~Renderer() {
        freeCommandBuffers();
    };

    void Renderer::recreateSwapChain() {
        auto extent = m_Window.getExtent();
        while(extent.width == 0 || extent.height == 0) {
            extent = m_Window.getExtent();
            glfwWaitEvents();
        };

        vkDeviceWaitIdle(m_Device.device());

        if(m_SwapChain == nullptr) {
            m_SwapChain = std::make_unique<SwapChain>(m_Device, extent);
        } else {
            std::shared_ptr<SwapChain> oldSwapChain = std::move(m_SwapChain);
            m_SwapChain = std::make_unique<SwapChain>(m_Device, extent, oldSwapChain);

            if(!oldSwapChain->compareSwapFormats(*m_SwapChain.get())) {
                throw std::runtime_error("Swap chain image format has changed");
            };
        };
    };

    void Renderer::freeCommandBuffers() {
        vkFreeCommandBuffers(
            m_Device.device(), 
            m_Device.getCommandPool(), 
            static_cast<uint32_t>(commandBuffers.size()), 
            commandBuffers.data()
        );
        commandBuffers.clear();
    };

    void Renderer::createCommandBuffers() {
        commandBuffers.resize(SwapChain::MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = m_Device.getCommandPool();
        allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

        if (vkAllocateCommandBuffers(
            m_Device.device(), 
            &allocInfo, 
            commandBuffers.data()
        ) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers");
        };
    };

    VkCommandBuffer Renderer::beginFrame() {
        assert(!isFrameStarted && "Cannot call baginFrame() while already in progress");
        
        auto result = m_SwapChain->acquireNextImage(&currentImageIndex);

        if(result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapChain();
            return nullptr;
        };
        if(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            std::runtime_error("failed to aquire swap chain image");
        };

        isFrameStarted = true;
        auto commandBuffer = getCurrentCommandBuffer();
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer");
        };

        return commandBuffer;
    };

    void Renderer::endFrame() {
        assert(isFrameStarted && "Cannot call endFrame() while frame is not in progress");
        auto commandBuffer = getCurrentCommandBuffer();

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer");
        };

        auto result = m_SwapChain->submitCommandBuffers(&commandBuffer, &currentImageIndex);
        if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_Window.wasWindowResized()) {
            m_Window.resetWindowResizedFlag();
            recreateSwapChain();
        }
        else if(result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image");
        };

        isFrameStarted = false;
        currentFrameIndex = (currentFrameIndex + 1) % SwapChain::MAX_FRAMES_IN_FLIGHT;
    };

    void Renderer::beginSwapChainRenderpass(VkCommandBuffer commandBuffer) {
        assert(isFrameStarted && "Cannot call beginSwapChainRenderpass() if frameis not in progress");
        assert(commandBuffer == getCurrentCommandBuffer() && "Cannot begin redner pass on a command buffer from a different frame");

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_SwapChain->getRenderPass();
        renderPassInfo.framebuffer = m_SwapChain->getFrameBuffer(currentImageIndex);

        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = m_SwapChain->getSwapChainExtent();

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {0.01f, 0.01f, 0.01f, 1.0f};
        clearValues[1].depthStencil = {1.0f, 0};
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(m_SwapChain->getSwapChainExtent().width);
        viewport.height = static_cast<float>(m_SwapChain->getSwapChainExtent().height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        VkRect2D scissor{{0, 0}, m_SwapChain->getSwapChainExtent()};
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    };

    void Renderer::endSwapChainRenderpass(VkCommandBuffer commandBuffer) {
        assert(isFrameStarted && "Cannot call endSwapChainRenderpass() if frameis not in progress");
        assert(commandBuffer == getCurrentCommandBuffer() && "Cannot end redner pass on a command buffer from a different frame");
        vkCmdEndRenderPass(commandBuffer);
    };
}