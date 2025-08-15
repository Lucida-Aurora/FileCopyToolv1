#pragma once
#include "pch.h"

#include <atomic>
#include <mutex>

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
	std::atomic<EFileStatus> status;
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
