#include "buffer.h"
//#include "../vk_mem_alloc.h"


namespace VulKan {
	Buffer* Buffer::createVertexBuffer(Device* device, VkDeviceSize size, void* pData, bool ThreadBool) {
		Buffer* buffer = new Buffer(
			device, size,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT//告诉他只有在GPU本地才可读
		);
		
		if (ThreadBool) {
			buffer->ThreadUpDateBufferByStage(pData, size);
		}
		else {
			buffer->updateBufferByStage(pData, size);
		}
		return buffer;
	}

	Buffer* Buffer::createIndexBuffer(Device* device, VkDeviceSize size, void* pData, bool ThreadBool) {
		Buffer* buffer = new Buffer(
			device, size,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT//告诉他只有在GPU本地才可读
		);

		if (ThreadBool) {
			buffer->ThreadUpDateBufferByStage(pData, size);
		}
		else {
			buffer->updateBufferByStage(pData, size);
		}
		return buffer;
	}
	Buffer* Buffer::createUniformBuffer(Device* device, VkDeviceSize size, void* pData) {
		Buffer* buffer = new Buffer(
			device, size,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,//告诉他这是 UniformBuffer 
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT//告诉他这个是CPU端内存可见，更新了立马显现。
		);

		if (pData != nullptr) {
			buffer->updateBufferByStage(pData, size);
		}
		return buffer;
	}

	Buffer* Buffer::createStageBuffer(Device* device, const VkImage& dstImage, VkImageLayout dstImageLayout, uint32_t width, uint32_t height, VkDeviceSize size, void* pData, bool ThreadBool) {
		Buffer* buffer = new Buffer(
			device, size,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
		);

		if (pData != nullptr) {
			if (ThreadBool) {
				buffer->ThreadUpDateImageByStage(dstImage, dstImageLayout, width, height, pData, size);
			}
			else {
				buffer->updateImageByStage(dstImage, dstImageLayout, width, height, pData, size);
			}
		}
		return buffer;
	}


	Buffer::Buffer(Device* device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkSharingMode Mode) {
		mDevice = device;

		VkBufferCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		createInfo.size = size;//数据大小
		createInfo.usage = usage;//数据是干什么用的
		createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;//专属显示队列
		
		VmaAllocationCreateInfo VmaallocInfo = {};


		if (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT & properties) {
			VmaallocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		}
		else {
			VmaallocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
		}

		

		//vkCreateBuffer(device->getDevice(), &createInfo, nullptr, &mBuffer)
		if (vmaCreateBuffer(device->getAllocator(), &createInfo, &VmaallocInfo, &mBuffer, &mAllocation, nullptr) != VK_SUCCESS) {
			throw std::runtime_error("Error:failed to create buffer");
		}


		/*
		创建显存空间的活，全部转交给 vma 来解决，因为申请的GPU内存块，是有上线的，比如 4096 块，就算申请的很小，也只可以申请 4096 块，
		所以解决方法就是多个 Buffer 公用一大块 GPU内存，对申请的内存进行分段，但是这种事情很麻烦，而且还不一定管的好，所以引用的 vma 来对 GPU内存 进行管理
		vma（Vulkan Memory Allocator） 在  Vulkan® SDK （1.3.216.0），被以扩展的形式加入了 VulKan 当中
		*/

		/*
		//创建显存空间
		VkMemoryRequirements memReq{};
		vkGetBufferMemoryRequirements(mDevice->getDevice(), mBuffer, &memReq);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memReq.size;
		//符合我上述buffer需求的内存类型的ID们！0x001 0x010
		allocInfo.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, properties);

		
		if (vkAllocateMemory(mDevice->getDevice(), &allocInfo, nullptr, &mBufferMemory) != VK_SUCCESS) {
			throw std::runtime_error("Error: failed to allocate memory");
		}

		//把 CPU 的 mBuffer（CPU端的内存描述） 和 GPU 的 mBufferMemory（GPU端的内存描述）, 进行绑定
		vkBindBufferMemory(mDevice->getDevice(), mBuffer, mBufferMemory, 0);
		*/

		mBufferInfo.buffer = mBuffer;
		mBufferInfo.offset = 0;
		mBufferInfo.range = size;
	}

	Buffer::~Buffer() {
		if (BuffercommandBuffer != VK_NULL_HANDLE) {
			delete BuffercommandBuffer;
			BuffercommandBuffer = VK_NULL_HANDLE;
		}
		if (BuffercommandPool != VK_NULL_HANDLE) {
			delete BuffercommandPool;
			BuffercommandPool = VK_NULL_HANDLE;
		}
		if (BufferstageBuffer != VK_NULL_HANDLE) {
			delete BufferstageBuffer;
			BufferstageBuffer = VK_NULL_HANDLE;
		}
		if (mBuffer != VK_NULL_HANDLE) {
			vmaDestroyBuffer(mDevice->getAllocator(), mBuffer, mAllocation);
			mBuffer = VK_NULL_HANDLE;
			//vkDestroyBuffer(mDevice->getDevice(), mBuffer, nullptr);
		}
		/*
		if (mBufferMemory != VK_NULL_HANDLE) {
			vkFreeMemory(mDevice->getDevice(), mBufferMemory, nullptr);
		}
		*/
	}

	uint32_t Buffer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
		VkPhysicalDeviceMemoryProperties memProps;
		vkGetPhysicalDeviceMemoryProperties(mDevice->getPhysicalDevice(), &memProps);

