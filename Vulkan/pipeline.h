#pragma once

#include "../base.h"
#include "device.h"
#include "shader.h"
#include "renderPass.h"

namespace VulKan {

	class Pipeline {
	public:
		Pipeline(Device* device, RenderPass* renderPass);

		~Pipeline();

		//设置Shader
		void setShaderGroup(std::vector<Shader*> shaderGroup);
		//设置视窗口
		void setViewports(const std::vector<VkViewport>& viewports) { mViewports = viewports; }
		//设置
		void setScissors(const std::vector<VkRect2D>& scissors) { mScissors = scissors; }

		void pushBlendAttachment(const VkPipelineColorBlendAttachmentState& blendAttachment) { 
			mBlendAttachmentStates.push_back(blendAttachment); 
		}
		//构建渲染管线
		void build();

		void ReconfigurationPipeline();

	public:
		VkPipelineVertexInputStateCreateInfo mVertexInputState{};//描述模型顶点的
		VkPipelineInputAssemblyStateCreateInfo mAssemblyState{};//图元组装的
		VkPipelineViewportStateCreateInfo mViewportState{};//
		VkPipelineRasterizationStateCreateInfo mRasterState{};
		VkPipelineMultisampleStateCreateInfo mSampleState{};
		std::vector<VkPipelineColorBlendAttachmentState> mBlendAttachmentStates{};
		VkPipelineColorBlendStateCreateInfo mBlendState{};
		VkPipelineDepthStencilStateCreateInfo mDepthStencilState{};//深度和模板测试
		VkPipelineLayoutCreateInfo mLayoutState{};//创建管线布局

	public:
		[[nodiscard]] auto getPipeline() const noexcept { return mPipeline; }
		[[nodiscard]] auto getLayout() const noexcept { return mLayout; }

		VkDescriptorSetLayout DescriptorSetLayout{ VK_NULL_HANDLE };

	private:
		VkPipeline mPipeline{ VK_NULL_HANDLE };
		VkPipelineLayout mLayout{ VK_NULL_HANDLE };
		Device* mDevice{ nullptr };
		RenderPass* mRenderPass{ nullptr };

		std::vector<Shader*> mShaders{};

		std::vector<VkViewport> mViewports{};
		std::vector<VkRect2D> mScissors{};
	};
}