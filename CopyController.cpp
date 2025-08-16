#include "pch.h"
#include "CopyController.h"

CCopyController::CCopyController() : m_threadCount(0), m_hNotifyWnd(nullptr), m_status(ECopyStatus::Idle), m_bCancelSignal(false) {
	m_hResumeEvent = CreateEvent(nullptr, TRUE, TRUE, nullptr);
	if (m_hResumeEvent == nullptr) {
		throw std::runtime_error("Failed to create resume event");
	}
}

CCopyController::~CCopyController() {
	_Cleanup();
}

bool CCopyController::StartCopy(const CString& source, const CString& dest, HWND wnd, UINT threadCount) {
	//��ֹ�ظ�������������
	if (m_status.load() != ECopyStatus::Idle) {
		return false;
	}
	m_srcPath = source;
	m_destPath = dest;
	m_hNotifyWnd = wnd;
	m_threadCount = threadCount > 0 ? threadCount : 1;
	m_status.store(ECopyStatus::Preparing);
	// ����׼���߳�
	AfxBeginThread(PreparationThreadProc, this);
	return true;
}

void CCopyController::PauseCopy() {
	if (m_status.load() == ECopyStatus::Copying) {
		ResetEvent(m_hResumeEvent);
		m_status.store(ECopyStatus::Paused);
	}
}

void CCopyController::ResumeCopy() {
	if (m_status.load() == ECopyStatus::Paused) {
		SetEvent(m_hResumeEvent);
		m_status.store(ECopyStatus::Copying);
	}
}

void CCopyController::CancelCopy() {
	m_bCancelSignal = TRUE;
	if (m_status.load() == ECopyStatus::Paused) {
		ResumeCopy();
	}
	m_status.store(ECopyStatus::Cancelling);
}

ECopyStatus CCopyController::GetStatus() const {
	if (m_status.load() == ECopyStatus::Copying && (m_sharedStats.completedFileCount + m_sharedStats.failedFileCount) == m_sharedStats.totalFileCount) {
		// ע�⣺�����о�̬�������գ���������ʾ��
		// ���õ��������ڹ����̷߳����������ʱ������Ϣ֪ͨUI��
		// const_cast<std::atomic<CopyStatus>&>(m_status).store(CopyStatus::Finished);
	}
	return m_status.load();
}

void CCopyController::ResetStatus() {
	m_status.store(ECopyStatus::Idle);
}

const SSharedStats& CCopyController::GetSharedStats() const {
	return m_sharedStats;
}

void CCopyController::_PreparationThread() {
	if (!_ScanFiles(m_srcPath, m_destPath)) {
		m_status.store(ECopyStatus::Error);
		//todo: ֪ͨUI׼��ʧ��
		//::PostMessage(m_hNotifyWnd, WM_COPY_PREPARATION_FAILED, 0, 0);
		return;
	}
	if (m_allFiles.empty()) {
		m_status.store(ECopyStatus::Finished);
		//todo: ֪ͨUIû���ļ���Ҫ����
		//::PostMessage(m_hNotifyWnd, WM_COPY_NO_FILES, 0, 0);
		return;
	}
	// �����ļ����߳�
	_DistributeFiles();
	// ���������߳�
	_LaunchWorkerThreads();
	m_status.store(ECopyStatus::Copying);
}

