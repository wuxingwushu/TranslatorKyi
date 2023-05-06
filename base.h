#pragma once
#include <iostream>
#include <vector>//动态数组
#include <array>//静态数组
#include <map>
#include <string>
#include <optional>
#include <set>
#include <fstream>


#include <windows.h>//WIN API

//矩阵计算库
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>//跨平台接口
#include <vulkan/vulkan.h>//VulKan API



//#define VMA_DEBUG_MARGIN 16//边距（Margins）https://blog.csdn.net/weixin_50523841/article/details/122506850
#include "vk_mem_alloc.h"//内存分配器，宏声明 放在了 device.cpp 当中去了，引用的时候要放在CPP当中用要不然会报错（反复定义）

#include "FilePath.h"//资源路径

#include "Tool/Tool.h"
#include "Variable.h"


//开启的测试模式
const std::vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"//测试类型
};


struct VPMatrices {
	glm::mat4 mViewMatrix;
	glm::mat4 mProjectionMatrix;

	VPMatrices() {
		mViewMatrix = glm::mat4(1.0f);
		mProjectionMatrix = glm::mat4(1.0f);
	}
};

struct ObjectUniform {
	glm::mat4 mModelMatrix;

	ObjectUniform() {
		mModelMatrix = glm::mat4(1.0f);
	}
};