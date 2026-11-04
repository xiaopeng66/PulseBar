#pragma once

//S3/S6: 超级悬停面板:悬停任务栏窗口时显示的多指标监控面板。
//头行(图钉可钉住) + 数值行(网速/CPU/GPU/温度/功耗) + 网速曲线 + CPU/GPU 曲线。
//点击右上角图钉可钉住(鼠标移开不消失),右键解除钉住并隐藏。
class CSpeedGraphTip
{
public:
    static CSpeedGraphTip& Instance();

    //每秒采样(任务栏窗口定时器调用,与可见性无关,保证弹出即有完整曲线)
    void AddSample(unsigned __int64 up_speed, unsigned __int64 down_speed, int cpu_usage, int gpu_usage);

    //显示面板(贴任务栏窗口上方;若太靠上则显示在下方)。已钉住时不移动位置。
    //dpi: 任务栏窗口的 DPI,面板与任务栏同倍率
    void Show(const CPoint& cursor_pos, const CRect& taskbar_wnd_rect, UINT dpi);
    //常规隐藏(鼠标移开):钉住时不隐藏
    void Hide();
    //强制隐藏(关闭开关/关闭任务栏窗口):解除钉住并隐藏
    void ForceHide();
    //任务栏窗口、面板和两者之间的短暂过渡区域都算悬停区域。
    bool IsCursorInHoverRegion(const CPoint& cursor_pos, const CRect& taskbar_wnd_rect) const;
    bool IsVisible() const { return m_hWnd != nullptr && IsWindowVisible(m_hWnd); }
    bool IsPinned() const { return m_pinned; }

    //在给定 DC 上绘制面板(供自绘窗口调用)
    void Draw(CDC& dc, const CRect& rect);

private:
    CSpeedGraphTip();
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    void CreateWindowIfNeeded();
    int CalcWindowHeight(int dpi) const;

    static const int SAMPLE_COUNT = 60;

    HWND m_hWnd{ nullptr };
    unsigned __int64 m_up[SAMPLE_COUNT]{};
    unsigned __int64 m_down[SAMPLE_COUNT]{};
    int m_cpu[SAMPLE_COUNT]{};      //CPU利用率历史(0-100)
    int m_gpu[SAMPLE_COUNT]{};      //GPU利用率历史(0-100)
    unsigned __int64 m_max_speed{ 64 * 1024 };  //网速曲线归一化峰值(自适应)
    UINT m_dpi{ 96 };               //面板 DPI(Show 时由任务栏窗口传入,绘制与命中一致使用)
    bool m_pinned{ false };         //钉住状态(会话级,不落盘)
};
