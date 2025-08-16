#include "pch.h"
#include "CopyController.h"

// ������ļ�����ֵ����Ƭ��С
const ULONGLONG LARGE_FILE_THRESHOLD = 128 * 1024 * 1024; // 128 MB
const ULONGLONG CHUNK_SIZE = 32 * 1024 * 1024;             // 32 MB

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
		PostMessage(m_hNotifyWnd, WM_USER_COPY_COMPLETE, 2, 0);
		return;
	}
	if (m_allFiles.empty()) {
		m_status.store(ECopyStatus::Finished);
		PostMessage(m_hNotifyWnd, WM_USER_COPY_COMPLETE, 0, 0);
		return;
	}
	// �����ļ����߳�
	//_DistributeFiles();
	_GenerateTasks();
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
	} else {
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
		} else {
			SFileInfo fileInfo;
			fileInfo.sourcePath = sourceFilePath;
			fileInfo.destPath = destFilePath;
			fileInfo.fileSize = finder.GetLength();
			m_allFiles.push_back(fileInfo);
		}
	}
	finder.Close();
}

void CCopyController::_GenerateTasks() {
	for (const auto& fileInfo : m_allFiles) {
		if (fileInfo.fileSize > LARGE_FILE_THRESHOLD) {
			// --- ���ļ������߼�����Ƭ ---

			// 1. Ԥ�ȴ���Ŀ���ļ����趨��С
			HANDLE hFile = CreateFile(fileInfo.destPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			if (hFile == INVALID_HANDLE_VALUE) {
				// ������󣬿��Լ�¼��־������
				continue;
			}
			LARGE_INTEGER li;
			li.QuadPart = fileInfo.fileSize;
			SetFilePointerEx(hFile, li, NULL, FILE_BEGIN);
			SetEndOfFile(hFile);
			CloseHandle(hFile);

			SChunkFileInfo* chunkFileInfo = new SChunkFileInfo;

			// 2. ������Ƭ�����������
			for (ULONGLONG offset = 0; offset < fileInfo.fileSize; offset += CHUNK_SIZE) {
				ULONGLONG currentChunkSize = min(CHUNK_SIZE, fileInfo.fileSize - offset);
				++chunkFileInfo->remainCopiedCount;
				m_taskQueue.Push({ ETaskType::FILE_CHUNK,
					fileInfo.sourcePath,
					fileInfo.destPath,
					fileInfo.fileSize,
					offset,
					currentChunkSize,
					chunkFileInfo });
			}
		} else {
			// --- С�ļ������߼����������� ---
			m_taskQueue.Push({ ETaskType::WHOLE_FILE, fileInfo.sourcePath, fileInfo.destPath, fileInfo.fileSize });
		}
	}
	// ֪ͨ���У����������Ѿ��������
	m_taskQueue.Finished();
}

bool CCopyController::_CopyFileChunk(const SCopyTask& task, SSharedStats* pSharedStats) {
	HANDLE hSource = CreateFile(task.sourcePath,
		GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (hSource == INVALID_HANDLE_VALUE) {
		return false;
	}
	HANDLE hDest = CreateFile(task.destPath,
		GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, NULL);
	if (hDest == INVALID_HANDLE_VALUE) {
		CloseHandle(hSource);
		return false;
	}
	LARGE_INTEGER sourceOffset, destOffset;
	sourceOffset.QuadPart = task.offset;
	destOffset.QuadPart = task.offset;

	if (!SetFilePointerEx(hSource,
		sourceOffset, NULL, FILE_BEGIN) || !SetFilePointerEx(hDest, destOffset, NULL, FILE_BEGIN)) {
		CloseHandle(hSource);
		CloseHandle(hDest);
		return false;
	}
	const DWORD bufferSize = 65536; // 64KB
	std::vector<char> buffer(bufferSize);
	ULONGLONG bytesRemaining = task.chunkSize;
	BOOL bSuccess = TRUE;
	while (bytesRemaining > 0) {
		DWORD bytesToRead = static_cast<DWORD>(min((ULONGLONG)bufferSize, bytesRemaining));
		DWORD bytesRead, bytesWritten;

		if (!ReadFile(hSource, buffer.data(), bytesToRead, &bytesRead, NULL) || bytesRead == 0) {
			bSuccess = FALSE;
			break;
		}
		if (!WriteFile(hDest, buffer.data(), bytesRead, &bytesWritten, NULL) || bytesRead != bytesWritten) {
			bSuccess = FALSE;
			break;
		}

		pSharedStats->totalBytesCopied += bytesWritten;
		bytesRemaining -= bytesWritten;
	}
	--task.chunkFileInfo->remainCopiedCount;
	CloseHandle(hSource);
	CloseHandle(hDest);
	return true;
}

void CCopyController::_LaunchWorkerThreads() {
	for (UINT i = 0; i < m_threadCount; ++i) {
		SThreadParam* pParam = new SThreadParam();
		pParam->threadId = i + 1;
		pParam->pController = this;
		m_threadParams.push_back(pParam);
		m_threads.push_back(AfxBeginThread(CopyWorkerThreadProc, pParam));
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
	SSharedStats* pSharedStats = pController->GetSharedStatsPtr();
	HWND hNotifyWnd = pController->GetNotifyWnd();
	UINT threadId = pThreadParam->threadId;
	++pSharedStats->activeCopyThreads;
	CString logMsg;
	logMsg.Format(_T("�߳�%u������"), threadId);

	LogMessage* pLogStart = new LogMessage{ logMsg, ELogLevel::Info };
	PostMessage(hNotifyWnd, WM_USER_LOG_MESSAGE, 0, reinterpret_cast<LPARAM>(pLogStart));

	SCopyTask currentTask;
	while (pController->m_taskQueue.Pop(currentTask)) {
		WaitForSingleObject(pController->GetResumeEvent(), INFINITE);
		if (*pController->GetCancelSignalPtr()) break;

		CString fileName = PathFindFileName(currentTask.sourcePath);
		SThreadStatusInfo* pStatusInfo = new SThreadStatusInfo{ threadId, _T("������"), fileName };
		PostMessage(hNotifyWnd, WM_USER_UPDATE_THREAD_STATUS, 0, reinterpret_cast<LPARAM>(pStatusInfo));

		BOOL bSuccess = FALSE;
		if (currentTask.type == ETaskType::WHOLE_FILE) {
			// --- ����С�ļ� ---
			SCopyProgressContext progressCtx;
			progressCtx.pSharedStats = pSharedStats;
			progressCtx.lastCopied = 0;
			bSuccess = CopyFileEx(currentTask.sourcePath, currentTask.destPath,
				(LPPROGRESS_ROUTINE)CopyProgressRoutine, &progressCtx,
				pController->GetCancelSignalPtr(), 0);

			if (bSuccess) {
				++pSharedStats->completedFileCount;
				LogMessage* pLogSuccess = new LogMessage{ fileName + _T(" ���Ƴɹ���"), ELogLevel::Success };
				PostMessage(hNotifyWnd, WM_USER_LOG_MESSAGE, 0, reinterpret_cast<LPARAM>(pLogSuccess));
			} else {
				// �������Ϊȡ����ʧ�ܣ�������ʧ��ͳ��
				if (!(*pController->GetCancelSignalPtr())) {
					++pSharedStats->failedFileCount;
					LogMessage* pLogError = new LogMessage{ fileName + _T(" ����ʧ�ܡ�"), ELogLevel::Error };
					PostMessage(hNotifyWnd, WM_USER_LOG_MESSAGE, 0, reinterpret_cast<LPARAM>(pLogError));
				}
			}
		} else if (currentTask.type == ETaskType::FILE_CHUNK) {
			// --- ������ļ��� ---
			bSuccess = _CopyFileChunk(currentTask, pSharedStats);
			if (bSuccess && currentTask.chunkFileInfo->remainCopiedCount.load() == 0) {
				++pSharedStats->completedFileCount;
				delete currentTask.chunkFileInfo;
				LogMessage* pLogSuccess = new LogMessage{ fileName + _T(" ���Ƴɹ���"), ELogLevel::Success };
				PostMessage(hNotifyWnd, WM_USER_LOG_MESSAGE, 0, reinterpret_cast<LPARAM>(pLogSuccess));
			}
			if (bSuccess == false) {
				//delete currentTask.chunkFileInfo;
				LogMessage* pLogError = new LogMessage{ fileName + _T(" ����ʧ�ܡ�"), ELogLevel::Error };
				PostMessage(hNotifyWnd, WM_USER_LOG_MESSAGE, 0, reinterpret_cast<LPARAM>(pLogError));
			}
		}
	}
	SThreadStatusInfo* pStatusDone = new SThreadStatusInfo{ threadId, _T("���"), _T("") };
	PostMessage(hNotifyWnd, WM_USER_UPDATE_THREAD_STATUS, 0, reinterpret_cast<LPARAM>(pStatusDone));

	// ��������һ����ɵ��̣߳����������Ϣ
	if (pSharedStats->activeCopyThreads.fetch_sub(1) == 1) {
		int finalStatus = *pController->GetCancelSignalPtr() ? 1 : 0; // 1=ȡ��, 0=�ɹ�
		PostMessage(hNotifyWnd, WM_USER_COPY_COMPLETE, finalStatus, 0);
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