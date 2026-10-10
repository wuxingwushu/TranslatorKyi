#include "Profile.h"

namespace TOOL {

	/*********************************************- FPS -*********************************************/
	clock_t kaishi_time, jieshu_time;//储存时间戳
	int number_time = 0;//当前第几帧
	const int number = 60;//多少帧刷新一次
	const double miao_time = (number + 1) * 1000;//用来计算FPS的数
	double FPStime = 0.0f;//帧数

	float values[values_number] = {};//储存FPS数据
	float Max_values; //FPS数据 最大值
	float Min_values;//FPS数据 最小值
	double Mean_values;//平均帧数

	void FPS()
	{
		if (number_time >= number) {
			number_time = 0;
			jieshu_time = clock();
			FPStime = miao_time / double(jieshu_time - kaishi_time);
			Max_values = 0.0f;
			Min_values = 10000.0f;
			Mean_values = 0.0f;
			for (int i = 0; i < values_number; i++) {
				Mean_values += values[i];

				if (values_number == i + 1) {
					values[i] = FPStime;
				}
				else {
					values[i] = values[i + 1];
				}

				if (values[i] > Max_values) {
					Max_values = values[i];
				}
				if (values[i] < Min_values) {
					Min_values = values[i];
				}
			}
			Mean_values = Mean_values / values_number;
			kaishi_time = clock();
		}
		else {
			number_time++;
		}
	}





	/*********************************************- 耗时检测 -*********************************************/

	clock_t TemporaryCycleTime;//周期总耗时开始时间戳
	clock_t CycleTime = 100;//周期总耗时
	int Gap = 100;//间隔
	int CurrentCount = 0;//现在是第次轮回
	bool DetectionSwitch = false;//更新开关
	bool DetectionQuantityName = true;//第一次检测开关
	int Quantity = 0;//总检测数量
	int DetectionCount = 0;//现在是检测第几个

	//嵌套堆载
	clock_t TemporaryConsumetime[10]{};//临时时间堆载
	clock_t TemporaryConsumeName[10]{};//临时嵌套索引堆载
	int TemporaryTimeQuantity = -1; //堆载指针

	//结果数据
	int ConsumeNumber;//最多检测数量
	char* Consume_name[DetectionNumber]{};//储存检测的名字
	clock_t TemporaryConsume_time[DetectionNumber]{};//储存检测的周期总累加耗时
	double Consume_time[DetectionNumber]{};//储存检测的百分比
	double Consume_Second[DetectionNumber]{};//储存检测的秒

	//时间记录
	bool SecondVectorBool[DetectionNumber]{}; //秒数据 最大值
	float* Consume_SecondVector[DetectionNumber]{};//储存检测的秒数组
	int SecondVectorIndex[DetectionNumber]{};//秒数组索引
	float Max_Secondvalues[SecondVectorNumber]{}; //秒数据 最大值
	float Min_Secondvalues[SecondVectorNumber]{};//秒数据 最小值

	void StartTiming(char* name, bool RecordBool)
	{
		if (DetectionQuantityName) {
			SecondVectorBool[Quantity] = RecordBool;//这个检测对象是否开启了记录
			if ((Quantity != 0) && (name == Consume_name[0])) {
				DetectionQuantityName = false;//所有检测录入一边（注意，要是一次循环没有录入就是没有录入，会有BUG，所以应用时要让他第一次全部录入进去）
				DetectionCount = 0;//新的一轮记得设为 0 ，要不然会出问题
				ConsumeNumber = Quantity;
			}
			else {
				Consume_name[Quantity] = name;
				if (SecondVectorBool[Quantity]) {
					Consume_SecondVector[Quantity] = new float[SecondVectorNumber] {};//申请记录时间数据用的空间
				}
				Quantity++;
			}
		}
		else {
			if (name == Consume_name[0]) {//判断是否是新的一轮
				DetectionCount = 0;
			}
		}
		TemporaryTimeQuantity++;//堆载指针压载
		TemporaryConsumetime[TemporaryTimeQuantity] = clock();
		TemporaryConsumeName[TemporaryTimeQuantity] = DetectionCount + TemporaryTimeQuantity;//压入索引
	}

	void StartEnd()
	{
		TemporaryConsume_time[TemporaryConsumeName[TemporaryTimeQuantity]] += (clock() - TemporaryConsumetime[TemporaryTimeQuantity]);
		TemporaryTimeQuantity--;//堆载指针出载
		DetectionCount++;
	}

	void MomentTiming(char* name, int* Index)
	{
		if (Index[0] == NULL) {
			Index[0] = Quantity;
			Consume_name[Quantity] = name;
			Consume_Second[Quantity] = 0.0f;
			SecondVectorBool[Quantity] = false;
			Quantity++;
		}
		TemporaryTimeQuantity++;//堆载指针压载
		TemporaryConsumetime[TemporaryTimeQuantity] = clock();
		TemporaryConsumeName[TemporaryTimeQuantity] = Index[0];//压入索引
	}

	void MomentEnd()
	{
		Consume_Second[TemporaryConsumeName[TemporaryTimeQuantity]] = double(clock() - TemporaryConsumetime[TemporaryTimeQuantity]) / 1000;
		TemporaryTimeQuantity--;//堆载指针出载
	}

	void RefreshTiming()
	{
		if (DetectionSwitch) {
			CycleTime = clock() - TemporaryCycleTime;//Interval 个轮回，结束计时，得出时间
			for (int i = 0; i < ConsumeNumber; i++) {
				Consume_time[i] = (double(TemporaryConsume_time[i] * 100) / CycleTime);//求出他在一个帧周期的耗时占比
				Consume_Second[i] = (double(TemporaryConsume_time[i]) / (1000 * Gap));//他所花时间
				TemporaryConsume_time[i] = 0;//清零，累计下 Interval 个轮回的时间

				if (SecondVectorBool[i]) {//时间是否记录
					Max_Secondvalues[i] = -10000.0f;
					Min_Secondvalues[i] = 10000.0f;
					for (int j = 0; j < SecondVectorNumber; j++) {
						if (SecondVectorNumber == j + 1) {
							Consume_SecondVector[i][j] = float(Consume_Second[i]);
						}
						else {
							Consume_SecondVector[i][j] = Consume_SecondVector[i][j + 1];
						}

						if (Consume_SecondVector[i][j] > Max_Secondvalues[i]) {
							Max_Secondvalues[i] = Consume_SecondVector[i][j];
						}
						if (Consume_SecondVector[i][j] < Min_Secondvalues[i]) {
							Min_Secondvalues[i] = Consume_SecondVector[i][j];
						}
					}
				}
			}
			DetectionSwitch = false;
			TemporaryCycleTime = clock();//Interval 个轮回，开始计时
		}
		else {
			CurrentCount++;//一轮结束
			if (CurrentCount >= Gap) {//判断第 Interval 就更新显示
				CurrentCount = 0;
				DetectionSwitch = true;
			}
		}
	}
}