// FileCopyToolv1Dlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "FileCopyToolv1.h"
#include "FileCopyToolv1Dlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialogEx {
public:
	CAboutDlg();

	// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX) {}

void CAboutDlg::DoDataExchange(CDataExchange* pDX) {
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()

// CFileCopyToolv1Dlg 对话框

CFileCopyToolv1Dlg::CFileCopyToolv1Dlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_FILECOPYTOOLV1_DIALOG, pParent) {
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CFileCopyToolv1Dlg::DoDataExchange(CDataExchange* pDX) {
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT_SRCPATH, m_srcEdit);
	DDX_Control(pDX, IDC_EDIT_DSTPATH, m_destEdit);
	DDX_Control(pDX, IDC_STATIC_TOTALPROGRESS, m_totalProgressText);
	DDX_Control(pDX, IDC_STATIC_TOTALSPEED, m_totalSpeedText);
	DDX_Control(pDX, IDC_STATIC_ALREADYCOPY, m_alreadyCopyText);
	DDX_Control(pDX, IDC_STATIC_REMAINTIME, m_remainTimeText);
	DDX_Control(pDX, IDC_PROGRESS, m_progressBar);
	DDX_Control(pDX, IDC_LIST_PROCESS, m_threadListCtrl);
	DDX_Control(pDX, IDC_RICHEDIT_LOG, m_logRichEdit);
}

BEGIN_MESSAGE_MAP(CFileCopyToolv1Dlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON_SCANSRC, &CFileCopyToolv1Dlg::OnBnClickedButtonScansrc)
	ON_BN_CLICKED(IDC_BUTTON_SCANDST, &CFileCopyToolv1Dlg::OnBnClickedButtonScandst)
	ON_BN_CLICKED(IDC_BUTTON_STARTCOPY, &CFileCopyToolv1Dlg::OnBnClickedButtonStartcopy)
	ON_MESSAGE(WM_USER_PREPARATION_COMPLETE, &CFileCopyToolv1Dlg::OnPreparationComplete)
	ON_MESSAGE(WM_USER_COPY_COMPLETE, &CFileCopyToolv1Dlg::OnCopyComplete)
	ON_MESSAGE(WM_USER_LOG_MESSAGE, &CFileCopyToolv1Dlg::OnLogMessage)
	ON_MESSAGE(WM_USER_UPDATE_THREAD_STATUS, &CFileCopyToolv1Dlg::OnUpdateThreadStatus)
	ON_WM_TIMER()
END_MESSAGE_MAP()

// CFileCopyToolv1Dlg 消息处理程序

