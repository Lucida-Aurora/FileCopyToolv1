#pragma once
#include "pch.h"

#include <vector>

#include "common.h"
#include <queue>
#include <mutex>
#include <condition_variable>

class CCopyController;

enum class ECopyStatus {
	Idle,        // ����
	Preparing,   // ����׼�� (ɨ���ļ�)
	Copying,     // ���ڸ���
	Paused,      // ����ͣ
	Cancelling,  // ����ȡ��
	Finished,    // ����� (�����ɹ���ʧ�ܻ�ȡ��)
	Error        // ������������
};

// Ϊÿ���̵߳�ÿ���ļ����Ʋ�������һ��������������
// ����ṹ��ᱻ���ݸ� CopyFileEx �Ļص�����
struct SCopyProgressContext {
	SSharedStats* pSharedStats;
	ULONGLONG lastCopied; // ����ļ���ʼ����ǰ���Ѿ����Ƶ����ֽ���
};

struct SThreadParam {
	UINT threadId;  //�߳�ID, ��Ҫ�û���־��¼
	CCopyController* pController;
};


template<typename T>
class CTaskQueue {
	std::queue<T> m_queue;
	std::mutex m_mutex;
	std::condition_variable m_cond;
	bool m_isFinished = false;
public:
	void Push(T value) {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_queue.push(std::move(value));
		m_cond.notify_one();
	}

	bool Pop(T& value) {
		std::unique_lock<std::mutex> lock(m_mutex);
		m_cond.wait(lock, [this] {
			return !m_queue.empty() || m_isFinished;
			});
		if (m_queue.empty() && m_isFinished) {
			return false;
		}
		value = std::move(m_queue.front());
		m_queue.pop();
		return true;
	}
	void Finished() {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_isFinished = true;
		m_cond.notify_all();
	}

	void Reset() {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_isFinished = false;
		// ��ն���
		std::queue<T> empty;
		m_queue.swap(empty);
	}
};

class CCopyController {
	CString m_srcPath;
	CString m_destPath; // Դ·����Ŀ��·��
	UINT m_threadCount; // �߳�����
	HWND m_hNotifyWnd; // ֪ͨ���ھ��

	std::atomic<ECopyStatus> m_status{ ECopyStatus::Idle }; // ��ǰ����״̬
	BOOL m_bCancelSignal{ FALSE }; // �Ƿ�ȡ������
	HANDLE m_hResumeEvent;
	SSharedStats m_sharedStats; // ����ͳ����Ϣ
	std::vector<SFileInfo> m_allFiles; // �������ļ��б�
	//std::vector<std::vector<SFileInfo*>> m_threadFileLists; // ÿ���̵߳��ļ��б�
	CTaskQueue<SCopyTask> m_taskQueue;
	std::vector<SThreadParam*> m_threadParams; // �̲߳����б�
	std::vector<CWinThread*> m_threads; // �߳��б�
public:
	CCopyController();
	~CCopyController();
	bool StartCopy(const CString& source, const CString& dest, HWND wnd, UINT threadCount);
	void PauseCopy();
	void ResumeCopy();
	void CancelCopy();
	// ��ȡ��ǰ����״̬
	ECopyStatus GetStatus() const;
	void ResetStatus();
	const SSharedStats& GetSharedStats() const;


	HANDLE GetResumeEvent() const { return m_hResumeEvent; }
	BOOL* GetCancelSignalPtr() { return &m_bCancelSignal; }
	SSharedStats* GetSharedStatsPtr() { return &m_sharedStats; }
	HWND GetNotifyWnd() const { return m_hNotifyWnd; }
private:
	void _PreparationThread();
	bool _ScanFiles(const CString& source, const CString& dest);
	void _RecursiveScan(const CString& currentSrcDir, const CString& currentDstDir);

	void _GenerateTasks();
	static bool _CopyFileChunk(const SCopyTask& task, SSharedStats* pSharedStats);


	void _LaunchWorkerThreads();
	void _Cleanup();
	static UINT AFX_CDECL PreparationThreadProc(LPVOID param);
	static UINT AFX_CDECL CopyWorkerThreadProc(LPVOID param);
	static DWORD CALLBACK CopyProgressRoutine(
		LARGE_INTEGER TotalFileSize,
		LARGE_INTEGER TotalBytesTransferred,
		LARGE_INTEGER StreamSize,
		LARGE_INTEGER StreamBytesTransferred,
		DWORD dwStreamNumber,
		DWORD dwCallbackReason,
		HANDLE hSourceFile,
		HANDLE hDestinationFile,
		LPVOID lpData);
};
