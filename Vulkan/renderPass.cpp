#include "renderPass.h"

namespace VulKan {

	SubPass::SubPass() {

	}

	SubPass::~SubPass() {

	}
	//添加颜色附件参考
	void SubPass::addColorAttachmentReference(const VkAttachmentReference& ref) {
		mColorAttachmentReferences.push_back(ref);
	}
	//添加输入附件参考
	void SubPass::addInputAttachmentReference(const VkAttachmentReference& ref) {
		mInputAttachmentReferences.push_back(ref);
	}
	//添加深度网格附件参考
	void SubPass::setDepthStencilAttachmentReference(const VkAttachmentReference& ref) {
		mDepthStencilAttachmentReference = ref;
	}
	//添加解决附件参考
	void SubPass::setResolveAttachmentReference(const VkAttachmentReference& ref) {
		mResolvedAttachmentReference = ref;
	}
	//构建子描述
	void SubPass::buildSubPassDescription() {
		if (mColorAttachmentReferences.empty()) {
			throw std::runtime_error("Error: color attachment group is empty!");
		}
		mSubPassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

		mSubPassDescription.colorAttachmentCount = static_cast<uint32_t>(mColorAttachmentReferences.size());
		mSubPassDescription.pColorAttachments = mColorAttachmentReferences.data();

		mSubPassDescription.inputAttachmentCount = static_cast<uint32_t>(mInputAttachmentReferences.size());
		mSubPassDescription.pInputAttachments = mInputAttachmentReferences.data();

		mSubPassDescription.pResolveAttachments = &mResolvedAttachmentReference;

		mSubPassDescription.pDepthStencilAttachment = mDepthStencilAttachmentReference.layout == VK_IMAGE_LAYOUT_UNDEFINED? nullptr : &mDepthStencilAttachmentReference;
	}

	
	RenderPass::RenderPass(Device* &device) {
		mDevice = device;
	}

	RenderPass::~RenderPass() {
		mSubPasses.clear();
		mDependencies.clear();
		mAttachmentDescriptions.clear();

		if (mRenderPass != VK_NULL_HANDLE) {
			vkDestroyRenderPass(mDevice->getDevice(), mRenderPass, nullptr);
		}
	}
	//添加子
	void RenderPass::addSubPass(const SubPass& subpass) { mSubPasses.push_back(subpass); }
	//添加依赖
	void RenderPass::addDependency(const VkSubpassDependency& dependency) { mDependencies.push_back(dependency); }
	//添加附件
	void RenderPass::addAttachment(const VkAttachmentDescription& attachmentDes) { mAttachmentDescriptions.push_back(attachmentDes); }
	//构建渲染
	void RenderPass::buildRenderPass() {
		if (mSubPasses.empty() || mAttachmentDescriptions.empty() || mDependencies.empty()) {
			throw std::runtime_error("Error: not enough elements to build renderPass");
		}

		//unwrap
		std::vector<VkSubpassDescription> subPasses{};
		for (int i = 0; i < mSubPasses.size(); ++i) {
			subPasses.push_back(mSubPasses[i].getSubPassDescription());
		}

		VkRenderPassCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

		createInfo.attachmentCount = static_cast<uint32_t>(mAttachmentDescriptions.size());
		createInfo.pAttachments = mAttachmentDescriptions.data();

		createInfo.dependencyCount = static_cast<uint32_t>(mDependencies.size());
		createInfo.pDependencies = mDependencies.data();

		createInfo.subpassCount = static_cast<uint32_t>(subPasses.size());
		createInfo.pSubpasses = subPasses.data();

		if (vkCreateRenderPass(mDevice->getDevice(), &createInfo, nullptr, &mRenderPass) != VK_SUCCESS) {
			throw std::runtime_error("Error: failed to create renderPass");
		}
	}
}