BOOL CFileCopyToolv1Dlg::OnInitDialog() {
	CDialogEx::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr) {
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty()) {
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// TODO: 在此添加额外的初始化代码
	m_pCopyController = std::make_unique<CCopyController>();
	// 设置扩展样式：显示网格线、整行选中
	m_threadListCtrl.SetExtendedStyle(m_threadListCtrl.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

	// 添加列标题
	m_threadListCtrl.InsertColumn(0, _T("线程ID"), LVCFMT_LEFT, 60);
	m_threadListCtrl.InsertColumn(1, _T("状态"), LVCFMT_LEFT, 80);
	m_threadListCtrl.InsertColumn(2, _T("当前文件"), LVCFMT_LEFT, 200);
	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CFileCopyToolv1Dlg::OnSysCommand(UINT nID, LPARAM lParam) {
	if ((nID & 0xFFF0) == IDM_ABOUTBOX) {
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else {
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CFileCopyToolv1Dlg::OnPaint() {
	if (IsIconic()) {
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else {
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CFileCopyToolv1Dlg::OnQueryDragIcon() {
	return static_cast<HCURSOR>(m_hIcon);
}

void CFileCopyToolv1Dlg::OnBnClickedButtonScansrc() {
	CFolderPickerDialog folderDlg(NULL, 0, this);
	folderDlg.m_ofn.lpstrTitle = _T("请选择一个目标文件夹");

	if (folderDlg.DoModal() == IDOK) {
		CString strFolderPath = folderDlg.GetPathName();
		m_srcEdit.SetWindowText(strFolderPath);
	}
}

void CFileCopyToolv1Dlg::OnBnClickedButtonScandst() {
	CFolderPickerDialog folderDlg(NULL, 0, this);
	folderDlg.m_ofn.lpstrTitle = _T("请选择一个目标文件夹");

	if (folderDlg.DoModal() == IDOK) {
		CString strFolderPath = folderDlg.GetPathName();
		m_destEdit.SetWindowText(strFolderPath);
	}
}

LRESULT CFileCopyToolv1Dlg::OnPreparationComplete(WPARAM wParam, LPARAM lParam) {
	// wParam 可以用来传递文件总数
	UINT totalFiles = static_cast<UINT>(wParam);
	// lParam 可以用来传递文件总大小
	ULONGLONG totalSize = static_cast<ULONGLONG>(lParam);
	if (totalSize >= 1024 * 1024 * 1024) {
		m_filesAllSizeText.Format(_T("%.2f GB"), totalSize / (1024.0 * 1024.0 * 1024.0));
	}
	else if (totalSize >= 1024 * 1024) {
		m_filesAllSizeText.Format(_T("%.2f MB"), totalSize / (1024.0 * 1024.0));
	}
	else if (totalSize >= 1024) {
		m_filesAllSizeText.Format(_T("%.2f KB"), totalSize / 1024.0);
	}
	else {
		m_filesAllSizeText.Format(_T("%u B"), totalSize);
	}

	m_progressBar.SetRange32(0, 100);
	CString statusText = _T("0 B/");
	statusText += m_filesAllSizeText;
	statusText += _T("  0/");
	CString fileCountText;
	fileCountText.Format(_T("%u个"), totalFiles);
	statusText += fileCountText;
	m_alreadyCopyText.SetWindowText(statusText);
	m_totalProgressText.SetWindowText(_T("0%"));
	return 0;
}

LRESULT CFileCopyToolv1Dlg::OnCopyComplete(WPARAM wParam, LPARAM lParam) {
	// wParam 可以用来传递任务的最终状态，比如 0=成功, 1=用户取消, 2=发生错误
	int finalStatus = static_cast<int>(wParam);
	//销毁定时器
	if (m_nTimerID != 0) {
		KillTimer(m_nTimerID);
		m_nTimerID = 0;
	}
	// 确保最后一次更新UI，显示100%
	OnTimer(1); // 手动调用一次，刷新最终状态

	switch (finalStatus) {
	case 0:
		AfxMessageBox(_T("所有文件复制完成！"));
		break;
	case 1:
		AfxMessageBox(_T("任务已被用户取消。"));
		break;
	case 2:
		AfxMessageBox(_T("复制过程中发生错误，请检查日志。"));
		break;
	}
	m_pCopyController->ResetStatus();
	// 恢复UI界面到初始状态（启用“开始”按钮等）
	// m_startButton.EnableWindow(TRUE);
	// m_pauseButton.EnableWindow(FALSE);
	// ...

	return 0;
}

LRESULT CFileCopyToolv1Dlg::OnLogMessage(WPARAM wParam, LPARAM lParam) {
	// 从lParam中安全地取回我们在后台线程 new 的 LogMessage 指针
	LogMessage* pLogMsg = reinterpret_cast<LogMessage*>(lParam);
	if (pLogMsg == nullptr) {
		return 1; // 错误
	}

	// 根据日志级别设置颜色
	COLORREF color = RGB(0, 0, 0); // 默认为黑色
	switch (pLogMsg->level) {
	case ELogLevel::Success:
		color = RGB(0, 128, 0); // 深绿色
		break;
	case ELogLevel::Warning:
		color = RGB(255, 165, 0); // 橙色
		break;
	case ELogLevel::Error:
		color = RGB(255, 0, 0); // 红色
		break;
	case ELogLevel::Info:
	default:
		color = RGB(0, 0, 0); // 黑色
		break;
	}

	// --- 向RichEdit控件追加带颜色的文本 ---
	CHARFORMAT2 cf;
	ZeroMemory(&cf, sizeof(CHARFORMAT2));
	cf.cbSize = sizeof(CHARFORMAT2);
	cf.dwMask = CFM_COLOR;
	cf.crTextColor = color;

	long nStart, nEnd;
	m_logRichEdit.GetSel(nStart, nEnd); // 获取当前文本长度
	m_logRichEdit.SetSel(nEnd, nEnd);   // 将光标移动到末尾
	m_logRichEdit.SetSelectionCharFormat(cf); // 设置新文本的格式

	CString logLine = pLogMsg->text + _T("\r\n");
	m_logRichEdit.ReplaceSel(logLine); // 追加文本

	// 关键：释放后台线程分配的内存，防止内存泄漏！
	delete pLogMsg;

	return 0;
}

LRESULT CFileCopyToolv1Dlg::OnUpdateThreadStatus(WPARAM wParam, LPARAM lParam) {
	SThreadStatusInfo* pInfo = reinterpret_cast<SThreadStatusInfo*>(lParam);
	if (pInfo) {
		// threadId是从1开始的，而列表索引是从0开始的
		int itemIndex = static_cast<int>(pInfo->threadId) - 1;

		if (itemIndex >= 0 && itemIndex < m_threadListCtrl.GetItemCount()) {
			m_threadListCtrl.SetItemText(itemIndex, 1, pInfo->statusText);
			m_threadListCtrl.SetItemText(itemIndex, 2, pInfo->currentFile);
		}
		delete pInfo; // 释放后台线程分配的内存
	}
	return 0;
}

void CFileCopyToolv1Dlg::OnTimer(UINT_PTR nIDEvent) {
	if (nIDEvent != m_nTimerID && nIDEvent != 1) {
		CDialogEx::OnTimer(nIDEvent);
	}
	if (!m_pCopyController) return;
	const SSharedStats& stats = m_pCopyController->GetSharedStats();
	ULONGLONG totalSize = stats.totalCopySize;
	ULONGLONG copiedSize = stats.totalBytesCopied;
	UINT totalFiles = stats.totalFileCount;
	UINT copiedFiles = stats.completedFileCount + stats.failedFileCount;
	int progress = 0;
	if (totalSize > 0) {
		progress = static_cast<int>((copiedSize * 100) / totalSize);
	}
	m_progressBar.SetPos(progress);
	CString progressText;
	progressText.Format(_T("%d%%"), progress);
	m_totalProgressText.SetWindowText(progressText);
	DWORD currentTick = GetTickCount();
	DWORD elapsedTime = currentTick - m_dwLastTickCount;
	if (elapsedTime > 0) {
		ULONGLONG deltaSize = copiedSize - m_ullLastCopiedSize;
		double speed = (deltaSize / static_cast<double>(elapsedTime)) * 1000.0;
		CString speedText;
		speedText.Format(_T("%.2f MB/s"), speed / (1024.0 * 1024.0));
		m_totalSpeedText.SetWindowText(speedText);
		if (speed > 0) {
			ULONGLONG remainingSize = totalSize - copiedSize;
			double remainingTimeSec = remainingSize / speed;
			CString timeText;
			timeText.Format(_T("%d s"), static_cast<int>(remainingTimeSec));
			m_remainTimeText.SetWindowText(timeText);
		}
		else {
			m_remainTimeText.SetWindowText(_T("N/A"));
		}
	}
	// 更新下一次计算的基准值
	m_dwLastTickCount = currentTick;
	m_ullLastCopiedSize = copiedSize;

	// 4. 更新已复制文本
	CString alreadyCopyText;
	alreadyCopyText.Format(_T("%s / %s (%u/%u个)"), FormatSize(copiedSize), FormatSize(totalSize), copiedFiles, totalFiles);
	m_alreadyCopyText.SetWindowText(alreadyCopyText);

	CDialogEx::OnTimer(nIDEvent);
}

void CFileCopyToolv1Dlg::InitThreadList(UINT threadCount) {
	m_threadListCtrl.DeleteAllItems(); // 清空旧数据
	for (UINT i = 0; i < threadCount; ++i) {
		CString threadIdStr;
		threadIdStr.Format(_T("%u"), i + 1);
		m_threadListCtrl.InsertItem(i, threadIdStr); // 插入新行，只填第一列
		m_threadListCtrl.SetItemText(i, 1, _T("等待中...")); // 设置第二列“状态”
		m_threadListCtrl.SetItemText(i, 2, _T("N/A"));      // 设置第三列“当前文件”
	}
}

void CFileCopyToolv1Dlg::OnBnClickedButtonStartcopy() {
	CString srcPath, destPath;
	m_srcEdit.GetWindowText(srcPath);
	m_destEdit.GetWindowText(destPath);
	UINT threadCount = 8; // 线程数可以从UI获取

	// 在启动后台任务前，先初始化好我们的“仪表盘”
	InitThreadList(threadCount);
	if (m_pCopyController->StartCopy(srcPath, destPath, this->m_hWnd, 8)) {
		m_dwLastTickCount = GetTickCount();
		m_ullLastCopiedSize = 0;
		m_nTimerID = SetTimer(1, 500, nullptr);
	}
}

CString CFileCopyToolv1Dlg::FormatSize(ULONGLONG size) {
	CString strSize;
	if (size >= 1024 * 1024 * 1024) { // GB
		strSize.Format(_T("%.2f GB"), size / (1024.0 * 1024.0 * 1024.0));
	}
	else if (size >= 1024 * 1024) { // MB
		strSize.Format(_T("%.2f MB"), size / (1024.0 * 1024.0));
	}
	else if (size >= 1024) { // KB
		strSize.Format(_T("%.2f KB"), size / 1024.0);
	}
	else { // B
		strSize.Format(_T("%llu B"), size);
	}
	return strSize;
}