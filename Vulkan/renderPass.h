#pragma once

#include "../base.h"
#include "device.h"

//用于汇集所有要渲染的物体，然后一次性提交渲染

namespace GAME::VulKan {
	//思路
	/*
	* 1 attachment  
	VkAttachmentDescription：描述一个颜色或者深度模板附着的结构，他并不是一个真正的附着，而是一种单纯的描述
	VkAttachmentDescription1 VkAttachmentDescription2 VkAttachmentDescription3 VkAttachmentDescription4（深度模板），告诉外界，我要什么
	VkAttachmentReference 说明本个SubPass 需要的attachment其中的一个，这一个的数组索引id是多少，这一个附着的图片期望格式是什么
	VkSubpassDescription用来填写一个子Pass的描述结构
	VkSubpassDependency：描述不同的子流程之间的依赖关系
	*/

	class SubPass {
	public:
		SubPass();

		~SubPass();

		//添加颜色附件参考
		void addColorAttachmentReference(const VkAttachmentReference& ref);
		//添加输入附件参考
		void addInputAttachmentReference(const VkAttachmentReference& ref);
		//添加深度网格附件参考
		void setDepthStencilAttachmentReference(const VkAttachmentReference& ref);
		//添加解决附件参考
		void setResolveAttachmentReference(const VkAttachmentReference& ref);
		//构建子描述
		void buildSubPassDescription();

		[[nodiscard]] auto getSubPassDescription() const { return mSubPassDescription; }

	private:
		VkSubpassDescription mSubPassDescription{};
		std::vector<VkAttachmentReference> mColorAttachmentReferences{};
		std::vector<VkAttachmentReference> mInputAttachmentReferences{};
		VkAttachmentReference mDepthStencilAttachmentReference{};
		VkAttachmentReference mResolvedAttachmentReference{};
	};




	class RenderPass {
	public:
		RenderPass(Device* &device);

		~RenderPass();
		//添加子
		void addSubPass(const SubPass& subpass);
		//添加依赖
		void addDependency(const VkSubpassDependency &dependency);
		//添加附件
		void addAttachment(const VkAttachmentDescription &attachmentDes);
		//构建渲染
		void buildRenderPass();

		[[nodiscard]] auto getRenderPass() const { return mRenderPass; }

	private:
		VkRenderPass mRenderPass{VK_NULL_HANDLE};

		std::vector<SubPass> mSubPasses{};
		std::vector<VkSubpassDependency> mDependencies{};
		std::vector<VkAttachmentDescription> mAttachmentDescriptions{};

		Device* mDevice{ nullptr };
	};
}