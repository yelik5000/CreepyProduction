#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <string>

namespace cpe {
    class Window {
        public:
            Window(int w, int h, std::string name);
            ~Window();

            Window(const Window &) = delete;
            Window &operator=(const Window &) = delete;

            bool shouldClose() { return glfwWindowShouldClose(window); };
            VkExtent2D getExtent() { return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}; };
            bool wasWindowResized() { return frameBufferResized; };
            void resetWindowResizedFlag() { frameBufferResized = false; };
            GLFWwindow *getGLFWwindow() const { return window; };

            void createWindowSurface(VkInstance instance, VkSurfaceKHR *surface);

            uint32_t getWidth() { return width; };
            uint32_t getHeight() { return height; };
        private:
            static void framebufferResizedCallback(GLFWwindow *window, int width, int height);
            void initWindow();

            int width;
            int height;
            bool frameBufferResized = false;

            std::string windowName;
            GLFWwindow *window;
    };
}