// FileCopyToolv1Dlg.h: 头文件
//

#pragma once
#include "CopyController.h"
#include "common.h"

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
	afx_msg LRESULT OnUpdateThreadStatus(WPARAM wParam, LPARAM lParam);
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	void InitThreadList(UINT threadCount);
	CEdit m_srcEdit;
	CEdit m_destEdit;
	std::unique_ptr<CCopyController> m_pCopyController;
	afx_msg void OnBnClickedButtonStartcopy();
	CStatic m_totalProgressText;
	CStatic m_totalSpeedText;
	CStatic m_alreadyCopyText;
	CStatic m_remainTimeText;
	CProgressCtrl m_progressBar;
	CListCtrl m_threadListCtrl;
	CRichEditCtrl m_logRichEdit;
	CString m_filesAllSizeText;

private:
	UINT_PTR m_nTimerID;           // 用于标识我们的定时器
	DWORD    m_dwLastTickCount;    // 上次计算时的时间戳
	ULONGLONG m_ullLastCopiedSize; // 上次计算时已复制的总字节数

	CString FormatSize(ULONGLONG size); //用于格式化字节大小
};
