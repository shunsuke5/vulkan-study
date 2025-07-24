#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#include <iostream>
#include <filesystem>
#include <fstream>

const uint32_t SCREEN_WIDTH = 640;
const uint32_t SCREEN_HEIGHT = 480;

int main()
{
    // GLFWの初期化処理
    if (!glfwInit()) return -1;

    // 必要な拡張機能の情報を取得
    uint32_t requiredExtensionsCount;
    const char** requiredExtensions = glfwGetRequiredInstanceExtensions(&requiredExtensionsCount);

    // 拡張機能を有効化
    vk::InstanceCreateInfo createInfo;
    createInfo.enabledExtensionCount = requiredExtensionsCount;
    createInfo.ppEnabledExtensionNames = requiredExtensions;

    std::cout << "Extensions:" << std::endl;
    for (int i = 0; i < requiredExtensionsCount; ++i) {
        std::cout << requiredExtensions[i] << std::endl;
    }

    vk::UniqueInstance instance;
    instance = vk::createInstanceUnique(createInfo);

    // ウィンドウの作成
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window;
    window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "GLFW Test Window", NULL, NULL);
    if (!window) {
        const char* err;
        glfwGetError(&err);
        std::cout << err << std::endl;
        glfwTerminate();
        return -1;
    }

    // サーフェイスの作成
    VkSurfaceKHR c_surface;
    VkResult result = glfwCreateWindowSurface(instance.get(), window, nullptr, &c_surface);
    if (result != VK_SUCCESS) {
        const char* err;
        glfwGetError(&err);
        std::cout << err << std::endl;
        glfwTerminate();
        return -1;
    }

    vk::UniqueSurfaceKHR surface(c_surface, instance.get());

    // 物理デバイスの選定
    std::vector<vk::PhysicalDevice> physicalDevices = instance->enumeratePhysicalDevices();

    vk::PhysicalDevice physicalDevice;
    bool existSuitablePhysicalDevice = false;
    uint32_t graphicsQueueFamilyIndex;

    for (size_t i = 0; i < physicalDevices.size(); ++i) {
        // グラフィックス機能、サーフェイスへのプレゼンテーションをサポートしているキューを厳選
        std::vector<vk::QueueFamilyProperties> queueProps = physicalDevices[i].getQueueFamilyProperties();
        bool existsGraphicsQueue = false;

        for (size_t j = 0; j < queueProps.size(); ++j) {
            if (queueProps[j].queueFlags & vk::QueueFlagBits::eGraphics &&
                physicalDevices[i].getSurfaceSupportKHR(j, surface.get()))
            {
                existsGraphicsQueue = true;
                graphicsQueueFamilyIndex = j;
                break;
            }
        }

        // スワップチェーンの拡張機能をサポートしているか確認
        std::vector<vk::ExtensionProperties> extProps = physicalDevices[i].enumerateDeviceExtensionProperties();
        bool supportsSwapchainExtension = false;

        for (size_t j = 0; j < extProps.size(); ++j) {
            if (std::string_view(extProps[j].extensionName.data()) == VK_KHR_SWAPCHAIN_EXTENSION_NAME) {
                supportsSwapchainExtension = true;
                break;
            }
        }

        // サーフェイスと相性の悪いGPUを弾く
        bool supportsSurface =
            !physicalDevices[i].getSurfaceFormatsKHR(surface.get()).empty() ||
            !physicalDevices[i].getSurfacePresentModesKHR(surface.get()).empty();

        if (existsGraphicsQueue && supportsSwapchainExtension && supportsSurface) {
            physicalDevice = physicalDevices[i];
            existSuitablePhysicalDevice = true;
            break;
        }
    }

    if (!existSuitablePhysicalDevice) {
        std::cerr << "使用可能な物理デバイスがありません。" << std::endl;
        return -1;
    }

    // 論理デバイスの作成
    vk::DeviceCreateInfo devCreateInfo;

    vk::DeviceQueueCreateInfo queueCreateInfo[1];
    queueCreateInfo[0].queueFamilyIndex = graphicsQueueFamilyIndex;
    queueCreateInfo[0].queueCount = 1;
    float queuePriorities[1] = { 1.0 };
    queueCreateInfo[0].pQueuePriorities = queuePriorities;

    devCreateInfo.pQueueCreateInfos = queueCreateInfo;
    devCreateInfo.queueCreateInfoCount = 1;

    // スワップチェーン機能の有効化
    auto devRequiredExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    devCreateInfo.enabledExtensionCount = devRequiredExtensions.size();
    devCreateInfo.ppEnabledExtensionNames = devRequiredExtensions.begin();

    vk::UniqueDevice device = physicalDevice.createDeviceUnique(devCreateInfo);
    vk::Queue graphicsQueue = device->getQueue(graphicsQueueFamilyIndex, 0);

    // スワップチェーンの作成
    vk::SurfaceCapabilitiesKHR surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface.get());
    std::vector<vk::SurfaceFormatKHR> surfaceFormats = physicalDevice.getSurfaceFormatsKHR(surface.get());
    std::vector<vk::PresentModeKHR> surfacePresentModes = physicalDevice.getSurfacePresentModesKHR(surface.get());

    vk::SurfaceFormatKHR swapchainFormat = surfaceFormats[0];
    vk::PresentModeKHR swapchainPresentMode = surfacePresentModes[0];

    vk::SwapchainCreateInfoKHR swapchainCreateInfo;
    swapchainCreateInfo.surface = surface.get();
    swapchainCreateInfo.minImageCount = surfaceCapabilities.minImageCount + 1;
    swapchainCreateInfo.imageFormat = swapchainFormat.format;
    swapchainCreateInfo.imageColorSpace = swapchainFormat.colorSpace;
    swapchainCreateInfo.imageExtent = surfaceCapabilities.currentExtent;
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
    swapchainCreateInfo.imageSharingMode = vk::SharingMode::eExclusive;
    swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
    swapchainCreateInfo.presentMode = swapchainPresentMode;
    swapchainCreateInfo.clipped = VK_TRUE;

    vk::UniqueSwapchainKHR swapchain = device->createSwapchainKHRUnique(swapchainCreateInfo);

    // スワップチェーンのイメージを取得
    std::vector<vk::Image> swapchainImages = device->getSwapchainImagesKHR(swapchain.get());

    // アタッチメントの作成
    vk::AttachmentDescription attachments[1];
    attachments[0].format = swapchainFormat.format;
    attachments[0].samples = vk::SampleCountFlagBits::e1;
    attachments[0].loadOp = vk::AttachmentLoadOp::eClear;
    attachments[0].storeOp = vk::AttachmentStoreOp::eStore;
    attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    attachments[0].initialLayout = vk::ImageLayout::eUndefined;
    attachments[0].finalLayout = vk::ImageLayout::ePresentSrcKHR;

    // サブパス依存の作成
    vk::AttachmentReference subpass0_attachmentRefs[1];
    subpass0_attachmentRefs[0].attachment = 0;
    subpass0_attachmentRefs[0].layout = vk::ImageLayout::eColorAttachmentOptimal;

    // サブパスの作成
    vk::SubpassDescription subpasses[1];
    subpasses[0].pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpasses[0].colorAttachmentCount = 1;
    subpasses[0].pColorAttachments = subpass0_attachmentRefs;

    // レンダーパスの作成
    vk::RenderPassCreateInfo renderpassCreateInfo;
    renderpassCreateInfo.attachmentCount = 1;
    renderpassCreateInfo.pAttachments = attachments;
    renderpassCreateInfo.subpassCount = 1;
    renderpassCreateInfo.pSubpasses = subpasses;
    renderpassCreateInfo.dependencyCount = 0;
    renderpassCreateInfo.pDependencies = nullptr;

    vk::UniqueRenderPass renderpass = device->createRenderPassUnique(renderpassCreateInfo);

    // パイプラインの作成
    vk::Viewport viewports[1];
    viewports[0].x = 0.0;
    viewports[0].y = 0.0;
    viewports[0].minDepth = 0.0;
    viewports[0].maxDepth = 1.0;
    viewports[0].width = SCREEN_WIDTH;
    viewports[0].height = SCREEN_HEIGHT;

    vk::Rect2D scissors[1];
    scissors[0].offset.x = 0;
    scissors[0].offset.y = 0;
    scissors[0].extent.width = SCREEN_WIDTH;
    scissors[0].extent.height = SCREEN_HEIGHT;

    vk::PipelineViewportStateCreateInfo viewportState;
    viewportState.viewportCount = 1;
    viewportState.pViewports = viewports;
    viewportState.scissorCount = 1;
    viewportState.pScissors = scissors;

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexAttributeDescriptions = nullptr;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.pVertexBindingDescriptions = nullptr;

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
    inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
    inputAssembly.primitiveRestartEnable = false;

    vk::PipelineRasterizationStateCreateInfo rasterizer;
    rasterizer.depthClampEnable = false;
    rasterizer.rasterizerDiscardEnable = false;
    rasterizer.polygonMode = vk::PolygonMode::eFill;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = vk::CullModeFlagBits::eBack;
    rasterizer.frontFace = vk::FrontFace::eClockwise;
    rasterizer.depthBiasEnable = false;

    vk::PipelineMultisampleStateCreateInfo multisample;
    multisample.sampleShadingEnable = false;
    multisample.rasterizationSamples = vk::SampleCountFlagBits::e1;

    vk::PipelineColorBlendAttachmentState blendattachment[1];
    blendattachment[0].colorWriteMask =
        vk::ColorComponentFlagBits::eA |
        vk::ColorComponentFlagBits::eR |
        vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB;
    blendattachment[0].blendEnable = false;

    vk::PipelineColorBlendStateCreateInfo blend;
    blend.logicOpEnable = false;
    blend.attachmentCount = 1;
    blend.pAttachments = blendattachment;

    vk::PipelineLayoutCreateInfo layoutCreateInfo;
    layoutCreateInfo.setLayoutCount = 0;
    layoutCreateInfo.pSetLayouts = nullptr;

    vk::UniquePipelineLayout pipelineLayout = device->createPipelineLayoutUnique(layoutCreateInfo);

    // 頂点シェーダファイルの読み込み
    size_t vertSpvFileSz = std::filesystem::file_size("../shader.vert.spv");
    std::ifstream vertSpvFile("../shader.vert.spv", std::ios_base::binary);
    std::vector<char> vertSpvFileData(vertSpvFileSz);
    vertSpvFile.read(vertSpvFileData.data(), vertSpvFileSz);

    // 頂点シェーダモジュールの作成
    vk::ShaderModuleCreateInfo vertShaderCreateInfo;
    vertShaderCreateInfo.codeSize = vertSpvFileSz;
    vertShaderCreateInfo.pCode = reinterpret_cast<const uint32_t*>(vertSpvFileData.data());

    vk::UniqueShaderModule vertShader = device->createShaderModuleUnique(vertShaderCreateInfo);

    // フラグメントシェーダファイルの読み込み
    size_t fragSpvFileSz = std::filesystem::file_size("../shader.frag.spv");
    std::ifstream fragSpvFile("../shader.frag.spv", std::ios_base::binary);
    std::vector<char> fragspvfileData(fragSpvFileSz);
    fragSpvFile.read(fragspvfileData.data(), fragSpvFileSz);

    // フラグメントシェーダモジュールの作成
    vk::ShaderModuleCreateInfo fragShaderCreateInfo;
    fragShaderCreateInfo.codeSize = fragSpvFileSz;
    fragShaderCreateInfo.pCode = reinterpret_cast<const uint32_t*>(fragspvfileData.data());

    vk::UniqueShaderModule fragShader = device->createShaderModuleUnique(fragShaderCreateInfo);

    vk::PipelineShaderStageCreateInfo shaderStage[2];
    shaderStage[0].stage = vk::ShaderStageFlagBits::eVertex;
    shaderStage[0].module = vertShader.get();
    shaderStage[0].pName = "main";
    shaderStage[1].stage = vk::ShaderStageFlagBits::eFragment;
    shaderStage[1].module = fragShader.get();
    shaderStage[1].pName = "main";

    vk::GraphicsPipelineCreateInfo pipelineCreateInfo;
    pipelineCreateInfo.pViewportState = &viewportState;
    pipelineCreateInfo.pVertexInputState = &vertexInputInfo;
    pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
    pipelineCreateInfo.pRasterizationState = &rasterizer;
    pipelineCreateInfo.pMultisampleState = &multisample;
    pipelineCreateInfo.pColorBlendState = &blend;
    pipelineCreateInfo.layout = pipelineLayout.get();
    pipelineCreateInfo.stageCount = 2;
    pipelineCreateInfo.pStages = shaderStage;
    pipelineCreateInfo.renderPass= renderpass.get();
    pipelineCreateInfo.subpass = 0;

    vk::UniquePipeline pipeline = device->createGraphicsPipelineUnique(nullptr, pipelineCreateInfo).value;

    // イメージビューの作成
    std::vector<vk::UniqueImageView> swapchainImageViews(swapchainImages.size());

    for (size_t i = 0; i < swapchainImages.size(); ++i) {
        vk::ImageViewCreateInfo imgViewCreateInfo;
        imgViewCreateInfo.image = swapchainImages[i];
        imgViewCreateInfo.viewType = vk::ImageViewType::e2D;
        imgViewCreateInfo.format = swapchainFormat.format;
        imgViewCreateInfo.components.r = vk::ComponentSwizzle::eIdentity;
        imgViewCreateInfo.components.g = vk::ComponentSwizzle::eIdentity;
        imgViewCreateInfo.components.b = vk::ComponentSwizzle::eIdentity;
        imgViewCreateInfo.components.a = vk::ComponentSwizzle::eIdentity;
        imgViewCreateInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        imgViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imgViewCreateInfo.subresourceRange.levelCount = 1;
        imgViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imgViewCreateInfo.subresourceRange.layerCount = 1;

        swapchainImageViews[i] = device->createImageViewUnique(imgViewCreateInfo);
    }

    // フレームバッファの作成
    std::vector<vk::UniqueFramebuffer> swapchainFramebufs(swapchainImages.size());

    for (size_t i = 0; i < swapchainImages.size(); ++i) {
        vk::ImageView frameBufAttachments[1];
        frameBufAttachments[0] = swapchainImageViews[i].get();

        vk::FramebufferCreateInfo frameBufCreateinfo;
        frameBufCreateinfo.width = surfaceCapabilities.currentExtent.width;
        frameBufCreateinfo.height = surfaceCapabilities.currentExtent.height;
        frameBufCreateinfo.layers = 1;
        frameBufCreateinfo.renderPass = renderpass.get();
        frameBufCreateinfo.attachmentCount = 1;
        frameBufCreateinfo.pAttachments = frameBufAttachments;

        swapchainFramebufs[i] = device->createFramebufferUnique(frameBufCreateinfo);
    }

    // コマンドプールの作成
    vk::CommandPoolCreateInfo cmdPoolCreateInfo;
    cmdPoolCreateInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
    // フレームバッファをリセット可能に設定
    cmdPoolCreateInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

    vk::UniqueCommandPool cmdPool = device->createCommandPoolUnique(cmdPoolCreateInfo);

    // コマンドバッファの作成
    vk::CommandBufferAllocateInfo cmdBufAllocInfo;
    cmdBufAllocInfo.commandPool = cmdPool.get();
    cmdBufAllocInfo.commandBufferCount = 1;
    cmdBufAllocInfo.level = vk::CommandBufferLevel::ePrimary;
    std::vector<vk::UniqueCommandBuffer> cmdBufs =
        device->allocateCommandBuffersUnique(cmdBufAllocInfo);

    // セマフォの作成
    vk::SemaphoreCreateInfo semaphoreCreateInfo;
    vk::UniqueSemaphore swapchainImgSemaphore, imgRenderedSemaphore;
    swapchainImgSemaphore = device->createSemaphoreUnique(semaphoreCreateInfo);
    imgRenderedSemaphore = device->createSemaphoreUnique(semaphoreCreateInfo);

    // フェンスの作成
    vk::FenceCreateInfo fenceCreateInfo;
    // 最初の1回は前フレームが無いため、初期状態をシグナル状態にする
    fenceCreateInfo.flags = vk::FenceCreateFlagBits::eSignaled;
    vk::UniqueFence imgRenderedFence = device->createFenceUnique(fenceCreateInfo);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // 前のレンダリング終了を待機
        device->waitForFences({ imgRenderedFence.get()}, VK_TRUE, UINT64_MAX);
        device->resetFences({ imgRenderedFence.get() });

        vk::ResultValue acquireImgResult = device->acquireNextImageKHR(swapchain.get(), 1'000'000'000, swapchainImgSemaphore.get());
        if (acquireImgResult.result != vk::Result::eSuccess) {
            std::cerr << "次フレームの取得に失敗しました。" << std::endl;
            return -1;
        }

        uint32_t imgIndex = acquireImgResult.value;

        // コマンドバッファのリセット
        cmdBufs[0]->reset();

        // コマンドバッファへのコマンドの記録
        vk::CommandBufferBeginInfo cmdBefinInfo;
        cmdBufs[0]->begin(cmdBefinInfo);

        vk::ClearValue clearVal[1];
        clearVal[0].color.float32[0] = 0.0f;
        clearVal[0].color.float32[1] = 0.0f;
        clearVal[0].color.float32[2] = 0.0f;
        clearVal[0].color.float32[3] = 1.0f;

        vk::RenderPassBeginInfo renderpassBeginInfo;
        renderpassBeginInfo.renderPass = renderpass.get();
        renderpassBeginInfo.framebuffer = swapchainFramebufs[imgIndex].get();
        renderpassBeginInfo.renderArea = vk::Rect2D({0,0}, { SCREEN_WIDTH, SCREEN_HEIGHT });
        renderpassBeginInfo.clearValueCount = 1;
        renderpassBeginInfo.pClearValues = clearVal;

        cmdBufs[0]->beginRenderPass(renderpassBeginInfo, vk::SubpassContents::eInline);
        cmdBufs[0]->bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.get());
        cmdBufs[0]->draw(3,1,0,0);
        cmdBufs[0]->endRenderPass();
        cmdBufs[0]->end();

        // コマンドバッファの送信
        vk::CommandBuffer submitCmdBuf[1] = { cmdBufs[0].get() };
        vk::SubmitInfo submitInfo;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = submitCmdBuf;

        // 待機するセマフォの指定
        vk::Semaphore renderwaitSemaphores[] = { swapchainImgSemaphore.get() };
        vk::PipelineStageFlags renderwaitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = renderwaitSemaphores;
        submitInfo.pWaitDstStageMask = renderwaitStages;

        // 完了時にシグナル状態にするセマフォの指定
        vk::Semaphore renderSignalSemaphores[] = { imgRenderedSemaphore.get() };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = renderSignalSemaphores;

        graphicsQueue.submit({ submitInfo }, imgRenderedFence.get());
        graphicsQueue.waitIdle();

        // プレゼンテーションを実行
        vk::PresentInfoKHR presentInfo;
        auto presentSwapchains = { swapchain.get() };
        auto imgIndices = { imgIndex };

        presentInfo.swapchainCount = presentSwapchains.size();
        presentInfo.pSwapchains = presentSwapchains.begin();
        presentInfo.pImageIndices = imgIndices.begin();

        // 待機するセマフォの指定
        vk::Semaphore presentWaitSemaphores[] = { imgRenderedSemaphore.get() };
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = presentWaitSemaphores;

        graphicsQueue.presentKHR(presentInfo);
    }

    graphicsQueue.waitIdle();
    glfwTerminate();
    return 0;
}