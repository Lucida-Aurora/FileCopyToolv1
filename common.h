#pragma once
#include "pch.h"

#include <atomic>
#include <mutex>

#define WM_USER_PREPARATION_COMPLETE (WM_USER + 1) // 准备工作完成
#define WM_USER_COPY_COMPLETE        (WM_USER + 2) // 整个复制任务完成
#define WM_USER_LOG_MESSAGE          (WM_USER + 3) // 添加一条日志消息


enum class EFileStatus {
	Pending,   // 等待处理
	InProgress, // 正在处理
	Completed, // 处理完成
	Failed,    // 处理失败
	Skipped,   // 被跳过
};

struct SFileInfo {
	CString sourcePath;
	CString destPath;
	ULONGLONG fileSize = 0;
	std::atomic<EFileStatus> status{ EFileStatus::Pending };
	SFileInfo() = default;
	SFileInfo(const SFileInfo& other)
		: sourcePath(other.sourcePath)
		, destPath(other.destPath)
		, fileSize(other.fileSize)
		, status(other.status.load()) {}

	SFileInfo& operator=(const SFileInfo& other) {
		if (this != &other) {
			sourcePath = other.sourcePath;
			destPath = other.destPath;
			fileSize = other.fileSize;
			status.store(other.status.load());
		}
		return *this;
	}
};

struct SSharedStats {
	std::atomic<ULONGLONG> totalBytesCopied{ 0 }; // 总共复制的字节数
	std::atomic<UINT> completedFileCount{ 0 }; // 已经完成的文件数量
	std::atomic<UINT> failedFileCount{ 0 }; // 处理失败的文件数量
	std::atomic<UINT> activeCopyThreads{ 0 }; // 当前活跃的复制线程数量

	ULONGLONG totalCopySize = 0; //所有文件总大小
	UINT totalFileCount = 0; //所有文件总数量
	std::mutex logMutex;
};


// 定义日志消息的类型（用于设置颜色）
enum class ELogLevel {
	Info,
	Success,
	Warning,
	Error
};
// 这是我们用来通过WM_USER_LOG_MESSAGE传递日志信息的结构体
// 工作线程会在堆上 new 一个，然后把指针作为LPARAM发送
// UI线程收到后负责 delete
struct LogMessage {
	CStringW text;
	ELogLevel level;
};