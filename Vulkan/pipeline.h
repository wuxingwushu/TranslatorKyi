#pragma once
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
		[[nodiscard]] inline VkPipeline getPipeline() const noexcept { return mPipeline; }
		[[nodiscard]] inline VkPipelineLayout getLayout() const noexcept { return mLayout; }

		//顶点着色器实例化展块标记：为真时该管线的一个实例（顶点缓冲按实例步进）
		//会被展开成 4 个顶点的方块，绘制必须用 draw(4, 数量)，见 UVDynamicDiagram::InitCommandBuffer。
		bool mQuadExpansionByVertexShader{ false };

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