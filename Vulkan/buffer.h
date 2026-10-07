#pragma once
#include "commandBuffer.h"


namespace VulKan {

	// Buffer 用于 CPU 向 GPU 提供数据用的

	class Buffer {
	public:
		Buffer(Device* device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkSharingMode Mode = VK_SHARING_MODE_EXCLUSIVE);

		~Buffer();

		//获取 Buffer
		[[nodiscard]] inline VkBuffer getBuffer() const noexcept { return mBuffer; }

		[[nodiscard]] inline VkDescriptorBufferInfo& getBufferInfo() noexcept { return mBufferInfo; }
		
		//创建顶点缓存
		static Buffer* createVertexBuffer(Device* device, VkDeviceSize size, void * pData, bool ThreadBool = false);
		//创建顶点索引缓存
		static Buffer* createIndexBuffer(Device* device, VkDeviceSize size, void* pData, bool ThreadBool = false);
		//创建描述符
		static Buffer* createUniformBuffer(Device* device, VkDeviceSize size, void* pData = nullptr);
		//创建 SSBO（Storage Buffer），用于粒子系统等实例化数据
		//persistentMapping=true 时构造后即映射内存，可用 getPersistentMappedPtr() 获取指针
		static Buffer* createStorageBuffer(Device* device, VkDeviceSize size, void* pData = nullptr, bool persistentMapping = false);
		//创建临时Buffer,用来上传图片
		static Buffer* createStageBuffer(Device* device, const VkImage& dstImage, VkImageLayout dstImageLayout, uint32_t width, uint32_t height, VkDeviceSize size, void* pData = nullptr, bool ThreadBool = false);


		/*
		* 1 通过内存Mapping的形式，直接对内存进行更改，适用于HostVisible类型的内存
		* 2 如果本内存是LocalOptimal， 那么就必须创建中间的StageBuffer，先复制到StageBuffer，再拷贝入目标Buffer
		*/
		

		//向GPU上传临时数据
		void updateBufferByMap(void *data, size_t size);//上传数据，（CPU可见的Buffer）
		//获取HOST数据指针
		void* getupdateBufferByMap();
		//关闭HOST数据指针
		void endupdateBufferByMap();
		//持久映射：首次调用映射内存，返回指针；后续调用直接返回已映射指针，避免每帧 map/unmap 开销
		void* getPersistentMappedPtr();
		/********************************  生成单一模型  *************************************/
		//我们创建的这个Buffer是CPU不可读的，所以不可以直接上传数据，所以做一个中间Buffer用来上传数据，上传完就销毁

		//向GPU上传Image数据
		void updateImageByStage(const VkImage& dstImage, VkImageLayout dstImageLayout, uint32_t width, uint32_t height, void* data, size_t size);
				
		//向GPU内存拷贝数据
		void copyImage(const VkBuffer& srcBuffer, const VkImage& dstImage, VkImageLayout dstImageLayout, uint32_t width, uint32_t height);


		//向GPU上传Buffer数据
		void updateBufferByStage(void* data, size_t size);
		//向GPU内存拷贝数据
		void copyBuffer(const VkBuffer& srcBuffer, const VkBuffer& dstBuffer, VkDeviceSize size);


		/********************************  生成大规模模型  *************************************/
		//我们创建的这个Buffer是CPU不可读的，所以不可以直接上传数据，所以做一个中间Buffer用来上传数据，上传完就销毁
		//向GPU上传Image数据
		void ThreadUpDateImageByStage(const VkImage& dstImage, VkImageLayout dstImageLayout, uint32_t width, uint32_t height, void* data, size_t size);
		//向GPU上传Buffer数据
		void ThreadUpDateBufferByStage(void* data, size_t size);

		void getUpDateImageCommandBuffer(CommandBuffer* commandbuffer);
		void getUpDateBufferCommandBuffer(CommandBuffer* commandbuffer);

	private://多线程临时储存数据
		VkImage mDstImage{ VK_NULL_HANDLE };
		VkImageLayout mDstImageLayout{};
		uint32_t mWidth{ 0 };
		uint32_t mHeight{ 0 };
		size_t mSize{ 0 };




		
		
	private:
		uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

	private:
		VkBuffer mBuffer{ VK_NULL_HANDLE };//CPU端的内存描述
#if defined(_WIN32) || defined(__ANDROID__)
		VmaAllocation mAllocation{};// vma 的GPU内存描述
#endif
		void* mPersistentMappedMemory{ nullptr };//持久映射指针
		//VkDeviceMemory mBufferMemory{ VK_NULL_HANDLE };//GPU端的内存描述，（废弃，专用 vma）
		Device* mDevice{ nullptr };
		VkDescriptorBufferInfo mBufferInfo{};



	private://这些数据是临时的，用来上传数据用的
		CommandPool* BuffercommandPool{ nullptr };
		CommandBuffer* BuffercommandBuffer{ nullptr };
		Buffer* BufferstageBuffer{ nullptr };
	};
}