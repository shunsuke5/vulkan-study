#include <vulkan/vulkan.hpp>
#include <iostream>

int main()
{
    vk::InstanceCreateInfo createInfo;
    vk::UniqueInstance instance = vk::createInstanceUnique(createInfo);
}