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

	// 在这里更新UI，比如设置进度条的最大值，显示总文件数等
	// m_progress.SetRange32(0, 100);
	// CString statusText;
	// statusText.Format(_T("共 %u 个文件，总大小 %.2f GB"), totalFiles, totalSize / (1024.0*1024.0*1024.0));
	// m_statusStatic.SetWindowText(statusText);

	return 0;
}

LRESULT CFileCopyToolv1Dlg::OnCopyComplete(WPARAM wParam, LPARAM lParam) {
	// wParam 可以用来传递任务的最终状态，比如 0=成功, 1=用户取消, 2=发生错误
	int finalStatus = static_cast<int>(wParam);

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
	// CHARFORMAT2 cf;
	// ZeroMemory(&cf, sizeof(CHARFORMAT2));
	// cf.cbSize = sizeof(CHARFORMAT2);
	// cf.dwMask = CFM_COLOR;
	// cf.crTextColor = color;

	// long nStart, nEnd;
	// m_logEdit.GetSel(nStart, nEnd); // 获取当前文本长度
	// m_logEdit.SetSel(nEnd, nEnd);   // 将光标移动到末尾
	// m_logEdit.SetSelectionCharFormat(cf); // 设置新文本的格式

	// CString logLine = pLogMsg->text + _T("\r\n");
	// m_logEdit.ReplaceSel(logLine); // 追加文本

	// 关键：释放后台线程分配的内存，防止内存泄漏！
	delete pLogMsg;

	return 0;
}

void CFileCopyToolv1Dlg::OnBnClickedButtonStartcopy() {
	CString srcPath, destPath;
	m_srcEdit.GetWindowText(srcPath);
	m_destEdit.GetWindowText(destPath);
	m_pCopyController->StartCopy(srcPath, destPath, this->m_hWnd, 8);
}