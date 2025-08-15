#include "pch.h"
#include "CopyController.h"

CCopyController::CCopyController() : m_threadCount(0), m_hNotifyWnd(nullptr), m_status(CopyStatus::Idle), m_bCancelSignal(false) {
	m_hResumeEvent = CreateEvent(nullptr, TRUE, TRUE, nullptr);
	if (m_hResumeEvent == nullptr) {
		throw std::runtime_error("Failed to create resume event");
	}
}

CCopyController::~CCopyController() {
	_Cleanup();
}

bool CCopyController::StartCopy(const CString& source, const CString& dest, HWND wnd, UINT threadCount) {
	//防止重复启动复制任务
	if (m_status.load() != CopyStatus::Idle) {
		return false;
	}
	m_srcPath = source;
	m_destPath = dest;
	m_hNotifyWnd = wnd;
	m_threadCount = threadCount > 0 ? threadCount : 1;
	m_status.store(CopyStatus::Preparing);
	// 创建准备线程
	AfxBeginThread(PreparationThreadProc, this);
	return true;
}

void CCopyController::PauseCopy() {
	if (m_status.load() == CopyStatus::Copying) {
		ResetEvent(m_hResumeEvent);
		m_status.store(CopyStatus::Paused);
	}
}

void CCopyController::ResumeCopy() {
	if (m_status.load() == CopyStatus::Paused) {
		SetEvent(m_hResumeEvent);
		m_status.store(CopyStatus::Copying);
	}
}

void CCopyController::CancelCopy() {
	m_bCancelSignal.store(true);
	if (m_status.load() == CopyStatus::Paused) {
		ResumeCopy();
	}
	m_status.store(CopyStatus::Cancelling);
}

CopyStatus CCopyController::GetStatus() const {
	if (m_status.load() == CopyStatus::Copying && (m_sharedStats.completedFileCount + m_sharedStats.failedFileCount) == m_sharedStats.totalFileCount) {
		// 注意：这里有竞态条件风险，仅用于演示。
		// 更好的做法是在工作线程发现任务完成时发送消息通知UI。
		// const_cast<std::atomic<CopyStatus>&>(m_status).store(CopyStatus::Finished);
	}
	return m_status.load();
}

const SSharedStats& CCopyController::GetSharedStats() const {
	return m_sharedStats;
}

void CCopyController::_PreparationThread() {
	if (!_ScanFiles(m_srcPath, m_destPath)) {
		m_status.store(CopyStatus::Error);
		//todo: 通知UI准备失败
		//::PostMessage(m_hNotifyWnd, WM_COPY_PREPARATION_FAILED, 0, 0);
		return;
	}
	if (m_allFiles.empty()) {
		m_status.store(CopyStatus::Finished);
		//todo: 通知UI没有文件需要复制
		//::PostMessage(m_hNotifyWnd, WM_COPY_NO_FILES, 0, 0);
		return;
	}
	// 分配文件到线程
	_DistributeFiles();
	// 启动工作线程
	_LaunchWorkerThreads();
	m_status.store(CopyStatus::Copying);
}

bool CCopyController::_ScanFiles(const CString& source, const CString& dest) {
	DWORD fileAttributes = GetFileAttributes(source);
	if (fileAttributes == INVALID_FILE_ATTRIBUTES) {
		// 源路径不存在或无法访问
		return false;
	}
	if (fileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		// 是文件夹，确保目标路径也是文件夹
		if (!PathFileExists(dest)) { //如果路径无效
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
	++pSharedStats->activeCopyThreads;
	for (SFileInfo* pFile : *pThreadParam->pFilesToCopy) {
		WaitForSingleObject(pController->m_hResumeEvent, INFINITE);
		if (pController->m_bCancelSignal.load()) break;
		pFile->status.store(EFileStatus::InProgress);
		if (CopyFile(pFile->sourcePath, pFile->destPath, FALSE)) {
			pFile->status.store(EFileStatus::Completed);
			++pSharedStats->completedFileCount;
		}
		else {
			pFile->status.store(EFileStatus::Failed);
			++pSharedStats->failedFileCount;
			//todo: 记录日志文件复制失败
		}
	}
	--pSharedStats->activeCopyThreads;
	delete pThreadParam;
	return 0;
}