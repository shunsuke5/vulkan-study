#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#include <iostream>

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

    // サーフェスの作成
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

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}