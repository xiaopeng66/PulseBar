//这是用于Tab标签中的子对话框类
#pragma once
#include "BaseDialog.h"

//由资源模板生成的控件布局表项（DLU 坐标）
struct TemplateControl
{
    int x;
    int y;
    int cx;
    int cy;
    bool visible;       //模板中默认是否可见
    int section;        //所属分区(0/1/2)，由模板中的 y 坐标决定
};

class CTabDlg : public CBaseDialog
{
    DECLARE_DYNAMIC(CTabDlg)
public:
    CTabDlg(UINT nIDTemplate, CWnd *pParent = NULL);
    ~CTabDlg();
    virtual BOOL PreTranslateMessage(MSG* pMsg);
    virtual BOOL OnInitDialog();
    CWnd* GetParentWindow();
    void SetScrollbarInfo(int nPage, int nMax);
    virtual void OnTabEntered() {}      //当标签切换到当前窗口时被调用
    virtual void OnOK();
    void SetScrollEnable(bool enable) { m_scroll_enable = enable; }
    virtual void SetControlMouseWheelEnable(bool enable) {}     //在派生类中重写此函数以设置控件是否允许响应鼠标滚轮
    virtual void OnSettingsApplied() {}   //点击了“应用”按钮，应用完设置之后被调用

    bool IsScrollBarVisible();    //滚动条是否可见

private:
    void ScrollWindowSimple(int step, bool absolute = false);
    void MoveChildControls(int offset);

protected:
    void EnableDlgCtrl(UINT id, bool enable);
    void ShowDlgCtrl(UINT id, bool show);
    void RepaintAfterMove();      //移动控件后擦除重绘（滚动时使用，避免闪烁）
    void RepaintPageSwitch();     //切换页面后彻底重绘（隐藏-擦除-重显，清除上一页残留）
    int EditControlHeight();      //单行编辑框的最佳高度（按字体高度计算，文字才不会贴顶）

protected:
    CWnd* m_pParent;
    bool m_scroll_enable{ false };
    int m_last_pos{};
    int m_edit_height{ -1 };    //EditControlHeight 的缓存（同一 DPI 下不变）

protected:
    // CWnd的虚方法，声明在afxwin.h，实现在wincore.cpp，在WM_SIZE消息处理时被CWnd调用
    // 内部在一些判断之后使用CWnd持有的CMFCDynamicLayout指针执行m_pDynamicLayout->Adjust()
    virtual void ResizeDynamicLayout() override final;  // 这里重写是为了改变调用时机，从默认的窗口OnSize之前改为父窗口OnSize之后

public:
    DECLARE_MESSAGE_MAP()
    afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnSize(UINT nType, int cx, int cy);
};

