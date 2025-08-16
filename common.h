#pragma once
#include "pch.h"

#include <atomic>
#include <mutex>

#define WM_USER_PREPARATION_COMPLETE (WM_USER + 1) // ׼���������
#define WM_USER_COPY_COMPLETE        (WM_USER + 2) // ���������������
#define WM_USER_LOG_MESSAGE          (WM_USER + 3) // ���һ����־��Ϣ
#define WM_USER_UPDATE_THREAD_STATUS (WM_USER + 4) // ����һ���̵߳�״̬

// �������ͣ��Ǹ�������С�ļ������Ǵ��ļ���һ�����ݿ�
enum class ETaskType {
	WHOLE_FILE,
	FILE_CHUNK
};
struct SChunkFileInfo {
	std::atomic<UINT> remainCopiedCount{ 0 };
};

// ����ṹ�壬����һ������ĸ�������
struct SCopyTask {
	ETaskType type;          // ��������
	CString sourcePath;      // Դ�ļ�·��
	CString destPath;        // Ŀ���ļ�·��
	ULONGLONG fileSize;      // �ļ��ܴ�С (����������Ҫ)
	ULONGLONG offset = 0;    // ���ݿ����ʼλ�� (�� FILE_CHUNK ��Ҫ)
	ULONGLONG chunkSize = 0; // ���ݿ�Ĵ�С (�� FILE_CHUNK ��Ҫ)
	SChunkFileInfo* chunkFileInfo = nullptr;
};



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
	std::atomic<ULONGLONG> totalBytesCopied{ 0 }; // �ܹ����Ƶ��ֽ���
	std::atomic<UINT> completedFileCount{ 0 }; // �Ѿ���ɵ��ļ�����
	std::atomic<UINT> failedFileCount{ 0 }; // ����ʧ�ܵ��ļ�����
	std::atomic<UINT> activeCopyThreads{ 0 }; // ��ǰ��Ծ�ĸ����߳�����

	ULONGLONG totalCopySize = 0; //�����ļ��ܴ�С
	UINT totalFileCount = 0; //�����ļ�������
	std::mutex logMutex;
};

// ������־��Ϣ�����ͣ�����������ɫ��
enum class ELogLevel {
	Info,
	Success,
	Warning,
	Error
};
// ������������ͨ��WM_USER_LOG_MESSAGE������־��Ϣ�Ľṹ��
// �����̻߳��ڶ��� new һ����Ȼ���ָ����ΪLPARAM����
// UI�߳��յ����� delete
struct LogMessage {
	CStringW text;
	ELogLevel level;
};

// ���ڴ����߳�״̬������Ϣ�����ݽṹ
struct SThreadStatusInfo {
	UINT     threadId;       // �߳�ID (��1��ʼ)
	CStringW statusText;     // ״̬�ı����� "������", "���"
	CStringW currentFile;    // ��ǰ���ڴ�����ļ���
};