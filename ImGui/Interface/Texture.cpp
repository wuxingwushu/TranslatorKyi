#include "../Interface.h"

// =====================================================================================
// 纹理相关：Vulkan 内存类型查询、图片上传为纹理、纹理清理
// （MyTextureData / findMemoryType / LoadTextureFromFile / RemoveTexture 的实现）
// =====================================================================================
namespace GAME {

	// Helper function to find Vulkan memory type bits. See ImGui_ImplVulkan_MemoryType() in imgui_impl_vulkan.cpp
	uint32_t ImGuiInterFace::findMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties)
	{
		VkPhysicalDeviceMemoryProperties mem_properties;
		vkGetPhysicalDeviceMemoryProperties(ImGuiVulkanInfo.PhysicalDevice, &mem_properties);

		for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++)
			if ((type_filter & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties)
				return i;

		return 0xFFFFFFFF; // Unable to find memoryType
	}

	// Helper function to load an image with common settings and return a MyTextureData with a VkDescriptorSet as a sort of Vulkan pointer
	bool ImGuiInterFace::LoadTextureFromFile(char* Texturedata, MyTextureData* tex_data)
	{
		if (tex_data->DS != nullptr) {
			RemoveTexture(tex_data);
		}

		TData = Texturedata;

		// Specifying 4 channels forces stb to load the image in RGBA which is an easy format for Vulkan
		tex_data->Channels = 4;
		//尺寸必须用"上一次截图"的那一对：Texturedata 就是 TOOL::screen 交出来的那块缓冲区，
		//按 windows_* 算的话，截图之后用户换了分辨率，这里就会按新尺寸去读旧缓冲区（越界读堆）。
		tex_data->Width = Variable::ScreenShot_Width;
		tex_data->Height = Variable::ScreenShot_Heigth;

		// Calculate allocation size (in number of bytes)
		size_t image_size = (size_t)tex_data->Width * (size_t)tex_data->Height * (size_t)tex_data->Channels;

		VkResult err;

		// Create the Vulkan image.
		{
			VkImageCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			info.imageType = VK_IMAGE_TYPE_2D;
			info.format = VK_FORMAT_R8G8B8A8_UNORM;
			info.extent.width = tex_data->Width;
			info.extent.height = tex_data->Height;
			info.extent.depth = 1;
			info.mipLevels = 1;
			info.arrayLayers = 1;
			info.samples = VK_SAMPLE_COUNT_1_BIT;
			info.tiling = VK_IMAGE_TILING_OPTIMAL;
			info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			err = vkCreateImage(ImGuiVulkanInfo.Device, &info, ImGuiVulkanInfo.Allocator, &tex_data->Image);
			//check_vk_result(err);
			VkMemoryRequirements req;
			vkGetImageMemoryRequirements(ImGuiVulkanInfo.Device, tex_data->Image, &req);
			VkMemoryAllocateInfo alloc_info = {};
			alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			alloc_info.allocationSize = req.size;
			alloc_info.memoryTypeIndex = findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			err = vkAllocateMemory(ImGuiVulkanInfo.Device, &alloc_info, ImGuiVulkanInfo.Allocator, &tex_data->ImageMemory);
			//check_vk_result(err);
			err = vkBindImageMemory(ImGuiVulkanInfo.Device, tex_data->Image, tex_data->ImageMemory, 0);
			//check_vk_result(err);
		}

		// Create the Image View
		{
			VkImageViewCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			info.image = tex_data->Image;
			info.viewType = VK_IMAGE_VIEW_TYPE_2D;
			info.format = VK_FORMAT_B8G8R8A8_UNORM;
			info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			info.subresourceRange.levelCount = 1;
			info.subresourceRange.layerCount = 1;
			err = vkCreateImageView(ImGuiVulkanInfo.Device, &info, ImGuiVulkanInfo.Allocator, &tex_data->ImageView);
			//check_vk_result(err);
		}

		// Create Sampler
		{
			VkSamplerCreateInfo sampler_info{};
			sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			sampler_info.magFilter = VK_FILTER_LINEAR;
			sampler_info.minFilter = VK_FILTER_LINEAR;
			sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT; // outside image bounds just use border color
			sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			sampler_info.minLod = -1000;
			sampler_info.maxLod = 1000;
			sampler_info.maxAnisotropy = 1.0f;
			err = vkCreateSampler(ImGuiVulkanInfo.Device, &sampler_info, ImGuiVulkanInfo.Allocator, &tex_data->Sampler);
			//check_vk_result(err);
		}

		// Create Descriptor Set using ImGUI's implementation
		tex_data->DS = ImGui_ImplVulkan_AddTexture(tex_data->Sampler, tex_data->ImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		// Create Upload Buffer
		{
			VkBufferCreateInfo buffer_info = {};
			buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			buffer_info.size = image_size;
			buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			err = vkCreateBuffer(ImGuiVulkanInfo.Device, &buffer_info, ImGuiVulkanInfo.Allocator, &tex_data->UploadBuffer);
			//check_vk_result(err);
			VkMemoryRequirements req;
			vkGetBufferMemoryRequirements(ImGuiVulkanInfo.Device, tex_data->UploadBuffer, &req);
			VkMemoryAllocateInfo alloc_info = {};
			alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			alloc_info.allocationSize = req.size;
			alloc_info.memoryTypeIndex = findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
			err = vkAllocateMemory(ImGuiVulkanInfo.Device, &alloc_info, ImGuiVulkanInfo.Allocator, &tex_data->UploadBufferMemory);
			//check_vk_result(err);
			err = vkBindBufferMemory(ImGuiVulkanInfo.Device, tex_data->UploadBuffer, tex_data->UploadBufferMemory, 0);
			//check_vk_result(err);
		}

		// Upload to Buffer:
		{
			void* map = NULL;
			err = vkMapMemory(ImGuiVulkanInfo.Device, tex_data->UploadBufferMemory, 0, image_size, 0, &map);
			//check_vk_result(err);
			memcpy(map, Texturedata, image_size);
			VkMappedMemoryRange range[1] = {};
			range[0].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
			range[0].memory = tex_data->UploadBufferMemory;
			range[0].size = image_size;
			err = vkFlushMappedMemoryRanges(ImGuiVulkanInfo.Device, 1, range);
			//check_vk_result(err);
			vkUnmapMemory(ImGuiVulkanInfo.Device, tex_data->UploadBufferMemory);
		}


		// Create a command buffer that will perform following steps when hit in the command queue.
		// TODO: this works in the example, but may need input if this is an acceptable way to access the pool/create the command buffer.
		VkCommandPool command_pool = ImGuiCommandPoolS[0]->getCommandPool();
		VkCommandBuffer command_buffer;
		{
			VkCommandBufferAllocateInfo alloc_info{};
			alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			alloc_info.commandPool = command_pool;
			alloc_info.commandBufferCount = 1;

			err = vkAllocateCommandBuffers(ImGuiVulkanInfo.Device, &alloc_info, &command_buffer);
			//check_vk_result(err);

			VkCommandBufferBeginInfo begin_info = {};
			begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			err = vkBeginCommandBuffer(command_buffer, &begin_info);
			//check_vk_result(err);
		}

		// Copy to Image
		{
			VkImageMemoryBarrier copy_barrier[1] = {};
			copy_barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			copy_barrier[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			copy_barrier[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			copy_barrier[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			copy_barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			copy_barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			copy_barrier[0].image = tex_data->Image;
			copy_barrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copy_barrier[0].subresourceRange.levelCount = 1;
			copy_barrier[0].subresourceRange.layerCount = 1;
			vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, copy_barrier);

			VkBufferImageCopy region = {};
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.layerCount = 1;
			region.imageExtent.width = tex_data->Width;
			region.imageExtent.height = tex_data->Height;
			region.imageExtent.depth = 1;
			vkCmdCopyBufferToImage(command_buffer, tex_data->UploadBuffer, tex_data->Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

			VkImageMemoryBarrier use_barrier[1] = {};
			use_barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			use_barrier[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			use_barrier[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			use_barrier[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			use_barrier[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			use_barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			use_barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			use_barrier[0].image = tex_data->Image;
			use_barrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			use_barrier[0].subresourceRange.levelCount = 1;
			use_barrier[0].subresourceRange.layerCount = 1;
			vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, use_barrier);
		}

		// End command buffer
		{
			VkSubmitInfo end_info = {};
			end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			end_info.commandBufferCount = 1;
			end_info.pCommandBuffers = &command_buffer;
			err = vkEndCommandBuffer(command_buffer);
			//check_vk_result(err);
			err = vkQueueSubmit(ImGuiVulkanInfo.Queue, 1, &end_info, VK_NULL_HANDLE);
			//check_vk_result(err);
			err = vkDeviceWaitIdle(ImGuiVulkanInfo.Device);
			//check_vk_result(err);
		}

		return true;
	}

	// Helper function to cleanup an image loaded with LoadTextureFromFile
	void ImGuiInterFace::RemoveTexture(MyTextureData* tex_data)
	{
		vkFreeMemory(ImGuiVulkanInfo.Device, tex_data->UploadBufferMemory, nullptr);
		vkDestroyBuffer(ImGuiVulkanInfo.Device, tex_data->UploadBuffer, nullptr);
		vkDestroySampler(ImGuiVulkanInfo.Device, tex_data->Sampler, nullptr);
		vkDestroyImageView(ImGuiVulkanInfo.Device, tex_data->ImageView, nullptr);
		vkDestroyImage(ImGuiVulkanInfo.Device, tex_data->Image, nullptr);
		vkFreeMemory(ImGuiVulkanInfo.Device, tex_data->ImageMemory, nullptr);
		ImGui_ImplVulkan_RemoveTexture(tex_data->DS);
	}

}