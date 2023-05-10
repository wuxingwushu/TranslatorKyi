#pragma once
#include "GUI.h"
#include "../Vulkan/Window.h"
#include "../Vulkan/device.h"
#include "../Vulkan/renderPass.h"
#include "../Vulkan/commandPool.h"
#include "../Vulkan/commandBuffer.h"
#include "../Function/tesseract.h"
#include "../Function/Translate.h"
#include <filesystem>

namespace GAME {
	class ImGuiInterFace
	{
	public:
		ImGuiInterFace(
			VulKan::Device* device, 
			VulKan::Window* Win, 
			ImGui_ImplVulkan_InitInfo Info, 
			VulKan::RenderPass* Pass,
			VulKan::CommandBuffer* commandbuffer,
			int FormatCount
		);

		~ImGuiInterFace();

		bool InterFace();

		bool GetInterFaceBool() {
			return InterFaceBool;
		}

		void SetInterFaceBool(bool Bool) {
			InterFaceBool = Bool;
			if (!InterFaceBool) {
				ImGui_ImplVulkan_NewFrame();
				ImGui_ImplGlfw_NewFrame();
				ImGui::NewFrame();
				ImGui::Render();
			}
		}

		void SetInterFace(int fi) {
			InterfaceIndexes = fi;
			SetInterFaceBool(true);
			switch (InterfaceIndexes)
			{
			case 0:
				TranslateBool = true;
				TranslateTime = clock();//获取显示时间戳
				break;
			case 1:
				ScreenshotBool = true;
				break;
			case 2:
				SetBool = true;
				break;
			case 3:
				MenuBool = true;
				TranslateTime = clock();//获取显示时间戳
				break;
			default:
				break;
			}
		}

		bool EndDisplayBool = false;//结束显示开关（给外界一个信号，结束显示）

		const VkCommandBuffer GetCommandBuffer(int i, VkCommandBufferInheritanceInfo info);

		ImGuiIO* m_io;

		Tesseract* mTesseract = nullptr;

		Translate* mTranslate = nullptr;	

	private:

		VkDescriptorPool			g_DescriptorPool = VK_NULL_HANDLE;//给 ImGui 创建的 DescriptorPool 记得销毁
		int							g_MinImageCount = 3;
		int mFormatCount;
		VulKan::Window* mWindown{ nullptr };
		VulKan::Device* mDevice{ nullptr };
		ImGui_ImplVulkan_InitInfo ImGuiVulkanInfo;

		int InterfaceIndexes = 0;
		bool InterFaceBool = false;//

		VulKan::CommandPool** ImGuiCommandPoolS;
		VulKan::CommandBuffer** ImGuiCommandBufferS;

		
		void InputTextMultilineText();//向翻译窗口粘贴文本

		bool TranslateInterface();//翻译内容显示界面
		bool TranslateBool;//翻译界面是否是刚显示
		bool ChildWindowBool = false;//右侧窗口是否显示
		bool WhoBool;//右侧窗口显示 From 还是 To
		bool WindowRenewBool = true;
		int BeginWindowSizeX = 280, BeginWindowSizeY = 148;//翻译窗口的宽高
		int RowsNumber = 4;//文本显示多行
		int kuangshu = 200;//文本有多少像素宽度
		ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackAlways;
	public:
		clock_t TranslateTime;//显示时间
		char eng[1024 * 1024];
		char zhong[1024 * 1024];

	private:
		void ScreenshotInterface();//截图操作界面
		POINT MousePosition_1;//鼠标点击位置
		POINT MousePosition_2;//鼠标松开位置
		bool ScreenshotBool = true;//界面是否是刚显示
		int x, y, w, h;//框的位置，大小

		void SetUpInterface();//设置界面
		bool SetBool = true;//界面是否是刚显示

		void MenuInterface();//菜单界面
		int MenuBool = true;//界面是否是刚显示

	public:
		char* TData;

		// A struct to manage data related to one image in vulkan
		struct MyTextureData
		{
			VkDescriptorSet DS = nullptr;         // Descriptor set: this is what you'll pass to Image()
			int             Width;
			int             Height;
			int             Channels;

			// Need to keep track of these to properly cleanup
			VkImageView     ImageView;
			VkImage         Image;
			VkDeviceMemory  ImageMemory;
			VkSampler       Sampler;
			VkBuffer        UploadBuffer;
			VkDeviceMemory  UploadBufferMemory;

			MyTextureData() { memset(this, 0, sizeof(*this)); }
		};

		MyTextureData mTextureData;

		// Helper function to find Vulkan memory type bits. See ImGui_ImplVulkan_MemoryType() in imgui_impl_vulkan.cpp
		uint32_t findMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties);

		// Helper function to load an image with common settings and return a MyTextureData with a VkDescriptorSet as a sort of Vulkan pointer
		bool LoadTextureFromFile(char* Texturedata, MyTextureData* tex_data);

		// Helper function to cleanup an image loaded with LoadTextureFromFile
		void RemoveTexture(MyTextureData* tex_data);

	};
}