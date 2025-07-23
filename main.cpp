#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <vulkan/vulkan.hpp>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>

const uint32_t SCREEN_WIDTH = 640;
const uint32_t SCREEN_HEIGHT = 480;

int main()
{
    // インスタンスの作成
    vk::InstanceCreateInfo createInfo;
    vk::UniqueInstance instance = vk::createInstanceUnique(createInfo);

    // 物理デバイスの取得
    std::vector<vk::PhysicalDevice> physicalDevices = instance->enumeratePhysicalDevices();

    // 描画処理可能なキューを持つ論理デバイスがあるか判定
    vk::PhysicalDevice physicalDevice;
    bool existsSuitablePhysicalDevice = false;
    uint32_t graphicsQueueFamilyIndex;

    for (size_t i = 0; i < physicalDevices.size(); ++i) {
        std::vector<vk::QueueFamilyProperties> queueProps = physicalDevices[i].getQueueFamilyProperties();
        bool existsGraphicsQueue = false;

        for (size_t j = 0; j < queueProps.size(); ++j) {
            if (queueProps[j].queueFlags & vk::QueueFlagBits::eGraphics) {
                existsGraphicsQueue = true;
                graphicsQueueFamilyIndex = j;
                break;
            }
        }

        if (existsGraphicsQueue) {
            physicalDevice = physicalDevices[i];
            existsSuitablePhysicalDevice = true;
            break;
        }
    }

    if (!existsSuitablePhysicalDevice) {
        std::cerr << "使用可能な物理デバイスがありません。" << std::endl;
        return -1;
    }

    vk::DeviceCreateInfo devCreateInfo;

    // 論理デバイスに対するキューの情報を設定
    vk::DeviceQueueCreateInfo queueCreateInfo[1];
    queueCreateInfo[0].queueFamilyIndex = graphicsQueueFamilyIndex;
    queueCreateInfo[0].queueCount = 1;
    float queuePriorities[1] = { 1.0f };
    queueCreateInfo[0].pQueuePriorities = queuePriorities;

    devCreateInfo.pQueueCreateInfos = queueCreateInfo;
    devCreateInfo.queueCreateInfoCount = 1;

    // 論理デバイスの作成
    vk::UniqueDevice device = physicalDevice.createDeviceUnique(devCreateInfo);
    vk::Queue graphicsQueue = device->getQueue(graphicsQueueFamilyIndex, 0);

    // コマンドプールの作成
    vk::CommandPoolCreateInfo cmdPoolCreateInfo;
    cmdPoolCreateInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
    vk::UniqueCommandPool cmdPool = device->createCommandPoolUnique(cmdPoolCreateInfo);

    // コマンドバッファの作成
    vk::CommandBufferAllocateInfo cmdBufAllocInfo;
    cmdBufAllocInfo.commandPool = cmdPool.get();
    cmdBufAllocInfo.commandBufferCount = 1;
    cmdBufAllocInfo.level = vk::CommandBufferLevel::ePrimary;
    std::vector<vk::UniqueCommandBuffer> cmdBufs = device->allocateCommandBuffersUnique(cmdBufAllocInfo);

    // イメージの作成
    vk::ImageCreateInfo imgCreateInfo;
    imgCreateInfo.imageType = vk::ImageType::e2D;
    imgCreateInfo.extent = vk::Extent3D(SCREEN_WIDTH, SCREEN_HEIGHT, 1);
    imgCreateInfo.mipLevels = 1;
    imgCreateInfo.arrayLayers = 1;
    imgCreateInfo.format = vk::Format::eR8G8B8A8Unorm;
    imgCreateInfo.tiling = vk::ImageTiling::eLinear;
    imgCreateInfo.initialLayout = vk::ImageLayout::eUndefined;
    imgCreateInfo.usage = vk::ImageUsageFlagBits::eColorAttachment;
    imgCreateInfo.sharingMode = vk::SharingMode::eExclusive;
    imgCreateInfo.samples = vk::SampleCountFlagBits::e1;

    vk::UniqueImage image = device->createImageUnique(imgCreateInfo);

    // デバイスメモリの種類を取得
    vk::PhysicalDeviceMemoryProperties memProps = physicalDevice.getMemoryProperties();

    // イメージに対して必要なメモリのサイズや種類などを取得
    vk::MemoryRequirements imgMemReq = device->getImageMemoryRequirements(image.get());

    vk::MemoryAllocateInfo imgMemAllocInfo;
    imgMemAllocInfo.allocationSize = imgMemReq.size;

    // 使用可能なメモリタイプの中で最初に見つかったものを使用する
    bool suitableMemoryTypeFound = false;
    for (size_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if (imgMemReq.memoryTypeBits & (1 << i) &&
            (memProps.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible))
        {
            imgMemAllocInfo.memoryTypeIndex = i;
            suitableMemoryTypeFound = true;
            break;
        }
    }

    if (!suitableMemoryTypeFound) {
        std::cerr << "使用可能なメモリタイプがありません" << std::endl;
        return -1;
    }

    vk::UniqueDeviceMemory imgMem = device->allocateMemoryUnique(imgMemAllocInfo);
    device->bindImageMemory(image.get(), imgMem.get(), 0);

    //アタッチメントの作成
    vk::AttachmentDescription attachments[1];
    attachments[0].format = vk::Format::eR8G8B8A8Unorm;
    attachments[0].samples = vk::SampleCountFlagBits::e1;
    attachments[0].loadOp = vk::AttachmentLoadOp::eClear;
    attachments[0].storeOp = vk::AttachmentStoreOp::eStore;
    attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    attachments[0].initialLayout = vk::ImageLayout::eUndefined;
    attachments[0].finalLayout = vk::ImageLayout::eGeneral;

    // サブパス依存関係の作成
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
    vk::ImageViewCreateInfo imgViewCreateInfo;
    imgViewCreateInfo.image = image.get();
    imgViewCreateInfo.viewType = vk::ImageViewType::e2D;
    imgViewCreateInfo.format = vk::Format::eR8G8B8A8Unorm;
    imgViewCreateInfo.components.r = vk::ComponentSwizzle::eIdentity;
    imgViewCreateInfo.components.g = vk::ComponentSwizzle::eIdentity;
    imgViewCreateInfo.components.b = vk::ComponentSwizzle::eIdentity;
    imgViewCreateInfo.components.a = vk::ComponentSwizzle::eIdentity;
    imgViewCreateInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    imgViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imgViewCreateInfo.subresourceRange.levelCount = 1;
    imgViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imgViewCreateInfo.subresourceRange.layerCount = 1;

    vk::UniqueImageView imgView = device->createImageViewUnique(imgViewCreateInfo);

   // フレームバッファの作成
   vk::ImageView frameBufAttachments[1];
   frameBufAttachments[0] = imgView.get();

   vk::FramebufferCreateInfo frameBufCreateInfo;
   frameBufCreateInfo.width = SCREEN_WIDTH;
   frameBufCreateInfo.height = SCREEN_HEIGHT;
   frameBufCreateInfo.layers = 1;
   frameBufCreateInfo.renderPass = renderpass.get();
   frameBufCreateInfo.attachmentCount = 1;
   frameBufCreateInfo.pAttachments = frameBufAttachments;

   vk::UniqueFramebuffer frameBuf = device->createFramebufferUnique(frameBufCreateInfo);

    // レンダーパスの開始
    vk::CommandBufferBeginInfo cmdBeginInfo;
    cmdBufs[0]->begin(cmdBeginInfo);

    // 特定の色でクリアする
    vk::ClearValue clearVal[1];
    clearVal[0].color.float32[0] = 0.0f;
    clearVal[0].color.float32[1] = 0.0f;
    clearVal[0].color.float32[2] = 0.0f;
    clearVal[0].color.float32[3] = 1.0f;

    vk::RenderPassBeginInfo renderpassBeginInfo;
    renderpassBeginInfo.renderPass = renderpass.get();
    renderpassBeginInfo.framebuffer = frameBuf.get();
    renderpassBeginInfo.renderArea = vk::Rect2D({ 0,0 }, { SCREEN_WIDTH, SCREEN_HEIGHT });
    renderpassBeginInfo.clearValueCount = 1;
    renderpassBeginInfo.pClearValues = clearVal;

    cmdBufs[0]->beginRenderPass(renderpassBeginInfo, vk::SubpassContents::eInline);
    // ここでサブパス0番の処理
    // どのパイプラインを使うか示す
    cmdBufs[0]->bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.get());
    cmdBufs[0]->draw(3, 1, 0, 0); // 描画処理
    cmdBufs[0]->endRenderPass();

    cmdBufs[0]->end();

    vk::CommandBuffer submitCmdBuf[1] = { cmdBufs[0].get() };
    vk::SubmitInfo submitInfo;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = submitCmdBuf;

    // GPUに命令を送信
    graphicsQueue.submit({ submitInfo }, nullptr);
    graphicsQueue.waitIdle();

    // ファイルに書き出す処理
    void* imgData = device->mapMemory(imgMem.get(), 0, imgMemReq.size);
    stbi_write_bmp("img.bmp", SCREEN_WIDTH, SCREEN_HEIGHT, 4, imgData);
    device->unmapMemory(imgMem.get());

    return 0;
}