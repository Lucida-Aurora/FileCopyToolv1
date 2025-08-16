#pragma once
#include "pch.h"

#include <vector>

#include "common.h"

class CCopyController;

enum class ECopyStatus {
	Idle,        // 空闲
	Preparing,   // 正在准备 (扫描文件)
	Copying,     // 正在复制
	Paused,      // 已暂停
	Cancelling,  // 正在取消
	Finished,    // 已完成 (包括成功、失败或被取消)
	Error        // 发生致命错误
};

// 为每个线程的每个文件复制操作创建一个独立的上下文
// 这个结构体会被传递给 CopyFileEx 的回调函数
struct SCopyProgressContext {
	SSharedStats* pSharedStats;
	ULONGLONG lastCopied; // 这个文件开始复制前，已经复制的总字节数
};

struct SThreadParam {
	UINT threadId;  //线程ID, 主要用户日志记录
	HWND hNotifyWnd; //通知窗口句柄, 用户PostMessage通知UI更新
	SSharedStats* pSharedStats; //共享统计信息
	std::vector<SFileInfo*>* pFilesToCopy; //文件列表
	CCopyController* pController;
};

class CCopyController {
	CString m_srcPath;
	CString m_destPath; // 源路径和目标路径
	UINT m_threadCount; // 线程数量
	HWND m_hNotifyWnd; // 通知窗口句柄

	std::atomic<ECopyStatus> m_status{ ECopyStatus::Idle }; // 当前任务状态
	BOOL m_bCancelSignal{ FALSE }; // 是否取消复制
	HANDLE m_hResumeEvent;
	SSharedStats m_sharedStats; // 共享统计信息
	std::vector<SFileInfo> m_allFiles; // 待复制文件列表
	std::vector<std::vector<SFileInfo*>> m_threadFileLists; // 每个线程的文件列表
	std::vector<SThreadParam*> m_threadParams; // 线程参数列表
	std::vector<CWinThread*> m_threads; // 线程列表
public:
	CCopyController();
	~CCopyController();
	bool StartCopy(const CString& source, const CString& dest, HWND wnd, UINT threadCount);
	void PauseCopy();
	void ResumeCopy();
	void CancelCopy();
	// 获取当前任务状态
	ECopyStatus GetStatus() const;
	void ResetStatus();
	const SSharedStats& GetSharedStats() const;
private:
	void _PreparationThread();
	bool _ScanFiles(const CString& source, const CString& dest);
	void _RecursiveScan(const CString& currentSrcDir, const CString& currentDstDir);
	void _DistributeFiles();
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
