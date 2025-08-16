#pragma once
#include "pch.h"

#include <vector>

#include "common.h"

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
	HWND hNotifyWnd; //֪ͨ���ھ��, �û�PostMessage֪ͨUI����
	SSharedStats* pSharedStats; //����ͳ����Ϣ
	std::vector<SFileInfo*>* pFilesToCopy; //�ļ��б�
	CCopyController* pController;
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
	std::vector<std::vector<SFileInfo*>> m_threadFileLists; // ÿ���̵߳��ļ��б�
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
