#pragma once

#include "../base.h"
#include "commandPool.h"
#include "device.h"

namespace VulKan {

	class CommandBuffer {
	public:
		CommandBuffer(Device* device, const CommandPool* commandPool, bool asSecondary = false);

		~CommandBuffer();

		/*
		for (int i = 0; i < Threads( 线程数 ); i++){
			二级CommandBuffer->begin

			二级CommandBuffer->绑定各种数据

			二级CommandBuffer->end
		}

		主CommandBuffer.begin ( 主CommandBuffer ) 
		主CommandBuffer.beginRenderPass

		vkCmdExecuteCommands(主CommandBuffer,  （二级CommandBuffer 的数量） ， （二级CommandBuffer 的数组）);

		主CommandBuffer.endRenderPass
		主CommandBuffer.end
		*/


		/****************************************  渲染指令  ********************************************************/
		//VkCommandBufferUsageFlags
		//VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT：这个命令，只会被使用提交一次
		//VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT：这个命令缓冲，是一个二级缓冲，位于一个renderPass当中
		//VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT:命令已经被提交了，执行期间，可以再次提交

		//VkCommandBufferInheritanceInfo:如果本命令缓冲是二级缓冲，那么这个结构体，记录了他所属的主命令信息/继承信息
		void begin(VkCommandBufferUsageFlags flag = 0, const VkCommandBufferInheritanceInfo& inheritance = {});

		//VkSubpassContents:
		//VK_SUBPASS_CONTENTS_INLINE:渲染指令会被记录在命令缓冲，本命令缓冲肯定就是主命令缓冲
		//VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS:渲染指令放在了二级指令缓冲当中,适用于主命令缓冲调用
		//beginRenderPass的时候&&使用了二级命令缓冲的情况下，使用
		void beginRenderPass(const VkRenderPassBeginInfo &renderPassBeginInfo, const VkSubpassContents &subPassContents = VK_SUBPASS_CONTENTS_INLINE);

		//绑定渲染管线
		void bindGraphicPipeline(const VkPipeline &pipeline, VkPipelineBindPoint PipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS);
		//绑定顶点缓存
		void bindVertexBuffer(const std::vector<VkBuffer> &buffers);
		//绑定顶点索引缓存
		void bindIndexBuffer(const VkBuffer& buffer);
		//绑定描述符集
		void bindDescriptorSet(const VkPipelineLayout layout, const VkDescriptorSet& descriptorSet, VkPipelineBindPoint PipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS);

		//画数量（有多少个点）
		void draw(size_t vertexCount);//没有顶点索引时用这个
		//画索引数量（有多少个点）
		void drawIndex(size_t indexCount);//有顶点索引时用这个

		//结束渲染管线
		void endRenderPass();
		//结束
		void end();



		/****************************************  数据上传指令  ********************************************************/
		//上传CommandBuffer函数(可以录制一次性上传)
		//拷贝缓存
		void copyBufferToBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t copyInfoCount, const std::vector<VkBufferCopy>& copyInfos);

		//拷贝图片
		void copyBufferToImage(VkBuffer srcBuffer,  VkImage dstImage, VkImageLayout dstImageLayout, uint32_t width, uint32_t height);
		//移动图片布局
		void transferImageLayout(const VkImageMemoryBarrier& imageMemoryBarrier, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask);


		//上传数据，同步用的工具函数
		void submitSync(VkQueue queue, VkFence fence = VK_NULL_HANDLE);//上传指令只可以再主线程运行
		
		// 获取录制好的 CommandBuffer
		[[nodiscard]] VkCommandBuffer getCommandBuffer() const noexcept { return mCommandBuffer; }
	private:
		VkCommandBuffer mCommandBuffer{ VK_NULL_HANDLE };
		Device* mDevice{ nullptr };
		const CommandPool* mCommandPool{ nullptr };
	};
}