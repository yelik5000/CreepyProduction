#include "first_app.hpp"

#include "keyboard_movement_controller.hpp"
#include "camera.hpp"
#include "buffer.hpp"
#include "shader_binding_table.hpp"
#include "systems/simple_render_system.hpp"
#include "systems/point_light_system.hpp"
#include "systems/ray_tracing_system.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FROCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"
#include <glm/gtc/constants.hpp>

#include <cassert>
#include <chrono>
#include <stdexcept>
#include <array>
#include <cstring>

#define MAX_FRAME_TIME 60.f

namespace cpe {
    FirstApp::FirstApp() {
        globalPool = DescriptorPool::Builder(m_Device)
            .setMaxSets(SwapChain::MAX_FRAMES_IN_FLIGHT * 10)
            .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, SwapChain::MAX_FRAMES_IN_FLIGHT * 10)
            .build();
        loadGameObjects();
    };

    FirstApp::~FirstApp() {
        cleanupAccelerationStructures();
    };

    void FirstApp::runRasterizer() {
        std::vector<std::unique_ptr<Buffer>> uboBuffers(SwapChain::MAX_FRAMES_IN_FLIGHT);
        for(int i = 0; i < uboBuffers.size(); i ++) {
            uboBuffers[i] = std::make_unique<Buffer>(
                m_Device,
                sizeof(GlobalUbo),
                1,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                m_Device.properties.limits.minUniformBufferOffsetAlignment
            );
            uboBuffers[i]->map();
        };

        auto globalSetLayout = DescriptorSetLayout::Builder(m_Device)
            .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
            .build();

        std::vector<VkDescriptorSet> globalDescriptorSets(SwapChain::MAX_FRAMES_IN_FLIGHT);

        for(int i = 0; i < globalDescriptorSets.size(); i ++) {
            auto bufferInfo = uboBuffers[i]->descriptorInfo();
            if (!DescriptorWriter(*globalSetLayout, *globalPool)
                    .writeBuffer(0, &bufferInfo)
                    .build(globalDescriptorSets[i])) {
                throw std::runtime_error("failed to allocate global descriptor set");
            }
        };

        SimpleRenderSystem simpleRenderSystem{
            m_Device,
            m_Renderer.getSwapChainRenderPass(),
            globalSetLayout->getDescriptorSetLayout()
        };
        PointLightSystem pointLightSystem{
            m_Device,
            m_Renderer.getSwapChainRenderPass(),
            globalSetLayout->getDescriptorSetLayout()
        };
        Camera camera{};

        auto viewerObject = GameObject::createGameObject();
        viewerObject.transform.translation.z = -2.5f;
        KeyboardMovementController cameraController{};

        auto currentTime = std::chrono::high_resolution_clock::now();

        while(!m_Window.shouldClose()) {
            glfwPollEvents();

            auto newTime = std::chrono::high_resolution_clock::now();
            float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
            currentTime = newTime;

            frameTime = glm::min(frameTime, MAX_FRAME_TIME);

            cameraController.moveInPlaneXZ(m_Window.getGLFWwindow(), frameTime, viewerObject);
            camera.setViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);

            float aspect = m_Renderer.getAspectRatio();
            camera.setPerspectiveProjection(glm::radians(50.f), aspect, .1f, 100.f);

            if(auto commandBuffer = m_Renderer.beginFrame()) {
                int frameIndex = m_Renderer.getFrameIndex();
                FrameInfo frameInfo{
                    frameIndex,
                    frameTime,
                    commandBuffer,
                    camera,
                    globalDescriptorSets[frameIndex],
                    gameObjects
                };

                GlobalUbo ubo{};
                ubo.projection = camera.getProjection();
                ubo.view = camera.getView();
                ubo.invView = camera.getInvView();
                pointLightSystem.update(frameInfo, ubo);
                uboBuffers[frameIndex]->writeToBuffer(&ubo);
                uboBuffers[frameIndex]->flush();

                m_Renderer.beginSwapChainRenderpass(commandBuffer);
                simpleRenderSystem.renderGameObjects(frameInfo);
                pointLightSystem.render(frameInfo);
                m_Renderer.endSwapChainRenderpass(commandBuffer);
                m_Renderer.endFrame();
            };
        };
        cleanupAccelerationStructures();
        vkDeviceWaitIdle(m_Device.device());
    };
    void FirstApp::runRayTracer() {
        std::vector<std::unique_ptr<Buffer>> uboBuffers(SwapChain::MAX_FRAMES_IN_FLIGHT);
        for(int i = 0; i < uboBuffers.size(); i ++) {
            uboBuffers[i] = std::make_unique<Buffer>(
                m_Device,
                sizeof(GlobalUbo),
                1,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                m_Device.properties.limits.minUniformBufferOffsetAlignment
            );
            uboBuffers[i]->map();
        };

        auto globalSetLayout = DescriptorSetLayout::Builder(m_Device)
            .addBinding(
                0,
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                    VK_SHADER_STAGE_MISS_BIT_KHR |
                    VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
            )
            .build();

        std::vector<VkDescriptorSet> globalDescriptorSets(SwapChain::MAX_FRAMES_IN_FLIGHT);

        for(int i = 0; i < globalDescriptorSets.size(); i ++) {
            auto bufferInfo = uboBuffers[i]->descriptorInfo();
            if (!DescriptorWriter(*globalSetLayout, *globalPool)
                    .writeBuffer(0, &bufferInfo)
                    .build(globalDescriptorSets[i])) {
                throw std::runtime_error("failed to allocate global descriptor set");
            }
        };

        RayTracingSystem rayTracingSystem{
            m_Device,
            globalSetLayout->getDescriptorSetLayout()
        };

        Camera camera{};

        auto viewerObject = GameObject::createGameObject();
        viewerObject.transform.translation.z = -2.5f;
        KeyboardMovementController cameraController{};

        auto currentTime = std::chrono::high_resolution_clock::now();

        if(!m_Window.shouldClose()) { // Set-up frame
            glfwPollEvents();

            if(auto commandBuffer = m_Renderer.beginFrame()) {
                for(auto &kv : gameObjects) {
                    GameObject &gameObject = kv.second;
                    buildBLAS(gameObject, commandBuffer);
                };
                buildTLAS(commandBuffer);

                m_Renderer.endFrame();
            }
        };

        while(!m_Window.shouldClose()) {
            glfwPollEvents();

            auto newTime = std::chrono::high_resolution_clock::now();
            float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
            currentTime = newTime;

            frameTime = glm::min(frameTime, MAX_FRAME_TIME);

            cameraController.moveInPlaneXZ(m_Window.getGLFWwindow(), frameTime, viewerObject);
            camera.setViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);

            float aspect = m_Renderer.getAspectRatio();
            camera.setPerspectiveProjection(glm::radians(50.f), aspect, .1f, 100.f);

            if(auto commandBuffer = m_Renderer.beginFrame()) {
                int frameIndex = m_Renderer.getFrameIndex();
                FrameInfo frameInfo{
                    frameIndex,
                    frameTime,
                    commandBuffer,
                    camera,
                    globalDescriptorSets[frameIndex],
                    gameObjects,
                    m_Window.getWidth(),
                    m_Window.getHeight()
                };

                rayTracingSystem.renderGameObjects(frameInfo);
                

                m_Renderer.endFrame();
            };
        };
        cleanupAccelerationStructures();
        vkDeviceWaitIdle(m_Device.device());
    };


    void FirstApp::cleanupAccelerationStructures() {
        PFN_vkDestroyAccelerationStructureKHR pfnVkDestroyAccelerationStructureKHR =
            reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(
                vkGetDeviceProcAddr(m_Device.device(), "vkDestroyAccelerationStructureKHR"));

        for (auto &pair : accelerationStructures) {
            AccelerationStructureResources &resources = pair.second;
            
            if (pfnVkDestroyAccelerationStructureKHR && resources.handle != VK_NULL_HANDLE) {
                pfnVkDestroyAccelerationStructureKHR(m_Device.device(), resources.handle, nullptr);
            }
            
            if (resources.buffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(m_Device.device(), resources.buffer, nullptr);
            }
            if (resources.memory != VK_NULL_HANDLE) {
                vkFreeMemory(m_Device.device(), resources.memory, nullptr);
            }
            
            if (resources.scratchBuffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(m_Device.device(), resources.scratchBuffer, nullptr);
            }
            if (resources.scratchMemory != VK_NULL_HANDLE) {
                vkFreeMemory(m_Device.device(), resources.scratchMemory, nullptr);
            }
        }
        accelerationStructures.clear();

        if (pfnVkDestroyAccelerationStructureKHR && m_TlasResources.handle != VK_NULL_HANDLE) {
            pfnVkDestroyAccelerationStructureKHR(m_Device.device(), m_TlasResources.handle, nullptr);
        }
            
        if (m_TlasResources.buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_Device.device(), m_TlasResources.buffer, nullptr);
        }
        if (m_TlasResources.memory != VK_NULL_HANDLE) {
            vkFreeMemory(m_Device.device(), m_TlasResources.memory, nullptr);
        }
            
        if (m_TlasResources.scratchBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_Device.device(), m_TlasResources.scratchBuffer, nullptr);
        }
        if (m_TlasResources.scratchMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_Device.device(), m_TlasResources.scratchMemory, nullptr);
        }
    };

    void FirstApp::buildBLAS(GameObject &gameObject, VkCommandBuffer commandBuffer) {
        // Early exit if model doesn't have index data
        if (!gameObject.model || !gameObject.model->hasIndexBufferData() || gameObject.model->getIndexCount() == 0) {
            return;
        }

        // Load extension functions
        PFN_vkGetAccelerationStructureBuildSizesKHR pfnVkGetAccelerationStructureBuildSizesKHR = 
            reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
                vkGetDeviceProcAddr(m_Device.device(), "vkGetAccelerationStructureBuildSizesKHR"));
        PFN_vkCreateAccelerationStructureKHR pfnVkCreateAccelerationStructureKHR = 
            reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(
                vkGetDeviceProcAddr(m_Device.device(), "vkCreateAccelerationStructureKHR"));
        PFN_vkGetBufferDeviceAddress pfnVkGetBufferDeviceAddress = 
            reinterpret_cast<PFN_vkGetBufferDeviceAddress>(
                vkGetDeviceProcAddr(m_Device.device(), "vkGetBufferDeviceAddress"));
        PFN_vkCmdBuildAccelerationStructuresKHR pfnVkCmdBuildAccelerationStructuresKHR = 
            reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(
                vkGetDeviceProcAddr(m_Device.device(), "vkCmdBuildAccelerationStructuresKHR"));
        
        VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
        triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        triangles.vertexData.deviceAddress = gameObject.model->getVertexBufferDeviceAddress();
        triangles.vertexStride = sizeof(Model::Vertex);
        triangles.maxVertex = gameObject.model->getVertexCount();
        triangles.indexType = VK_INDEX_TYPE_UINT32;
        triangles.indexData.deviceAddress = gameObject.model->getIndexBufferDeviceAddress();

        VkAccelerationStructureGeometryKHR geometry{};
        geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        geometry.geometry.triangles = triangles;
        geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

        VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
        buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        buildInfo.geometryCount = 1;
        buildInfo.pGeometries = &geometry;

        uint32_t primitiveCount = gameObject.model->getIndexCount() / 3;  // triangles = indices / 3

        VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
        sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

        pfnVkGetAccelerationStructureBuildSizesKHR(
            m_Device.device(),
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &buildInfo,
            &primitiveCount,
            &sizeInfo
        );

        // Create AS buffer using the existing createBuffer helper
        VkBuffer asBuffer{};
        VkDeviceMemory asBufferMemory{};
        m_Device.createBuffer(
            sizeInfo.accelerationStructureSize,
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            asBuffer,
            asBufferMemory
        );

        // Create scratch buffer
        VkBuffer scratchBuffer{};
        VkDeviceMemory scratchBufferMemory{};
        m_Device.createBuffer(
            sizeInfo.buildScratchSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            scratchBuffer,
            scratchBufferMemory
        );

        VkAccelerationStructureCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        createInfo.buffer = asBuffer;
        createInfo.offset = 0;
        createInfo.size = sizeInfo.accelerationStructureSize;
        createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

        VkAccelerationStructureKHR blasHandle;
        if (pfnVkCreateAccelerationStructureKHR(m_Device.device(), &createInfo, nullptr, &blasHandle) != VK_SUCCESS) {
            vkDestroyBuffer(m_Device.device(), asBuffer, nullptr);
            vkFreeMemory(m_Device.device(), asBufferMemory, nullptr);
            vkDestroyBuffer(m_Device.device(), scratchBuffer, nullptr);
            vkFreeMemory(m_Device.device(), scratchBufferMemory, nullptr);
            std::runtime_error("Couldn't load extentions for ray tracing");
            return;
        }

        VkBufferDeviceAddressInfo scratchAddressInfo{};
        scratchAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        scratchAddressInfo.buffer = scratchBuffer;
        VkDeviceAddress scratchAddress = pfnVkGetBufferDeviceAddress(m_Device.device(), &scratchAddressInfo);

        buildInfo.dstAccelerationStructure = blasHandle;
        buildInfo.scratchData.deviceAddress = scratchAddress;

        VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
        rangeInfo.primitiveCount = primitiveCount;
        rangeInfo.primitiveOffset = 0;
        rangeInfo.firstVertex = 0;
        rangeInfo.transformOffset = 0;

        const VkAccelerationStructureBuildRangeInfoKHR *pRangeInfos[] = { &rangeInfo };

        pfnVkCmdBuildAccelerationStructuresKHR(
            commandBuffer,
            1,
            &buildInfo,
            pRangeInfos
        );

        accelerationStructures.emplace(gameObject.getId(), AccelerationStructureResources{blasHandle, asBuffer, asBufferMemory, scratchBuffer, scratchBufferMemory});
    };

    void FirstApp::buildTLAS(VkCommandBuffer commandBuffer) {
        PFN_vkGetAccelerationStructureBuildSizesKHR pfnVkGetAccelerationStructureBuildSizesKHR = 
            reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
                vkGetDeviceProcAddr(m_Device.device(), "vkGetAccelerationStructureBuildSizesKHR"));
        PFN_vkCreateAccelerationStructureKHR pfnVkCreateAccelerationStructureKHR = 
            reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(
                vkGetDeviceProcAddr(m_Device.device(), "vkCreateAccelerationStructureKHR"));
        PFN_vkGetBufferDeviceAddress pfnVkGetBufferDeviceAddress = 
            reinterpret_cast<PFN_vkGetBufferDeviceAddress>(
                vkGetDeviceProcAddr(m_Device.device(), "vkGetBufferDeviceAddress"));
        PFN_vkCmdBuildAccelerationStructuresKHR pfnVkCmdBuildAccelerationStructuresKHR = 
            reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(
                vkGetDeviceProcAddr(m_Device.device(), "vkCmdBuildAccelerationStructuresKHR"));
        PFN_vkGetAccelerationStructureDeviceAddressKHR pfnVkGetAccelerationStructureDeviceAddressKHR = 
            reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(
                vkGetDeviceProcAddr(m_Device.device(), "vkGetAccelerationStructureDeviceAddressKHR"));

        std::vector<VkAccelerationStructureInstanceKHR> instances{gameObjects.size()};

        for(auto &accelerationStructure : accelerationStructures) {
            VkAccelerationStructureDeviceAddressInfoKHR blasAddressInfo{};
            blasAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
            blasAddressInfo.accelerationStructure = accelerationStructure.second.handle;

            VkDeviceAddress blasAddress = pfnVkGetAccelerationStructureDeviceAddressKHR(m_Device.device(), &blasAddressInfo);

        

        glm::mat4 m = gameObjects.at(accelerationStructure.first).transform.mat4();

        VkTransformMatrixKHR transform{};
        transform.matrix[0][0] = m[0][0];
        transform.matrix[0][1] = m[1][0];
        transform.matrix[0][2] = m[2][0];
        transform.matrix[0][3] = m[3][0];

        transform.matrix[1][0] = m[0][1];
        transform.matrix[1][1] = m[1][1];
        transform.matrix[1][2] = m[2][1];
        transform.matrix[1][3] = m[3][1];

        transform.matrix[2][0] = m[0][2];
        transform.matrix[2][1] = m[1][2];
        transform.matrix[2][2] = m[2][2];
        transform.matrix[2][3] = m[3][2];
        instances[0].transform = transform;
        instances[0].mask = 0xFF;
        instances[0].instanceShaderBindingTableRecordOffset = 0;
        instances[0].flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        instances[0].accelerationStructureReference = blasAddress;
        }

        VkDeviceSize instanceBufferSize = sizeof(VkAccelerationStructureInstanceKHR) * instances.size();

        VkBuffer instanceBuffer;
        VkDeviceMemory instanceBufferMemory;

        m_Device.createBuffer(
            instanceBufferSize,
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            instanceBuffer,
            instanceBufferMemory
        );

        void* data;
        vkMapMemory(m_Device.device(), instanceBufferMemory, 0, instanceBufferSize, 0, &data);
        memcpy(data, instances.data(), instanceBufferSize);
        vkUnmapMemory(m_Device.device(), instanceBufferMemory);

        VkBufferDeviceAddressInfo instanceAddressInfo{};
        instanceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        instanceAddressInfo.buffer = instanceBuffer;
        VkDeviceAddress instanceBufferAddress = vkGetBufferDeviceAddress(m_Device.device(), &instanceAddressInfo);

        VkAccelerationStructureGeometryInstancesDataKHR instancesData{};
        instancesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        instancesData.arrayOfPointers = VK_FALSE;
        instancesData.data.deviceAddress = instanceBufferAddress;

        VkAccelerationStructureGeometryKHR tlasGeometry{};
        tlasGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        tlasGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        tlasGeometry.geometry.instances = instancesData;

        VkAccelerationStructureBuildGeometryInfoKHR tlasBuildInfo{};
        tlasBuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        tlasBuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        tlasBuildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        tlasBuildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        tlasBuildInfo.geometryCount = 1;
        tlasBuildInfo.pGeometries = &tlasGeometry;

        uint32_t primitiveCount = static_cast<uint32_t>(instances.size());


        VkAccelerationStructureBuildSizesInfoKHR tlasSizeInfo{};
        tlasSizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

        pfnVkGetAccelerationStructureBuildSizesKHR(
            m_Device.device(),
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &tlasBuildInfo,
            &primitiveCount,
            &tlasSizeInfo
        );
        VkBuffer tlasBuffer{};

        VkMemoryAllocateFlagsInfo tlasAllocFlags{};
        VkDeviceMemory tlasBufferMemory;

        m_Device.createBuffer(
            tlasSizeInfo.accelerationStructureSize,
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            tlasBuffer,
            tlasBufferMemory
        );

        VkBuffer tlasScratchBuffer;
        VkDeviceMemory tlasScratchMemory;

        m_Device.createBuffer(
            tlasSizeInfo.buildScratchSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            tlasScratchBuffer,
            tlasScratchMemory
        );

        VkBufferDeviceAddressInfo tlasScratchAddressInfo{};
        tlasScratchAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        tlasScratchAddressInfo.buffer = tlasScratchBuffer;

        VkDeviceAddress tlasScratchAddress = vkGetBufferDeviceAddress(m_Device.device(), &tlasScratchAddressInfo);

        VkAccelerationStructureCreateInfoKHR tlasCreateInfo{};
        tlasCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        tlasCreateInfo.buffer = tlasBuffer;
        tlasCreateInfo.size = tlasSizeInfo.accelerationStructureSize;
        tlasCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

        VkAccelerationStructureKHR tlasHandle;
        pfnVkCreateAccelerationStructureKHR(m_Device.device(), &tlasCreateInfo, nullptr, &tlasHandle);

        VkMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            0,
            1, &barrier,
            0, nullptr,
            0, nullptr
        );

        tlasBuildInfo.dstAccelerationStructure = tlasHandle;
        tlasBuildInfo.scratchData.deviceAddress = tlasScratchAddress;

        VkAccelerationStructureBuildRangeInfoKHR tlasRangeInfo{};
        tlasRangeInfo.primitiveCount = static_cast<uint32_t>(instances.size());
        tlasRangeInfo.primitiveOffset = 0;
        tlasRangeInfo.firstVertex = 0;
        tlasRangeInfo.transformOffset = 0;

        const VkAccelerationStructureBuildRangeInfoKHR* pTlasRangeInfos[] = { &tlasRangeInfo };
        pfnVkCmdBuildAccelerationStructuresKHR(
            commandBuffer,
            1,
            &tlasBuildInfo,
            pTlasRangeInfos
        );
        m_TlasResources = {tlasHandle, tlasBuffer, tlasBufferMemory, tlasScratchBuffer, tlasScratchMemory};
    };
    
    void FirstApp::loadGameObjects() {
        std::shared_ptr<Model> m_Model;
        m_Model = Model::createModelFromFile(m_Device, "models/smooth_vase.obj");  // Test models, editor is for later
        auto smoothVase = GameObject::createGameObject();
        smoothVase.model = m_Model;
        smoothVase.transform.translation = {.5f, .5f, .0f,};
        smoothVase.transform.scale = glm::vec3{3.f};
        gameObjects.emplace(smoothVase.getId(), std::move(smoothVase));

        m_Model = Model::createModelFromFile(m_Device, "models/quad.obj");
        auto floor = GameObject::createGameObject();
        floor.model = m_Model;
        floor.transform.translation = {.0f, .5f, .0f,};
        floor.transform.scale = glm::vec3{3.f, 1.f, 3.f};
        gameObjects.emplace(floor.getId(), std::move(floor));

        auto pointLight = GameObject::makePointLight(.2f);
        pointLight.color = glm::vec4{1.f};
        pointLight.transform.translation = {.0f, -.5f, .0f};
        gameObjects.emplace(pointLight.getId(), std::move(pointLight));
    };
}