		//0x001 | 0x100 = 0x101  i = 0 第i个对应类型就是  1 << i 1   i = 1 0x010
		for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
			if ((typeFilter & (1 << i)) && ((memProps.memoryTypes[i].propertyFlags & properties) == properties)) {
				return i;
			}
		}

		throw std::runtime_error("Error: cannot find the property memory type!");
	}

	void Buffer::updateBufferByMap(void* data, size_t size) {
		void* memPtr = nullptr;

		//vkMapMemory(mDevice->getDevice(), mBufferMemory, 0, size, 0, &memPtr);
		vmaMapMemory(mDevice->getAllocator(), mAllocation, &memPtr);
		memcpy(memPtr, data, size);
		//vkUnmapMemory(mDevice->getDevice(), mBufferMemory);
		vmaUnmapMemory(mDevice->getAllocator(), mAllocation);
	}

	void* Buffer::getupdateBufferByMap() {
		void* memPtr = nullptr;

		//vkMapMemory(mDevice->getDevice(), mBufferMemory, 0, size, 0, &memPtr);
		vmaMapMemory(mDevice->getAllocator(), mAllocation, &memPtr);
		return memPtr;
	}
	void Buffer::endupdateBufferByMap() {
		//vkUnmapMemory(mDevice->getDevice(), mBufferMemory);
		vmaUnmapMemory(mDevice->getAllocator(), mAllocation);
	}

	void Buffer::updateImageByStage(const VkImage& dstImage, VkImageLayout dstImageLayout, uint32_t width, uint32_t height, void* data, size_t size) {
		if (BufferstageBuffer != VK_NULL_HANDLE) {
			delete BufferstageBuffer;
			BufferstageBuffer = VK_NULL_HANDLE;
		}
		
		BufferstageBuffer = new Buffer(mDevice, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		BufferstageBuffer->updateBufferByMap(data, size);

		copyImage(BufferstageBuffer->getBuffer(), dstImage, dstImageLayout, width, height);

		if (BufferstageBuffer != VK_NULL_HANDLE) {
			delete BufferstageBuffer;
			BufferstageBuffer = VK_NULL_HANDLE;
		}
	}

	void Buffer::copyImage(const VkBuffer& srcBuffer, const VkImage& dstImage, VkImageLayout dstImageLayout, uint32_t width, uint32_t height) {
		if (BuffercommandPool == VK_NULL_HANDLE) {
			BuffercommandPool = new CommandPool(mDevice);
		}
		if (BuffercommandBuffer == VK_NULL_HANDLE) {
			BuffercommandBuffer = new CommandBuffer(mDevice, BuffercommandPool);
		}

		BuffercommandBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);//这个命令只提交一次

		BuffercommandBuffer->copyBufferToImage(srcBuffer, dstImage, dstImageLayout, width, height);

		BuffercommandBuffer->end();

		BuffercommandBuffer->submitSync(mDevice->getGraphicQueue(), VK_NULL_HANDLE);
	}

	void Buffer::updateBufferByStage(void* data, size_t size) {
		if (BufferstageBuffer != VK_NULL_HANDLE) {
			delete BufferstageBuffer;
			BufferstageBuffer = VK_NULL_HANDLE;
		}
		
		BufferstageBuffer = new Buffer(mDevice, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		BufferstageBuffer->updateBufferByMap(data, size);

		copyBuffer(BufferstageBuffer->getBuffer(), mBuffer, static_cast<VkDeviceSize>(size));

		if (BufferstageBuffer != VK_NULL_HANDLE) { 
			delete BufferstageBuffer;
			BufferstageBuffer = VK_NULL_HANDLE;
		}
	}

	void Buffer::copyBuffer(const VkBuffer& srcBuffer, const VkBuffer& dstBuffer, VkDeviceSize size) {
		if (BuffercommandPool == VK_NULL_HANDLE) {
			BuffercommandPool = new CommandPool(mDevice);
		}
		if (BuffercommandBuffer == VK_NULL_HANDLE) {
			BuffercommandBuffer = new CommandBuffer(mDevice, BuffercommandPool);
		}

		BuffercommandBuffer->begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);//这个命令只提交一次

		VkBufferCopy copyInfo{};
		copyInfo.size = size;
		
		BuffercommandBuffer->copyBufferToBuffer(srcBuffer, dstBuffer, 1, { copyInfo });

		BuffercommandBuffer->end();

		BuffercommandBuffer->submitSync(mDevice->getGraphicQueue(), VK_NULL_HANDLE);
	}




	

	void Buffer::ThreadUpDateImageByStage(const VkImage& dstImage, VkImageLayout dstImageLayout, uint32_t width, uint32_t height, void* data, size_t size) {
		if (BufferstageBuffer != VK_NULL_HANDLE) {
			delete BufferstageBuffer;
			BufferstageBuffer = VK_NULL_HANDLE;
		}
		BufferstageBuffer = new Buffer(mDevice, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		BufferstageBuffer->updateBufferByMap(data, size);
		mDstImage = dstImage;
		mDstImageLayout = dstImageLayout;
		mWidth = width;
		mHeight = height;
	}

	void Buffer::ThreadUpDateBufferByStage(void* data, size_t size) {
		if (BufferstageBuffer != VK_NULL_HANDLE) {
			delete BufferstageBuffer;
			BufferstageBuffer = VK_NULL_HANDLE;
		}
		BufferstageBuffer = new Buffer(mDevice, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		BufferstageBuffer->updateBufferByMap(data, size);
		mSize = size;
	}

	void Buffer::getUpDateImageCommandBuffer(CommandBuffer* commandbuffer) {
		commandbuffer->copyBufferToImage(BufferstageBuffer->getBuffer(), mDstImage, mDstImageLayout, mWidth, mHeight);
	}

	void Buffer::getUpDateBufferCommandBuffer(CommandBuffer* commandbuffer) {
		VkBufferCopy copyInfo{};
		copyInfo.size = mSize;
		commandbuffer->copyBufferToBuffer(BufferstageBuffer->getBuffer(), mBuffer, 1, { copyInfo });
	}

}