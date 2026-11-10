#pragma once

#include <cstdint>
#include <string>

#include <vulkan/vulkan_core.h>

#include <vk_mem_alloc.h>

struct GLFWwindow;

namespace graphics::internal {

struct Context {
	uint32_t validation_errors = 0;
	VkPhysicalDevice physical_device;
	VkDevice device;

	VmaAllocator allocator;

	VkQueue graphics_queue;
	uint32_t graphics_queue_index;

	VkFormat swapchain_format;
	VkExtent2D swapchain_extent;

	VkRenderPass render_pass;
};

struct FrameData {
	VkFramebuffer framebuffer;
	VkCommandBuffer command_buffer;
};

extern Context context;

bool initialize(GLFWwindow* const window);
void shutdown();

void resize(uint32_t width, uint32_t height);

FrameData prepare();
void submitAndPresent();
void requestCapture(const std::string& path);

} // namespace graphics::internal