bool CCopyController::_ScanFiles(const CString& source, const CString& dest) {
	DWORD fileAttributes = GetFileAttributes(source);
	if (fileAttributes == INVALID_FILE_ATTRIBUTES) {
		// Դ·�������ڻ��޷�����
		return false;
	}
	if (fileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		// ���ļ��У�ȷ��Ŀ��·��Ҳ���ļ���
		if (!PathFileExists(dest)) { //���·����Ч
			CreateDirectory(dest, nullptr);
		}
		_RecursiveScan(source, dest);
	}
	else {
		SFileInfo fileInfo;
		fileInfo.sourcePath = source;
		CString finalDestPath = dest;
		if (PathIsDirectory(dest)) {
			finalDestPath.TrimRight(_T("\\"));
			finalDestPath += _T("\\") + CString(PathFindFileName(source));
		}
		fileInfo.destPath = finalDestPath;
		HANDLE hFile = CreateFile(source, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (hFile != INVALID_HANDLE_VALUE) {
			LARGE_INTEGER fileSize;
			GetFileSizeEx(hFile, &fileSize);
			fileInfo.fileSize = fileSize.QuadPart;
			CloseHandle(hFile);
			m_allFiles.push_back(fileInfo);
		}
	}
	for (const auto& file : m_allFiles) {
		m_sharedStats.totalCopySize += file.fileSize;
	}
	m_sharedStats.totalFileCount = m_allFiles.size();
	PostMessage(m_hNotifyWnd, WM_USER_PREPARATION_COMPLETE, m_sharedStats.totalFileCount, m_sharedStats.totalCopySize);
	return true;
}

void CCopyController::_RecursiveScan(const CString& currentSrcDir, const CString& currentDstDir) {
	CFileFind finder;
	CString searchPath = currentSrcDir + _T("\\*.*");
	BOOL bWorking = finder.FindFile(searchPath);
	while (bWorking) {
		bWorking = finder.FindNextFile();
		if (finder.IsDots()) {
			continue;
		}
		CString sourceFilePath = finder.GetFilePath();
		CString fileName = finder.GetFileName();
		CString destFilePath = currentDstDir + _T("\\") + fileName;
		if (finder.IsDirectory()) {
			CreateDirectory(destFilePath, nullptr);
			_RecursiveScan(sourceFilePath, destFilePath);
		}
		else {
			SFileInfo fileInfo;
			fileInfo.sourcePath = sourceFilePath;
			fileInfo.destPath = destFilePath;
			fileInfo.fileSize = finder.GetLength();
			m_allFiles.push_back(fileInfo);
		}
	}
	finder.Close();
}

void CCopyController::_DistributeFiles() {
	m_threadFileLists.resize(m_threadCount);
	for (int i = 0; i < m_allFiles.size(); ++i) {
		m_threadFileLists[i % m_threadCount].push_back(&m_allFiles[i]);
	}
}

void CCopyController::_LaunchWorkerThreads() {
	for (int i = 0; i < m_threadCount; ++i) {
		SThreadParam* pParam = new SThreadParam();
		pParam->threadId = i + 1;
		pParam->hNotifyWnd = m_hNotifyWnd;
		pParam->pSharedStats = &m_sharedStats;
		pParam->pFilesToCopy = &m_threadFileLists[i];
		pParam->pController = this;
		m_threadParams.push_back(pParam);

		CWinThread* pThread = AfxBeginThread(CopyWorkerThreadProc, pParam);
		m_threads.push_back(pThread);
	}
}

void CCopyController::_Cleanup() {
	//for (int i = 0; i < m_threadCount; i++) {
	//	SetEvent(m_threadCompletedEvents[i]);
	//}
	for (CWinThread* pThread : m_threads) {
		if (pThread) {
			WaitForSingleObject(pThread->m_hThread, INFINITE);
		}
	}

	m_threads.clear();
	if (m_hResumeEvent) {
		CloseHandle(m_hResumeEvent);
		m_hResumeEvent = NULL;
	}
	m_threadParams.clear();

	m_allFiles.clear();
	m_threadFileLists.clear();
}

UINT CCopyController::PreparationThreadProc(LPVOID param) {
	CCopyController* pThis = static_cast<CCopyController*>(param);
	if (pThis) {
		pThis->_PreparationThread();
	}
	return 0;
}

UINT CCopyController::CopyWorkerThreadProc(LPVOID param) {
	SThreadParam* pThreadParam = static_cast<SThreadParam*>(param);
	if (pThreadParam == nullptr) {
		return 1;
	}
	CCopyController* pController = pThreadParam->pController;
	SSharedStats* pSharedStats = pThreadParam->pSharedStats;
	HWND hNotifyWnd = pThreadParam->hNotifyWnd;
	UINT threadId = pThreadParam->threadId;
	++pSharedStats->activeCopyThreads;
	CString logMsg;
	logMsg.Format(_T("�߳�%u������"), threadId);

	LogMessage* pLogStart = new LogMessage{ logMsg, ELogLevel::Info };
	PostMessage(hNotifyWnd, WM_USER_LOG_MESSAGE, 0, reinterpret_cast<LPARAM>(pLogStart));
	for (SFileInfo* pFile : *pThreadParam->pFilesToCopy) {
		WaitForSingleObject(pController->m_hResumeEvent, INFINITE);
		if (pController->m_bCancelSignal) break;
		pFile->status.store(EFileStatus::InProgress);

		CString fileName = PathFindFileName(pFile->sourcePath);
		SThreadStatusInfo* pStatusInfo = new SThreadStatusInfo{ threadId, _T("������"), fileName };
		PostMessage(hNotifyWnd, WM_USER_UPDATE_THREAD_STATUS, 0, reinterpret_cast<LPARAM>(pStatusInfo));
		logMsg.Format(_T("�߳�%u��ʼ���ƣ�"), threadId);
		logMsg += fileName;
		LogMessage* pLogCopy = new LogMessage{ logMsg, ELogLevel::Info };
		PostMessage(hNotifyWnd, WM_USER_LOG_MESSAGE, 0, reinterpret_cast<LPARAM>(pLogCopy));
		SCopyProgressContext progressCtx;
		progressCtx.pSharedStats = pSharedStats;
		progressCtx.lastCopied = 0;
		BOOL bSuccess = CopyFileEx(
			pFile->sourcePath,
			pFile->destPath,
			(LPPROGRESS_ROUTINE)CopyProgressRoutine, // ���ǵĻص�����
			&progressCtx,                            // ���ݱ��θ��ƵĶ���������
			&(pController->m_bCancelSignal),         // ����ȡ���źŵĵ�ַ
			0);                                      // Ĭ�ϱ�־
		if (bSuccess) {
			pFile->status.store(EFileStatus::Completed);
			++pSharedStats->completedFileCount;
			LogMessage* pLogSuccess = new LogMessage{ fileName + _T(" ���Ƴɹ���"), ELogLevel::Success };
			PostMessage(hNotifyWnd, WM_USER_LOG_MESSAGE, 0, reinterpret_cast<LPARAM>(pLogSuccess));
		}
		else {
			pFile->status.store(EFileStatus::Failed);
			++pSharedStats->failedFileCount;
			LogMessage* pLogError = new LogMessage{ fileName + _T(" ����ʧ�ܡ�"), ELogLevel::Error };
			PostMessage(hNotifyWnd, WM_USER_LOG_MESSAGE, 0, reinterpret_cast<LPARAM>(pLogError));
		}
	}
	SThreadStatusInfo* pStatusDone = new SThreadStatusInfo{ threadId, L"���", L"" };
	PostMessage(hNotifyWnd, WM_USER_UPDATE_THREAD_STATUS, 0, reinterpret_cast<LPARAM>(pStatusDone));
	if (pSharedStats->activeCopyThreads.fetch_sub(1) == 1) {
		PostMessage(hNotifyWnd, WM_USER_COPY_COMPLETE, 0, 0);
	}
	delete pThreadParam;
	return 0;
}

DWORD CCopyController::CopyProgressRoutine(LARGE_INTEGER TotalFileSize, LARGE_INTEGER TotalBytesTransferred,
	LARGE_INTEGER StreamSize, LARGE_INTEGER StreamBytesTransferred, DWORD dwStreamNumber, DWORD dwCallbackReason,
	HANDLE hSourceFile, HANDLE hDestinationFile, LPVOID lpData) {
	SCopyProgressContext* pContext = static_cast<SCopyProgressContext*>(lpData);
	if (pContext && pContext->pSharedStats) {
		// ʵʱ����ȫ���Ѹ����ֽ���
		// ��ǰ�ܽ��� = ����ļ���ʼǰ���ܽ��� + ����ļ���ǰ�Ѿ����ƵĽ���
		pContext->pSharedStats->totalBytesCopied.store(
			pContext->pSharedStats->totalBytesCopied.load() +
			TotalBytesTransferred.QuadPart - pContext->lastCopied
		);
		pContext->lastCopied = TotalBytesTransferred.QuadPart;
	}
	return PROGRESS_CONTINUE;
}