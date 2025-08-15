// FileCopyToolv1Dlg.h: 头文件
//

#pragma once
#include "CopyController.h"

#define WM_USER_PREPARATION_COMPLETE (WM_USER + 1) // 准备工作完成
#define WM_USER_COPY_COMPLETE        (WM_USER + 2) // 整个复制任务完成
#define WM_USER_LOG_MESSAGE          (WM_USER + 3) // 添加一条日志消息

// 定义日志消息的类型（用于设置颜色）
enum class ELogLevel {
	Info,
	Success,
	Warning,
	Error
};
// 这是我们用来通过WM_USER_LOG_MESSAGE传递日志信息的结构体
// 工作线程会在堆上 new 一个，然后把指针作为LPARAM发送
// UI线程收到后负责 delete
struct LogMessage {
	CStringW text;
	LogLevel level;
};
// CFileCopyToolv1Dlg 对话框
class CFileCopyToolv1Dlg : public CDialogEx {
	// 构造
public:
	CFileCopyToolv1Dlg(CWnd* pParent = nullptr);	// 标准构造函数

	// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_FILECOPYTOOLV1_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持

	// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedButtonScansrc();
	afx_msg void OnBnClickedButtonScandst();
	afx_msg LRESULT OnPreparationComplete(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnCopyComplete(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnLogMessage(WPARAM wParam, LPARAM lParam);
	CEdit m_srcEdit;
	CEdit m_destEdit;
	std::unique_ptr<CCopyController> m_pCopyController;
	afx_msg void OnBnClickedButtonStartcopy();
};
