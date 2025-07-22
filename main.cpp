#include <vulkan/vulkan.hpp>
#include <iostream>

int main()
{
    // インスタンスの作成
    vk::InstanceCreateInfo createInfo;
    vk::UniqueInstance instance = vk::createInstanceUnique(createInfo);

    // 物理デバイスの取得
    std::vector<vk::PhysicalDevice> physicalDevices = instance->enumeratePhysicalDevices();

    vk::PhysicalDevice physicalDevice;
    bool existsSuitablePhysicalDevice = false;
    uint32_t graphicsQueueFamilyIndex;

    // 描画処理可能なキューを持つ論理デバイスがあるか判定
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

    vk::CommandBufferBeginInfo cmdBeginInfo;
    cmdBufs[0]->begin(cmdBeginInfo);
    // コマンドを記録
    cmdBufs[0]->end();

    // セマフォの作成
    vk::SemaphoreCreateInfo semaphoreCreateInfo;
    vk::UniqueSemaphore semaphore = device->createSemaphoreUnique(semaphoreCreateInfo);
    vk::Semaphore signalSemaphores[] = { semaphore.get() };
    vk::Semaphore waitSemaphores[] = { semaphore.get() };
    vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eAllCommands };

    vk::CommandBuffer submitCmdBuf[1] = { cmdBufs[0].get() };
    vk::SubmitInfo submitInfo;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = submitCmdBuf;

    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    // submitInfo.waitSemaphoreCount = 1;
    // submitInfo.pWaitSemaphores = waitSemaphores;
    // submitInfo.pWaitDstStageMask = waitStages;

    // フェンスの作成
    vk::FenceCreateInfo fenceCreateInfo;
    fenceCreateInfo.flags = vk::FenceCreateFlagBits::eSignaled; // 初期状態をシグナルに設定
    vk::UniqueFence fence = device->createFenceUnique(fenceCreateInfo);

    // GPUに命令を送信
    graphicsQueue.submit({ submitInfo }, fence.get());

    device->waitForFences({ fence.get() }, VK_TRUE, 1'000'000'000);
    device->resetFences({ fence.get() });
    graphicsQueue.waitIdle();

    return 0;
}