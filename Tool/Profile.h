#pragma once
//性能统计模块：FPS 采样 + 耗时检测。原先这些声明/定义都在 Tool.h / Tool.cpp 里。
#include <time.h>

namespace TOOL {

/************************************************- FPS -************************************************/

	extern clock_t kaishi_time, jieshu_time;//储存时间戳
	extern int number_time;//当前第几帧
	extern const int number;//多少帧刷新一次
	extern const double miao_time;//用来计算FPS的数
	extern double FPStime;//帧数

	const int values_number = 60;//记录多少FPS数据
	extern float values[values_number];//储存FPS数据
	extern float Max_values; //FPS数据 最大值
	extern float Min_values;//FPS数据 最小值
	extern double Mean_values;//平均帧数

	extern void FPS();





/*********************************************- 耗时检测 -*********************************************/

/*
功能暂时不太完善，但是可以使用，按照规定使用就可以了，

1. 他们三，RefreshTiming()，EndTiming()，StartTiming()，必须在一次循环都得运行一边
2. 一个  StartTiming()  就必须有一个  StartEnd()  。
3. RefreshTiming() 只可以有一个。
4. StartTiming()， StartEnd() 嵌套最多10层。
5. StartTiming()，设置的名字必须是唯一的，不可以重复。

6. MomentTiming(), MomentEnd()这是检测间接运行的代码片段的时耗
7. MomentTiming() 名字可以不唯一，需要一个 int 来储存索引。

例子：
#include "Profile.h"//引用头文件就可以了

int main(){
	while(1){
	TOOL::StartTiming("唯一的名字", true);
	-- 被检测耗时的区域 --
	TOOL::StartEnd();

	RefreshTiming();
	}

	return 0;
}
*/

	extern clock_t TemporaryCycleTime;//周期总耗时开始时间戳
	extern clock_t CycleTime;//周期总耗时
	extern int Gap;//间隔
	extern int CurrentCount;//现在是第次轮回
	extern bool DetectionSwitch;//更新开关
	extern bool DetectionQuantityName;//第一次检测开关
	extern int Quantity;//总检测数量
	extern int DetectionCount;//现在是检测第几个

	//嵌套堆载
	extern clock_t TemporaryConsumetime[10];//临时时间堆载
	extern clock_t TemporaryConsumeName[10];//临时嵌套索引堆载
	extern int TemporaryTimeQuantity; //堆载指针

	//结果数据
	const int DetectionNumber = 1000;//最多检测数量
	extern int ConsumeNumber;//最多检测数量
	extern char* Consume_name[DetectionNumber];//储存检测的名字
	extern clock_t TemporaryConsume_time[DetectionNumber];//储存检测的周期总累加耗时
	extern double Consume_time[DetectionNumber];//储存检测的百分比
	extern double Consume_Second[DetectionNumber];//储存检测的秒

	//时间记录
	const int SecondVectorNumber = 60;//最多记录检测数量
	extern bool SecondVectorBool[DetectionNumber]; //秒数据 最大值
	extern float* Consume_SecondVector[DetectionNumber];//储存检测的秒数组
	extern int SecondVectorIndex[DetectionNumber];//秒数组索引
	extern float Max_Secondvalues[SecondVectorNumber]; //秒数据 最大值
	extern float Min_Secondvalues[SecondVectorNumber];//秒数据 最小值

	//                       唯一名字       是否记录时间数据
	extern void StartTiming(char* name, bool RecordBool = false);
	extern void StartEnd();
	//                       唯一名字     储存独立索引
	extern void MomentTiming(char* name, int* Index);
	extern void MomentEnd();
	extern void RefreshTiming();
}