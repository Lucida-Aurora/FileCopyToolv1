#pragma once
#include "pch.h"

#include <atomic>
#include <mutex>

enum class EFileStatus {
	Pending,   // �ȴ�����
	InProgress, // ���ڴ���
	Completed, // �������
	Failed,    // ����ʧ��
	Skipped,   // ������
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
	std::atomic<ULONGLONG> totalBytesCopied{ 0 }; // �ܹ����Ƶ��ֽ���
	std::atomic<UINT> completedFileCount{ 0 }; // �Ѿ���ɵ��ļ�����
	std::atomic<UINT> failedFileCount{ 0 }; // ����ʧ�ܵ��ļ�����
	std::atomic<UINT> activeCopyThreads{ 0 }; // ��ǰ��Ծ�ĸ����߳�����

	ULONGLONG totalCopySize = 0; //�����ļ��ܴ�С
	UINT totalFileCount = 0; //�����ļ�������
	std::mutex logMutex;
};
