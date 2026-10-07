#pragma once
//VulKan 层的分级日志开关。默认关闭，全部宏都会被优化成 ((void)0)。
//需要排查渲染设备 / 交换链问题时，把下面这行改成 1 重新编译即可。
#define TRANSLATOR_ENABLE_LOG 0

#if TRANSLATOR_ENABLE_LOG

	#if defined(__ANDROID__)
		#include <android/log.h>
		#define TK_LOG_TAG "TranslatorKyi"
		#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, TK_LOG_TAG, ##__VA_ARGS__)
		#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG,   TK_LOG_TAG, ##__VA_ARGS__)
		#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,    TK_LOG_TAG, ##__VA_ARGS__)
		#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,    TK_LOG_TAG, ##__VA_ARGS__)
		#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,   TK_LOG_TAG, ##__VA_ARGS__)
		#define LOGF(...) __android_log_print(ANDROID_LOG_FATAL,   TK_LOG_TAG, ##__VA_ARGS__)
	#elif defined(_WIN32)
		#include <cstdio>
		#define LOGV(...) do { printf("[VERBOSE] "); printf(__VA_ARGS__); printf("\n"); } while(0)
		#define LOGD(...) do { printf("[DEBUG] ");   printf(__VA_ARGS__); printf("\n"); } while(0)
		#define LOGI(...) do { printf("[INFO] ");    printf(__VA_ARGS__); printf("\n"); } while(0)
		#define LOGW(...) do { printf("[WARN] ");    printf(__VA_ARGS__); printf("\n"); } while(0)
		#define LOGE(...) do { fprintf(stderr, "[ERROR] "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } while(0)
		#define LOGF(...) do { fprintf(stderr, "[FATAL] "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } while(0)
	#endif

#else

	#define LOGV(...) ((void)0)
	#define LOGD(...) ((void)0)
	#define LOGI(...) ((void)0)
	#define LOGW(...) ((void)0)
	#define LOGE(...) ((void)0)
	#define LOGF(...) ((void)0)

#endif
