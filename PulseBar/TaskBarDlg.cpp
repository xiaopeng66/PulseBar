// TaskBarDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "PulseBar.h"
#include "TaskBarDlg.h"
#include "afxdialogex.h"
#include "PulseBarDlg.h"
#include "WindowsSettingHelper.h"
#include "WIC.h"
#include "Nullable.hpp"
#include "DrawCommonFactory.h"
#include "WindowsWebExperienceDetector.h"
#include "TaskbarHelper.h"
#include "SpeedGraphTip.h"

#ifdef DEBUG
// DX调试信息捕获
#include "dxgi1_3.h"
#include "DXProgrammableCapture.h"
#endif

// CTaskBarDlg 对话框

IMPLEMENT_DYNAMIC(CTaskBarDlg, CDialogEx)

CTaskBarDlg::CTaskBarDlg(CWnd* pParent /*=NULL*/)
    : CDialogEx(IDD_TASK_BAR_DIALOG, pParent)
{
    static UINT_PTR next_instance_token{};
    m_instance_token = ++next_instance_token;
}

CTaskBarDlg::~CTaskBarDlg()
{
    for (auto iter = m_map_history_data.begin(); iter != m_map_history_data.end(); ++iter)
    {
        iter->second.clear();
    }
    //兜底：OnDestroy 已在窗口有效时释放 DC，正常路径不会走到这里。
    if (m_pDC != nullptr && ::IsWindow(m_hWnd))
    {
        ::ReleaseDC(m_hWnd, m_pDC->GetSafeHdc());
        m_pDC = nullptr;
    }
}

void CTaskBarDlg::OnDestroy()
{
    //OnInitDialog 获取的窗口 DC 必须在窗口销毁前释放，此时 m_hWnd 仍然有效。
    if (m_pDC != nullptr)
    {
        ::ReleaseDC(m_hWnd, m_pDC->GetSafeHdc());
        m_pDC = nullptr;
    }
    CDialogEx::OnDestroy();
}

void CTaskBarDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CTaskBarDlg, CDialogEx)
    ON_WM_DESTROY()
    ON_WM_RBUTTONUP()
    ON_WM_INITMENU()
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONDBLCLK()
    ON_WM_TIMER()
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_CLOSE()
    ON_WM_LBUTTONUP()
    ON_MESSAGE(WM_EXITMENULOOP, &CTaskBarDlg::OnExitmenuloop)
    ON_MESSAGE(WM_TABLET_QUERYSYSTEMGESTURESTATUS, &CTaskBarDlg::OnTabletQuerysystemgesturestatus)
    ON_WM_MOUSEWHEEL()
END_MESSAGE_MAP()

// CTaskBarDlg 消息处理程序

void CTaskBarDlg::ShowInfo(CDC* pDC)
{
    HWND current_hwnd = this->GetSafeHwnd();
    if (current_hwnd == NULL || !IsWindow(this->GetSafeHwnd()))
        return;
    if (m_rect.IsRectEmpty() || m_rect.IsRectNull())
        return;
    DrawCommonHelper::BeginFrameSubmit();
    if (m_supported_render_enums.IsD2D1Enabled())
    {
        auto last_update_layered_window_error =
            theApp.m_taskbar_data.update_layered_window_error_code;
        if (last_update_layered_window_error)
        {
            m_supported_render_enums.DisableD2D1();
            if (!m_supported_render_enums.IsD2D1WithDCompositionEnabled())
            {
                m_supported_render_enums.EnableDefaultOnly();
                LogWin32ApiErrorMessage(last_update_layered_window_error);
                CString error_info{};
                error_info.Format(
                    _T("Call UpdateLayeredWindowIndirect failed. Use GDI render instead. Error code = %ld."),
                    last_update_layered_window_error);
                CCommon::WriteLog(error_info, theApp.m_log_path.c_str());
                // 禁用D2D
                theApp.m_taskbar_data.disable_d2d = true;
                // 展示错误信息
                ::MessageBox(NULL, CCommon::LoadText(IDS_UPDATE_TASKBARDLG_FAILED_TIP), NULL, MB_OK | MB_ICONWARNING);
            }
        }
    }
    auto render_type = m_supported_render_enums.GetAutoFitEnum();
    CRect draw_rect{m_rect}; // 绘图的矩形区域
    draw_rect.MoveToXY(0, 0);

#ifdef DEBUG
    Microsoft::WRL::ComPtr<IDXGraphicsAnalysis> p_dxgi_analysis{};
#endif
    // 初始化DrawBuffer和DrawCommon栈内存
    AllInvolvedDrawCommonObjectsStorage all_involved_draw_common_objects{};
    IDrawCommon* draw_common_interface{nullptr};
    std::tie(std::ignore, draw_common_interface) =
        GetInterfaceFromAllInvolvedDrawCommonObjects(
            all_involved_draw_common_objects,
            render_type,
            {{DrawCommonHelper::RenderType::DEFAULT,
              [&](IDrawBuffer* p_draw_buffer_interface, IDrawCommon* p_draw_common_interface)
              {
                  auto p_draw_buffer = static_cast<CDrawDoubleBufferCached*>(p_draw_buffer_interface);
                  auto p_draw_common = static_cast<CDrawCommon*>(p_draw_common_interface);

                  // 必须先于绘图对象构造
                  p_draw_buffer = all_involved_draw_common_objects.EmplaceDrawBuffer<CDrawDoubleBufferCached>(pDC, draw_rect);
                  //构造完成立即接管；Create 抛异常时仍能正确析构。
                  p_draw_common = all_involved_draw_common_objects.EmplaceDrawCommon<CDrawCommon>();
                  p_draw_common->Create(p_draw_buffer->GetMemDC(), nullptr);
                  //主题面板/背景图模式:绘制圆角/渐变/背景图,否则普通填充背景色
                  if (theApp.m_taskbar_data.IsTaskbarThemePanel())
                  {
                      //面板只画圆角路径,必须先把缓存DC整幅清成色键色:
                      //否则冷启动的黑色位图/上一帧残留会透过半透明面板(表现为面板下垫着一层),
                      //圆角外的区域也无法等于色键色而保持不透明
                      COLORREF base_color = theApp.m_taskbar_data.back_color;
                      if (theApp.m_taskbar_data.transparent_color != 0 && theApp.m_taksbar_transparent_color_enable)
                          base_color = theApp.m_taskbar_data.transparent_color;
                      p_draw_common->FillRect(draw_rect, base_color);
                      p_draw_common->DrawThemeBackground(draw_rect, DPI(theApp.m_taskbar_data.tb_corner_radius));
                  }
                  else
                      p_draw_common->FillRect(draw_rect, theApp.m_taskbar_data.back_color); // 填充背景色
                  p_draw_common->SetFont(m_bold_font.m_hObject ? &m_bold_font : &m_font);
                  p_draw_common->SetBackColor(theApp.m_taskbar_data.back_color);
              }},
             {DrawCommonHelper::RenderType::D2D1_WITH_DCOMPOSITION,
              [&](IDrawBuffer* p_draw_buffer_interface, IDrawCommon* p_draw_common_interface)
              {
                  auto p_draw_buffer = static_cast<CTaskBarDlgDrawBufferUseDComposition*>(p_draw_buffer_interface);
                  auto p_draw_common = static_cast<CTaskBarDlgDrawCommon*>(p_draw_common_interface);

                  auto& ref_d2d1_app_support = theApp.m_d2d_taskbar_draw_common_support.Get();
                  m_d2d1_device_context_support.Get().SetWorkingDevice(
                      ref_d2d1_app_support.GetDCompositionDevice(),
                      ref_d2d1_app_support.GetD3D10Device1(),
                      m_hWnd);
                  // 这里与上面相反，是先构造DrawCommon再构造Buffer
                  // 这里构造绘图对象
                  p_draw_common = all_involved_draw_common_objects.EmplaceDrawCommon<CTaskBarDlgDrawCommon>();
                  D2D1_SIZE_U d2d_size;
                  d2d_size.width = draw_rect.Width();
                  d2d_size.height = draw_rect.Height();
                  p_draw_common->Create(
                      this->m_taskbar_draw_common_window_support.Get(),
                      this->m_d2d1_device_context_support.Get(),
                      d2d_size);
                  // 仅透明时，且UpdateLayeredWindowIndirect失败时，启用此渲染器，默认初始化为全黑，alpha=1
                  p_draw_common->FillRect(draw_rect, 0x00000000, 1);
                  p_draw_common->SetFont(m_bold_font.m_hObject ? &m_bold_font : &m_font);
                  p_draw_common->SetBackColor(theApp.m_taskbar_data.back_color);
                  // 构造buffer
                  p_draw_buffer = all_involved_draw_common_objects.EmplaceDrawBuffer<CTaskBarDlgDrawBufferUseDComposition>(
                      this->m_d2d1_device_context_support.Get(), current_hwnd);
              }},
             {DrawCommonHelper::RenderType::D2D1,
              [&](IDrawBuffer* p_draw_buffer_interface, IDrawCommon* p_draw_common_interface)
              {
                  auto p_draw_buffer = static_cast<CTaskBarDlgDrawBuffer*>(p_draw_buffer_interface);
                  auto p_draw_common = static_cast<CTaskBarDlgDrawCommon*>(p_draw_common_interface);

                  auto& ref_d2d1_app_support = theApp.m_d2d_taskbar_draw_common_support.Get();
                  m_d2d1_device_context_support.Get().SetWorkingDevice(ref_d2d1_app_support.GetD3D10Device1());
                  // 这里与上面相反，是先构造DrawCommon再构造Buffer
                  // 这里构造绘图对象
                  p_draw_common = all_involved_draw_common_objects.EmplaceDrawCommon<CTaskBarDlgDrawCommon>();
                  D2D1_SIZE_U d2d_size;
                  d2d_size.width = draw_rect.Width();
                  d2d_size.height = draw_rect.Height();
                  p_draw_common->Create(
                      this->m_taskbar_draw_common_window_support.Get(),
                      this->m_d2d1_device_context_support.Get(),
                      d2d_size);
                  // 仅透明时启用此渲染器，默认初始化为全黑，alpha=1
                  p_draw_common->FillRect(draw_rect, 0x00000000, 1);
                  p_draw_common->SetFont(m_bold_font.m_hObject ? &m_bold_font : &m_font);
                  p_draw_common->SetBackColor(theApp.m_taskbar_data.back_color);
                  // 构造buffer
                  CSize draw_size{draw_rect.Width(), draw_rect.Height()};
                  p_draw_buffer = all_involved_draw_common_objects.EmplaceDrawBuffer<CTaskBarDlgDrawBuffer>(
                            this->m_taskbar_draw_common_window_support.Get(),
                            this->m_d2d1_device_context_support.Get(),
                            draw_size,
                            current_hwnd);

#ifdef DEBUG
                  DXGIGetDebugInterface1(0, IID_PPV_ARGS(&p_dxgi_analysis));
                  if (p_dxgi_analysis)
                  {
                      p_dxgi_analysis->BeginCapture();
                  }
#endif
              }}});
    IDrawCommon& draw = *draw_common_interface;
    // 绘制任务栏项目
    for (auto iter = m_item_widths.begin(); iter != m_item_widths.end(); ++iter)
    {
        CRect item_rect = m_item_rects[*iter];
        if (!item_rect.IsRectEmpty())
        {
            if (iter->IsPlugin())
                DrawPluginItem(draw, iter->PluginItem(), item_rect, iter->item_width.label_width, iter->is_double_line);
            else
                DrawDisplayItem(draw, iter->ItemType(), item_rect, iter->item_width.label_width, iter->is_double_line);
        }
    }

#ifdef DEBUG
    if (p_dxgi_analysis)
    {
        p_dxgi_analysis->EndCapture();
    }
#endif
    if (render_type == DrawCommonHelper::RenderType::DEFAULT)
        DrawCommonHelper::MarkFrameSubmitSucceeded();
}

namespace
{
    //硬件占用分档阈值：达到预警后数值改用主题的 warn 色，达到危险后改用 danger 色。
    constexpr int PERCENT_COLOR_WARN = 60;
    constexpr int PERCENT_COLOR_DANGER = 85;
    //无主题(经典)或自定义主题未提供警示色时的兜底配色。
    constexpr COLORREF DEFAULT_WARN_COLOR = RGB(230, 152, 40);
    constexpr COLORREF DEFAULT_DANGER_COLOR = RGB(226, 64, 52);

    //返回该显示项的占用百分比；非百分比型项目返回 -1。
    int GetItemUsagePercent(DisplayItem type)
    {
        switch (type)
        {
        case TDI_CPU:
            return theApp.m_cpu_usage;
        case TDI_MEMORY:
            return theApp.m_memory_usage;
        case TDI_GPU_USAGE:
            return theApp.m_gpu_usage;
        case TDI_DEDICATED_GPU_USAGE:
            return theApp.m_dedicated_gpu_memory_pct;
        case TDI_HDD_USAGE:
            return theApp.m_hdd_usage;
        default:
            return -1;
        }
    }
}


void CTaskBarDlg::DrawDisplayItem(IDrawCommon& drawer, DisplayItem type, CRect rect, int label_width, bool vertical)
{
    //设置要绘制的文本颜色
    COLORREF label_color{};
    COLORREF text_color{};
    //主题启用时优先使用主题的标签/数值配色(用户指定每项颜色除外)
    const TaskbarThemeInfo* cur_theme = GetActiveTaskbarTheme();
    bool theme_active = (cur_theme != nullptr && theApp.m_taskbar_data.tb_theme != 0
        && !theApp.m_taskbar_data.specify_each_item_color);
    if (theme_active)
    {
        label_color = cur_theme->label;
        text_color = cur_theme->value;
    }
    else if (theApp.m_taskbar_data.specify_each_item_color)
    {
        label_color = theApp.m_taskbar_data.text_colors[type].label;
        text_color = theApp.m_taskbar_data.text_colors[type].value;
    }
    else if (!theApp.m_taskbar_data.text_colors.empty())
    {
        //UI优化:标签用 label 色,数值用 value 色(原版两者同用 label 色,数值不突出)
        label_color = theApp.m_taskbar_data.text_colors.begin()->second.label;
        text_color = theApp.m_taskbar_data.text_colors.begin()->second.value;
        if (text_color == 0)
            text_color = label_color;
        //标签与数值同色时,标签调暗 25% 形成层级
        if (label_color == text_color)
        {
            label_color = RGB(GetRValue(label_color) * 3 / 4,
                              GetGValue(label_color) * 3 / 4,
                              GetBValue(label_color) * 3 / 4);
        }
    }
    else
    {
        //未设置任何颜色时的默认配色:标签浅灰、数值白色,层级更清晰
        label_color = RGB(190, 190, 190);
        text_color = RGB(255, 255, 255);
    }

    //硬件占用按高低分档：只改数值颜色，标签保持主题配色不变；
    //用户显式指定了逐项颜色时不覆盖其选择。
    if (theApp.m_taskbar_data.percent_color_enabled
        && !theApp.m_taskbar_data.specify_each_item_color)
    {
        const int usage = GetItemUsagePercent(type);
        if (usage >= PERCENT_COLOR_DANGER)
            text_color = (cur_theme != nullptr) ? cur_theme->danger : DEFAULT_DANGER_COLOR;
        else if (usage >= PERCENT_COLOR_WARN)
            text_color = (cur_theme != nullptr) ? cur_theme->warn : DEFAULT_WARN_COLOR;
    }

    //设置标签和数值的矩形区域
    CRect rect_label{ rect };
    CRect rect_value{ rect };
    if (!vertical)
    {
        rect_label.right = rect_label.left + label_width;
        rect_value.left = rect_label.right;
    }
    else if (label_width != 0)
    {
        rect_label.bottom = rect_label.top + (rect_label.Height() / 2);
        rect_value.top = rect_label.bottom;
    }

    // 绘制状态条
    if (type == TDI_CPU || type == TDI_MEMORY || type == TDI_GPU_USAGE || type == TDI_CPU_TEMP
        || type == TDI_GPU_TEMP || type == TDI_HDD_TEMP || type == TDI_MAIN_BOARD_TEMP || type == TDI_HDD_USAGE
        || type == TDI_DEDICATED_GPU_USAGE
        || type == TDI_UP || type == TDI_DOWN || type == TDI_TOTAL_SPEED/* ||type==TDI_CPU_FREQ*/)
    {
        int figure_value{};
        switch (type)
        {
        case TDI_CPU:
            figure_value = theApp.m_cpu_usage;
            break;
        case TDI_MEMORY:
            figure_value = theApp.m_memory_usage;
            break;
        case TDI_GPU_USAGE:
            figure_value = theApp.m_gpu_usage;
            break;
        case TDI_DEDICATED_GPU_USAGE:
            figure_value = theApp.m_dedicated_gpu_memory_pct;
            break;
        case TDI_CPU_TEMP:
            figure_value = theApp.m_cpu_temperature;
            break;
        case TDI_GPU_TEMP:
            figure_value = theApp.m_gpu_temperature;
            break;
        case TDI_HDD_TEMP:
            figure_value = theApp.m_hdd_temperature;
            break;
        case TDI_MAIN_BOARD_TEMP:
            figure_value = theApp.m_main_board_temperature;
            break;
        case TDI_HDD_USAGE:
            figure_value = theApp.m_hdd_usage;
            break;
        //case TDI_CPU_FREQ:
        //    figure_value = theApp.m_cpu_freq;
        //    break;
        case TDI_UP:
            figure_value = CalculateNetspeedPercent(theApp.m_out_speed);
            break;
        case TDI_DOWN:
            figure_value = CalculateNetspeedPercent(theApp.m_in_speed);
            break;
        case TDI_TOTAL_SPEED:
            figure_value = CalculateNetspeedPercent(theApp.m_in_speed + theApp.m_out_speed);
            break;
        default:
            break;
        }

        if ((type != TDI_UP && type != TDI_DOWN && type != TDI_TOTAL_SPEED) && theApp.m_taskbar_data.show_status_bar
            || (type == TDI_UP || type == TDI_DOWN || type == TDI_TOTAL_SPEED) && theApp.m_taskbar_data.show_netspeed_figure)
        {
            if (theApp.m_taskbar_data.cm_graph_type)
            {
                AddHisToList(type, figure_value);
                TryDrawGraph(drawer, rect, type);
            }
            else
            {
                TryDrawStatusBar(drawer, rect, figure_value);
            }
        }
    }

    //绘制标签
    if (label_width > 0)
    {
        wstring str_label = theApp.m_taskbar_data.disp_str.GetConst(type);
        drawer.DrawWindowText(rect_label, str_label.c_str(), label_color, (vertical ? IDrawCommon::Alignment::CENTER : IDrawCommon::Alignment::LEFT));
    }

    //绘制数值
    IDrawCommon::Alignment value_alignment{ theApp.m_taskbar_data.value_right_align ? IDrawCommon::Alignment::RIGHT : IDrawCommon::Alignment::LEFT };      //数值的对齐方式
    if (vertical)
        value_alignment = IDrawCommon::Alignment::CENTER;
    CString str_value = CommonDisplayItem(type).GetItemValueText(false);
    drawer.DrawWindowText(rect_value, str_value, text_color, value_alignment);
}

void CTaskBarDlg::DrawPluginItem(IDrawCommon& drawer, IPluginItem* item, CRect rect, int label_width, bool vertical)
{
    if (item == nullptr)
        return;

    //绘制资源占用图
    ITMPlugin* plugin = theApp.m_plugins.GetPluginByItem(item);
    if (theApp.m_taskbar_data.show_status_bar && plugin != nullptr && plugin->GetAPIVersion() >= 6)
    {
        if (item->IsDrawResourceUsageGraph())
        {
             int figure_value = item->GetResourceUsageGraphValue() * 100;
            //横向滚动图
            if (theApp.m_taskbar_data.cm_graph_type)
            {
                AddHisToList(item, figure_value);
                TryDrawGraph(drawer, rect, item);
            }
            //柱状图
            else
            {
                TryDrawStatusBar(drawer, rect, figure_value);
            }
        }
    }

    //设置要绘制的文本颜色
    COLORREF label_text_color{};
    COLORREF value_text_color{};
    //插件项与内置项共用同一套配色规则：启用主题时用主题配色，否则用逐项颜色，
    //都没有时给出一组可见的默认色。此前这里取的是 label 色且没有默认值，
    //插件文字在深色主题下会发黑。
    const TaskbarThemeInfo* cur_theme = GetActiveTaskbarTheme();
    const bool theme_active = (cur_theme != nullptr && theApp.m_taskbar_data.tb_theme != 0
        && !theApp.m_taskbar_data.specify_each_item_color);
    if (theme_active)
    {
        label_text_color = cur_theme->label;
        value_text_color = cur_theme->value;
    }
    else if (theApp.m_taskbar_data.specify_each_item_color)
    {
        label_text_color = theApp.m_taskbar_data.text_colors[item].label;
        value_text_color = theApp.m_taskbar_data.text_colors[item].value;
    }
    else if (!theApp.m_taskbar_data.text_colors.empty())
    {
        label_text_color = theApp.m_taskbar_data.text_colors.begin()->second.label;
        value_text_color = theApp.m_taskbar_data.text_colors.begin()->second.value;
        if (value_text_color == 0)
            value_text_color = label_text_color;
        if (label_text_color == value_text_color)
        {
            label_text_color = RGB(GetRValue(label_text_color) * 3 / 4,
                GetGValue(label_text_color) * 3 / 4, GetBValue(label_text_color) * 3 / 4);
        }
    }
    else
    {
        label_text_color = RGB(190, 190, 190);
        value_text_color = RGB(255, 255, 255);
    }
    //声明了资源占用图的插件项同样按占用高低分档变色，与内置项保持一致。
    if (theApp.m_taskbar_data.percent_color_enabled && !theApp.m_taskbar_data.specify_each_item_color
        && plugin != nullptr && item->IsDrawResourceUsageGraph())
    {
        const int usage = item->GetResourceUsageGraphValue() * 100;
        if (usage >= PERCENT_COLOR_DANGER)
            value_text_color = (cur_theme != nullptr) ? cur_theme->danger : DEFAULT_DANGER_COLOR;
        else if (usage >= PERCENT_COLOR_WARN)
            value_text_color = (cur_theme != nullptr) ? cur_theme->warn : DEFAULT_WARN_COLOR;
    }

    if (plugin != nullptr && plugin->GetAPIVersion() >= 2)
    {
        try
        {
            plugin->OnExtenedInfo(ITMPlugin::EI_DRAW_TASKBAR_WND, L"1");
        }
        catch (...)
        {
            LogPluginCallbackFailure(L"ITMPlugin::OnExtenedInfo");
        }
    }
    if (item->IsCustomDraw())
    {
        //根据背景色的亮度判断深色还是浅色模式
        const COLORREF& bk{ theApp.m_taskbar_data.back_color };
        int background_brightness{ (GetRValue(bk) + GetGValue(bk) + GetBValue(bk)) / 3 };
        //由插件自绘
        if (plugin != nullptr && plugin->GetAPIVersion() >= 2)
        {
            try
            {
                plugin->OnExtenedInfo(ITMPlugin::EI_LABEL_TEXT_COLOR, std::to_wstring(label_text_color).c_str());
                plugin->OnExtenedInfo(ITMPlugin::EI_VALUE_TEXT_COLOR, std::to_wstring(value_text_color).c_str());
            }
            catch (...)
            {
                LogPluginCallbackFailure(L"ITMPlugin::OnExtenedInfo");
            }
        }
        drawer.SetTextColor(value_text_color);
        //插件自绘可能崩溃（/EHa 下 catch 能捕获）：只放弃这一项的绘制，不拖垮整帧
        try
        {
            //需要rtti
            if (typeid(drawer) == typeid(CDrawCommon))
            {
                auto* p_dc = static_cast<CDrawCommon&>(drawer).GetDC();
                item->DrawItem(p_dc->GetSafeHdc(), rect.left, rect.top, rect.Width(), rect.Height(), background_brightness < 128);
                p_dc->SelectObject(&m_font);
            }
            else if (typeid(drawer) == typeid(CTaskBarDlgDrawCommon))
            {
                auto& ref_d2d1_drawer = static_cast<CTaskBarDlgDrawCommon&>(drawer);
                ref_d2d1_drawer.ExecuteGdiOperation(rect,
                    [item, rect, background_brightness](HDC gdi_dc)
                    { item->DrawItem(gdi_dc,
                        rect.left,
                        rect.top,
                        rect.Width(),
                        rect.Height(),
                        background_brightness < 128); });
            }
            if (plugin != nullptr && plugin->GetAPIVersion() >= 8)
                item->DrawItemEx(&drawer, rect.left, rect.top, rect.Width(), rect.Height(), background_brightness < 128);
        }
        catch (...)
        {
            LogPluginCallbackFailure(L"IPluginItem::DrawItem");
        }
    }
    else
    {
        CRect rect_label, rect_value;
        rect_label = rect_value = rect;
        if (label_width > 0)
        {
            if (!vertical)
            {
                rect_label = rect_value = rect;
                rect_label.right = rect_label.left + label_width;
                rect_value.left = rect_label.right;
            }
            else
            {
                rect_label.bottom = rect_label.top + rect.Height() / 2;
                rect_value.top = rect_label.bottom;
            }
        }
        //画标签
        CString lable_text = theApp.m_taskbar_data.disp_str.GetConst(item).c_str();
        drawer.DrawWindowText(rect_label, lable_text, label_text_color, (vertical ? IDrawCommon::Alignment::CENTER : IDrawCommon::Alignment::LEFT));
        //画数值
        IDrawCommon::Alignment value_alignment{ theApp.m_taskbar_data.value_right_align ? IDrawCommon::Alignment::RIGHT : IDrawCommon::Alignment::LEFT };      //数值的对齐方式
        if (vertical)
            value_alignment = IDrawCommon::Alignment::CENTER;
        CString value_text;
        try
        {
            const wchar_t* plugin_text = item->GetItemValueText();
            value_text = plugin_text != nullptr ? plugin_text : L"";
        }
        catch (...)
        {
            LogPluginCallbackFailure(L"IPluginItem::GetItemValueText");
        }
        drawer.DrawWindowText(rect_value, value_text, value_text_color, value_alignment);
    }
}

void CTaskBarDlg::MoveWindow(CRect rect)
{
    if (IsWindow(GetSafeHwnd()))
    {
        ::MoveWindow(GetSafeHwnd(), rect.left, rect.top, rect.Width(), rect.Height(), TRUE);
    }
}

void CTaskBarDlg::DisableRenderFeatureIfNecessary(CSupportedRenderEnums& ref_supported_render_enums)
{
    bool is_transparent = theApp.m_taskbar_data.IsTaskbarTransparent();
    // UpdateLayeredWindowIndirect失败则禁用D2D
    if (theApp.m_taskbar_data.update_layered_window_error_code)
    {
        ref_supported_render_enums.DisableD2D1();
    }
    // 不符合条件则启用Default（MFC）
    if (!is_transparent || theApp.m_taskbar_data.auto_set_background_color || theApp.m_taskbar_data.disable_d2d
        || theApp.m_taskbar_data.IsTaskbarThemePanel())     //主题面板/背景图模式走GDI渲染(便于绘制圆角/渐变/图片)
    {
        ref_supported_render_enums.EnableDefaultOnly();
    }
}

void CTaskBarDlg::TryDrawStatusBar(IDrawCommon& drawer, const CRect& rect_bar, int usage_percent)
{
    //限制范围
    if (usage_percent > 100)
        usage_percent = 100;
    if (usage_percent < 0)
        usage_percent = 0;
    COLORREF graph_color = theApp.m_taskbar_data.GetUsageGraphColor();
    CSize fill_size = CSize(rect_bar.Width() * usage_percent / 100, rect_bar.Height());
    CRect rect_fill(rect_bar.TopLeft(), fill_size);
    if (theApp.m_taskbar_data.show_graph_dashed_box)
        drawer.DrawRectOutLine(rect_bar, graph_color, 1, true);
    drawer.FillRect(rect_fill, graph_color);
}

bool CTaskBarDlg::AdjustWindowPos(bool force_adjust)
{
    if (this->GetSafeHwnd() == NULL || !IsWindow(this->GetSafeHwnd()))
        return false;

    if (m_is_width_changed)
        force_adjust = true;

    if (force_adjust)
        ResetTaskbarPos();

    //外层任务栏矩形不变时，其子窗口仍可能移动或改变尺寸。
    //始终执行派生类检查，由派生类已有的变化检测避免重复 MoveWindow。
    ::GetWindowRect(m_hTaskbar, m_rcTaskbar);   //获得任务栏的矩形区域

    static bool last_taskbar_on_top_or_bottom;
    CheckTaskbarOnTopOrBottom();
    if (force_adjust || m_taskbar_on_top_or_bottom != last_taskbar_on_top_or_bottom)
    {
        CalculateWindowSize();
        last_taskbar_on_top_or_bottom = m_taskbar_on_top_or_bottom;
        force_adjust = true;
    }

    AdjustTaskbarWndPos(force_adjust);

    //如果窗口没有被成功嵌入到任务栏，窗口移动到了基于屏幕左上角的绝对位置，则修正窗口的位置
    if (m_connot_insert_to_task_bar)
    {
        CRect rc_parent;
        ::GetWindowRect(GetParentHwnd(), rc_parent);
        CRect rect{ m_rect };
        rect.MoveToXY(rect.left + rc_parent.left, rect.top + rc_parent.top);
        this->MoveWindow(rect);

        if (::GetForegroundWindow() == m_hTaskbar)   //在窗口无法嵌入任务栏时，如果焦点设置在了任务栏上，则让窗口置顶
        {
            SetWindowPos(&wndTopMost, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);         //设置置顶
        }
    }

    m_is_width_changed = false;     //调整完窗口位置重置标志
    CRect taskbar_rect;
    GetWindowRect(&taskbar_rect);
    CSpeedGraphTip::Instance().OnDpiChanged(GetDPI(), taskbar_rect);
    return true;
}

void CTaskBarDlg::ApplyWindowTransparentColor()
{
#ifndef COMPILE_FOR_WINXP
    if (theApp.IsWindows11Taskbar())      //Windows11下背景色不使用纯黑色，以解决深色模式下右键菜单无法弹出的问题
    {
        if (theApp.m_taskbar_data.transparent_color == 0 && theApp.m_taskbar_data.back_color == 0)
        {
            theApp.m_taskbar_data.transparent_color = 1;
            theApp.m_taskbar_data.back_color = 1;
        }
    }
    if ((theApp.m_taskbar_data.transparent_color != 0) && theApp.m_taksbar_transparent_color_enable)
    {
        auto render_type = m_supported_render_enums.GetAutoFitEnum();
        switch (render_type)
        {
            using namespace DrawCommonHelper;
        case RenderType::D2D1_WITH_DCOMPOSITION:
            // D2D绘图并使用DComposition混合，不设置任何属性
            break;
        case RenderType::D2D1:
            // D2D绘图直接使用alpha混合，只设置 WS_EX_LAYERED
            SetWindowLong(m_hWnd, GWL_EXSTYLE, GetWindowLong(m_hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);
            break;
        case RenderType::DEFAULT:
            // GDI绘图使用色键抠像
            // 仅在透明且不使用自动决定背景颜色时启动D2D渲染器
            SetWindowLong(m_hWnd, GWL_EXSTYLE, GetWindowLong(m_hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);
            SetLayeredWindowAttributes(theApp.m_taskbar_data.transparent_color, 0, LWA_COLORKEY);
        }
    }
    else
    {
        SetWindowLong(m_hWnd, GWL_EXSTYLE, GetWindowLong(m_hWnd, GWL_EXSTYLE) & ~WS_EX_LAYERED);
    }
#endif // !COMPILE_FOR_WINXP
}

//bool CTaskBarDlg::IsTaskbarChanged()
//{
//    bool is_scendary_display;
//    static HWND last_taskbar_handle{};
//    HWND taskbar_handle = FindTaskbarHandle(is_scendary_display);
//    bool changed{ false };
//    if (last_taskbar_handle != taskbar_handle)
//    {
//        changed = true;
//        last_taskbar_handle = taskbar_handle;
//    }
//    return changed;
//}

void CTaskBarDlg::WidthChanged()
{
    m_is_width_changed = true;
}

const CRect& CTaskBarDlg::GetRectForDpiCheck() const
{
    return m_rcTaskbar;
}

UINT CTaskBarDlg::GetDPI() const
{
    return m_taskbar_dpi;
}

void CTaskBarDlg::SetDPI(UINT dpi)
{
    if (dpi == 0 || dpi == m_taskbar_dpi)
        return;

    m_taskbar_dpi = dpi;
    CRect taskbar_rect;
    if (!::IsWindow(m_hTaskbar) || !::GetWindowRect(m_hTaskbar, &taskbar_rect))
        taskbar_rect = m_rcTaskbar;
    CSpeedGraphTip::Instance().OnDpiChanged(dpi, taskbar_rect);
}

UINT CTaskBarDlg::DPI(UINT pixel) const
{
    return m_taskbar_dpi * pixel / 96;
}

int CTaskBarDlg::DPI(int pixel) const
{
    return static_cast<int>(m_taskbar_dpi) * pixel / 96;
}

LONG CTaskBarDlg::DPI(LONG pixel) const
{
    return static_cast<LONG>(m_taskbar_dpi) * pixel / 96;
}

void CTaskBarDlg::DPI(CRect& rect) const
{
    rect.left = DPI(rect.left);
    rect.right = DPI(rect.right);
    rect.top = DPI(rect.top);
    rect.bottom = DPI(rect.bottom);
}

CTaskBarDlg::ClassCheckWindowMonitorDPIAndHandle CTaskBarDlg::CheckWindowMonitorDPIAndHandle{};

UINT CTaskBarDlg::ClassCheckWindowMonitorDPIAndHandle::buffered_dpi_x{0};
UINT CTaskBarDlg::ClassCheckWindowMonitorDPIAndHandle::buffered_dpi_y{0};
UINT CTaskBarDlg::ClassCheckWindowMonitorDPIAndHandle::dpi_x{0};
UINT CTaskBarDlg::ClassCheckWindowMonitorDPIAndHandle::dpi_y{0};

HWND CTaskBarDlg::FindTaskbarHandle(bool& is_scendary_display)
{
    is_scendary_display = false;
    HWND hTaskbar = nullptr;
    //显示在副显示器上
    if (theApp.m_taskbar_data.show_taskbar_wnd_in_secondary_display && CWindowsSettingHelper::IsTaskbarShowingInAllDisplays())
    {
        //获取所有副显示器的任务栏
        std::vector<HWND> secondary_taskbars;
        CTaskbarHelper::GetAllSecondaryDisplayTaskbar(secondary_taskbars);
        if (!secondary_taskbars.empty())
        {
            int index = theApp.m_taskbar_data.secondary_display_index;
            if (index < 0)
                index = 0;
            if (index >= static_cast<int>(secondary_taskbars.size()))
                index = static_cast<int>(secondary_taskbars.size()) - 1;
            hTaskbar = secondary_taskbars[index];
            if (hTaskbar != nullptr)
                is_scendary_display = true;
        }
    }
    if (hTaskbar == nullptr)
        hTaskbar = ::FindWindow(_T("Shell_TrayWnd"), NULL);

    return hTaskbar;
}

CString CTaskBarDlg::GetMouseTipsInfo()
{
    CString tip_info;
    CString temp;
    temp.Format(_T("%s: %s\r\n (%s: %s/%s: %s)"), CCommon::LoadText(IDS_TRAFFIC_USED_TODAY),
        CCommon::KBytesToString((theApp.m_today_up_traffic + theApp.m_today_down_traffic) / 1024u),
        CCommon::LoadText(IDS_UPLOAD), CCommon::KBytesToString(theApp.m_today_up_traffic / 1024u),
        CCommon::LoadText(IDS_DOWNLOAD), CCommon::KBytesToString(theApp.m_today_down_traffic / 1024u)
    );
    tip_info += temp;
    if (!IsItemShow(TDI_UP))
    {
        temp.Format(_T("\r\n%s: %s/s"), CCommon::LoadText(IDS_UPLOAD),
            CCommon::DataSizeToString(theApp.m_out_speed, theApp.m_taskbar_data));
        tip_info += temp;
    }
    if (!IsItemShow(TDI_DOWN))
    {
        temp.Format(_T("\r\n%s: %s/s"), CCommon::LoadText(IDS_DOWNLOAD),
            CCommon::DataSizeToString(theApp.m_in_speed, theApp.m_taskbar_data));
        tip_info += temp;
    }
    if (!IsItemShow(TDI_CPU))
    {
        temp.Format(_T("\r\n%s: %d %%"), CCommon::LoadText(IDS_CPU_USAGE), theApp.m_cpu_usage);
        tip_info += temp;
    }
    if (!IsItemShow(TDI_MEMORY))
    {
        temp.Format(_T("\r\n%s: %s/%s (%d %%)"), CCommon::LoadText(IDS_MEMORY_USAGE),
            CCommon::KBytesToString(theApp.m_used_memory),
            CCommon::KBytesToString(theApp.m_total_memory), theApp.m_memory_usage);
        tip_info += temp;
    }
    else
    {
        temp.Format(_T("\r\n%s: %s/%s"), CCommon::LoadText(IDS_MEMORY_USAGE),
            CCommon::KBytesToString(theApp.m_used_memory),
            CCommon::KBytesToString(theApp.m_total_memory));
        tip_info += temp;
    }
    if (!IsItemShow(TDI_CPU_FREQ) && theApp.m_cpu_freq > 0)
    {
        temp.Format(_T("\r\n%s: %s"), CCommon::LoadText(IDS_CPU_FREQ), CCommon::FreqToString(theApp.m_cpu_freq, theApp.m_taskbar_data));
        tip_info += temp;
    }

    if (!IsItemShow(TDI_GPU_USAGE) && theApp.m_gpu_usage >= 0)
    {
        temp.Format(_T("\r\n%s: %d %%"), CCommon::LoadText(IDS_GPU_USAGE), theApp.m_gpu_usage);
        tip_info += temp;
    }

    if (!IsItemShow(TDI_DEDICATED_GPU_USAGE) && theApp.m_dedicated_gpu_memory_pct >= 0)
    {
        temp.Format(_T("\r\n%s: %d%% (%s / %s)"), CCommon::LoadText(IDS_GPU_MEMORY), theApp.m_dedicated_gpu_memory_pct,
            CCommon::KBytesToString(theApp.m_dedicated_gpu_memory_used_mb * 1024ull).GetString(),
            CCommon::KBytesToString(theApp.m_dedicated_gpu_memory_total_mb * 1024ull).GetString());
        tip_info += temp;
    }

    if (!IsItemShow(TDI_GPU_POWER) && theApp.m_gpu_power_w > 0)
    {
        temp.Format(_T("\r\n%s: %d W"), CCommon::LoadText(IDS_GPU_POWER), theApp.m_gpu_power_w);
        tip_info += temp;
    }

    if (!IsItemShow(TDI_DISK_READ) && theApp.m_disk_speed_available)
    {
        temp.Format(_T("\r\n%s: %s"), CCommon::LoadText(IDS_DISK_READ),
            (CCommon::DataSizeToString(theApp.m_disk_read_speed, theApp.m_taskbar_data) + _T("/s")).GetString());
        tip_info += temp;
    }

    if (!IsItemShow(TDI_DISK_WRITE) && theApp.m_disk_speed_available)
    {
        temp.Format(_T("\r\n%s: %s"), CCommon::LoadText(IDS_DISK_WRITE),
            (CCommon::DataSizeToString(theApp.m_disk_write_speed, theApp.m_taskbar_data) + _T("/s")).GetString());
        tip_info += temp;
    }

#ifndef WITHOUT_TEMPERATURE
    CPulseBarDlg* pMainWnd = dynamic_cast<CPulseBarDlg*>(theApp.m_pMainWnd);
    if (pMainWnd != nullptr && pMainWnd->IsTemperatureNeeded())
    {
        if (!IsItemShow(TDI_CPU_TEMP) && theApp.m_cpu_temperature > 0)
        {
            temp.Format(_T("\r\n%s: %s"), CCommon::LoadText(IDS_CPU_TEMPERATURE), CCommon::TemperatureToString(theApp.m_cpu_temperature, theApp.m_taskbar_data));
            tip_info += temp;
        }
        if (!IsItemShow(TDI_GPU_TEMP) && theApp.m_gpu_temperature > 0)
        {
            temp.Format(_T("\r\n%s: %s"), CCommon::LoadText(IDS_GPU_TEMPERATURE), CCommon::TemperatureToString(theApp.m_gpu_temperature, theApp.m_taskbar_data));
            tip_info += temp;
        }
        if (!IsItemShow(TDI_HDD_TEMP) && theApp.m_hdd_temperature > 0)
        {
            temp.Format(_T("\r\n%s: %s"), CCommon::LoadText(IDS_HDD_TEMPERATURE), CCommon::TemperatureToString(theApp.m_hdd_temperature, theApp.m_taskbar_data));
            tip_info += temp;
        }
        if (!IsItemShow(TDI_MAIN_BOARD_TEMP) && theApp.m_main_board_temperature > 0)
        {
            temp.Format(_T("\r\n%s: %s"), CCommon::LoadText(IDS_MAINBOARD_TEMPERATURE), CCommon::TemperatureToString(theApp.m_main_board_temperature, theApp.m_taskbar_data));
            tip_info += temp;
        }
    }
#endif
    if (!IsItemShow(TDI_HDD_USAGE) && theApp.m_hdd_usage >= 0)
    {
        temp.Format(_T("\r\n%s: %d %%"), CCommon::LoadText(IDS_HDD_USAGE), theApp.m_hdd_usage);
        tip_info += temp;
    }

    //添加插件项目的鼠标提示
    tip_info += theApp.GetPlauginTooltipInfo().c_str();

    return tip_info;
}

void CTaskBarDlg::SetTextFont()
{
    //如果m_font已经关联了一个字体资源对象，则释放它
    if (m_font.m_hObject)
    {
        m_font.DeleteObject();
    }
    //创建新的字体
    theApp.m_taskbar_data.font.Create(m_font, GetDPI());

    //主题启用数值加粗时创建加粗字体
    const TaskbarThemeInfo* theme = GetActiveTaskbarTheme();
    if (m_bold_font.m_hObject)
    {
        m_bold_font.DeleteObject();
    }
    if (theme != nullptr && theme->value_bold && !theApp.m_taskbar_data.font.name.IsEmpty())
    {
        LOGFONT lf{};
        m_font.GetLogFont(&lf);
        lf.lfWeight = FW_BOLD;
        m_bold_font.CreateFontIndirect(&lf);
    }
}

bool CTaskBarDlg::NeedsWindowRecreation() const
{
    CSupportedRenderEnums desired;
    DisableRenderFeatureIfNecessary(desired);
    const auto desired_type = desired.GetAutoFitEnum();
    const bool needs_no_redirection = desired_type == DrawCommonHelper::RenderType::D2D1_WITH_DCOMPOSITION;
    const bool has_no_redirection = (GetWindowLongPtr(GetSafeHwnd(), GWL_EXSTYLE) & WS_EX_NOREDIRECTIONBITMAP) != 0;
    return desired_type != m_supported_render_enums.GetAutoFitEnum() || needs_no_redirection != has_no_redirection;
}

void CTaskBarDlg::ApplySettings()
{
    SetTextFont();
    //主题/背景图模式变化后重新评估渲染方式(主题面板强制GDI),菜单切换主题无需重启即生效
    DisableRenderFeatureIfNecessary(m_supported_render_enums);
    CalculateWindowSize();
    //CalculateWindowSize 只更新模型矩形；这里必须立即把新尺寸/位置提交给 HWND，
    //不能等任务栏定时器，否则新 m_rect 会与旧 client size 并存一段时间。
    AdjustWindowPos(true);
    Invalidate(FALSE);
    UpdateWindow();
    CSpeedGraphTip::Instance().Refresh();
    if (!theApp.m_taskbar_data.show_speed_graph_tip)
        HideSpeedGraphTip();
    //开启悬停面板后立即收掉可能残留的原生tooltip，避免两者同时显示
    if (theApp.m_taskbar_data.show_speed_graph_tip && IsWindow(m_tool_tips.GetSafeHwnd()))
        m_tool_tips.Pop();
}

void CTaskBarDlg::InvalidateIfNeeded()
{
    //重绘按监控样本失效，覆盖数值、插件文本、自绘内容及图表等所有任务栏显示数据。
    //采样通知每秒到达，避免在100ms定时器中轮询插件取值函数。
    //菜单弹出期间由系统负责菜单窗口绘制，不能因为 m_menu_popuped 保持原窗口空转重绘。
    if (m_monitor_data_updated && !m_render_retry_pending)
    {
        m_monitor_data_updated = false;
        Invalidate(FALSE);
    }
}

void CTaskBarDlg::CalculateWindowSize()
{
    //优化:布局重算的输入(显示项集合/字体/任务栏方向/DPI/文本宽度相关配置)没变化时跳过。
    //每秒全量重算要清空重建容器并做十几次 GetTextExtent 文本测量。
    m_pDC->SelectObject(&m_font);

    // 自绘插件可在不修改宿主配置的情况下改变宽度，宽度本身也是布局输入。
    std::map<IPluginItem*, int> custom_draw_item_widths;
    std::wstring custom_draw_width_signature;
    for (const auto& item : theApp.m_plugins.AllDisplayItemsWithPlugins())
    {
        if (!item.IsPlugin())
            continue;
        IPluginItem* plugin = item.PluginItem();
        if (plugin == nullptr || plugin->GetItemId() == nullptr
            || !theApp.m_taskbar_data.plugin_display_item.Contains(plugin->GetItemId())
            || !plugin->IsCustomDraw())
            continue;
        const int width = theApp.m_plugins.GetItemWidth(plugin, m_pDC);
        custom_draw_item_widths[plugin] = width;
        custom_draw_width_signature += std::wstring(plugin->GetItemId()) + L":" + std::to_wstring(width) + L";";
    }

    wstring disp_str_hash;
    for (const auto& iter : theApp.m_taskbar_data.disp_str.GetAllItems())
        disp_str_hash += std::wstring(iter.first.GetItemName().GetString()) + iter.second + L";";
    size_t disp_hash = std::hash<std::wstring>{}(
        theApp.m_taskbar_data.plugin_display_item.ToString()
        + theApp.m_taskbar_data.item_order.ToString()
        + std::to_wstring((INT_PTR)m_font.GetSafeHandle())
        + (m_taskbar_on_top_or_bottom ? L"T" : L"F")
        + std::to_wstring(m_taskbar_dpi)
        + std::to_wstring(theApp.m_taskbar_data.horizontal_arrange ? 1 : 0)
        + std::to_wstring(theApp.m_taskbar_data.display_item.ToInt())
        + std::to_wstring(theApp.m_taskbar_data.item_space)
        + std::to_wstring(theApp.m_taskbar_data.vertical_margin)
        + std::to_wstring(theApp.m_taskbar_data.digits_number)
        + std::to_wstring(theApp.m_taskbar_data.speed_short_mode ? 1 : 0)
        + std::to_wstring(theApp.m_taskbar_data.separate_value_unit_with_space ? 1 : 0)
        + std::to_wstring(static_cast<int>(theApp.m_taskbar_data.speed_unit))
        + std::to_wstring(theApp.m_taskbar_data.hide_unit ? 1 : 0)
        + std::to_wstring(theApp.m_taskbar_data.hide_percent ? 1 : 0)
        + std::to_wstring(static_cast<int>(theApp.m_taskbar_data.memory_display))
        + std::to_wstring(theApp.m_taskbar_data.value_right_align ? 1 : 0)
        + std::to_wstring(theApp.m_taskbar_data.show_status_bar ? 1 : 0)
        + std::to_wstring(theApp.m_taskbar_data.show_netspeed_figure ? 1 : 0)
        + custom_draw_width_signature
        + disp_str_hash);
    if (disp_hash == m_last_layout_signature && !m_item_widths.empty())
        return;
    m_last_layout_signature = disp_hash;

    bool horizontal_arrange = theApp.m_taskbar_data.horizontal_arrange && m_taskbar_on_top_or_bottom;
    if (theApp.m_taskbar_data.display_item.IsEmpty() && theApp.m_taskbar_data.plugin_display_item.data().empty())
        theApp.m_taskbar_data.display_item.Add(TDI_UP);        //至少显示一项

    m_item_widths.clear();
    m_item_rects.clear();
    //显示项目的宽度
    std::map<CommonDisplayItem, ItemWidth> item_widths;

    //计算标签和数值的宽度
    //const auto& item_map = theApp.m_taskbar_data.disp_str.GetAllItems();
    for (auto iter = theApp.m_plugins.AllDisplayItemsWithPlugins().begin(); iter != theApp.m_plugins.AllDisplayItemsWithPlugins().end(); ++iter)
    {
        if (iter->IsPlugin())
        {
            auto plugin = iter->PluginItem();
            //GetItemId() 可能返回空指针，Contains 的参数是 std::wstring，
            //空指针构造 std::wstring 会直接崩溃
            if (plugin != nullptr && plugin->GetItemId() != nullptr && theApp.m_taskbar_data.plugin_display_item.Contains(plugin->GetItemId()))
            {
                //标签宽度
                int& label_width{ item_widths[*iter].label_width };
                //数值宽度
                int& value_width{ item_widths[plugin].value_width };
                if (plugin->IsCustomDraw())
                {
                    label_width = 0;
                    auto measured = custom_draw_item_widths.find(plugin);
                    value_width = measured != custom_draw_item_widths.end()
                        ? measured->second
                        : theApp.m_plugins.GetItemWidth(plugin, m_pDC);
                }
                else
                {
                    CString lable_text = theApp.m_taskbar_data.disp_str.GetConst(plugin).c_str();
                    label_width = m_pDC->GetTextExtent(lable_text).cx;
                    const wchar_t* sample_text = plugin->GetItemValueSampleText();
                    if (sample_text != nullptr)
                        value_width = m_pDC->GetTextExtent(sample_text).cx;
                }
            }
        }
        else
        {
            //标签宽度
            item_widths[*iter].label_width = m_pDC->GetTextExtent(theApp.m_taskbar_data.disp_str.GetConst(*iter).c_str()).cx;
            //数值宽度
            CString sample_str = iter->GetItemValueSampleText(false);
            item_widths[*iter].value_width = m_pDC->GetTextExtent(sample_str).cx;
        }
    }

    auto item_order{ theApp.m_taskbar_data.item_order.GetAllDisplayItemsWithOrder() };
    for (const auto& item : item_order)
    {
        if (theApp.IsTaksbarItemDisplayed(item))
        {
            ItemWidthInfo width_info = item;
            width_info.item_width = item_widths[item];
            m_item_widths.push_back(width_info);
        }
    }

    int item_count = static_cast<int>(m_item_widths.size());

    auto last_window_width{ m_window_width };
    auto last_window_height{ m_window_height };

    //计算窗口总宽度/高度和矩形区域
    if (IsTasksbarOnTopOrBottom())  //任务栏在桌面的顶部或底部时
    {
        if (!horizontal_arrange)
            m_window_height = TASKBAR_WND_HEIGHT;
        else
            m_window_height = TASKBAR_WND_HEIGHT * 2 / 3;

        m_window_width = 0;
        if (theApp.m_taskbar_data.horizontal_arrange)   //水平排列时
        {
            CRect item_rect{};
            int index = 0;
            for (auto iter = m_item_widths.begin(); iter != m_item_widths.end(); ++iter)
            {
                m_window_width += iter->item_width.TotalWidth();
                if (index > 0)
                    item_rect.MoveToX(item_rect.right + DPI(theApp.m_taskbar_data.item_space));
                item_rect.right = item_rect.left + iter->item_width.TotalWidth();
                item_rect.bottom = item_rect.top + m_window_height;
                m_item_rects[*iter] = item_rect;
                index++;
            }
            m_window_width += DPI(theApp.m_taskbar_data.item_space) * item_count;   //加上每个标签间的空隙
        }
        else        //非水平排列时，每两个一组排列
        {
            int current_x = DPI(theApp.m_taskbar_data.item_space); // 初始 X 坐标为左侧空隙
            int item_space = DPI(theApp.m_taskbar_data.item_space);

            //计算两个一组时上下区域的垂直位置
            int item_height = m_window_height / 2;
            int y_pos_up = -DPI(theApp.m_taskbar_data.vertical_margin) / 2;
            int y_pos_down = y_pos_up + item_height + DPI(theApp.m_taskbar_data.vertical_margin);

            int width0 = 0;
            int max_width0 = 0;
            bool has_first_item = false;
            auto first_iter = m_item_widths.end(); // 用于保存当前列第一个项目的迭代器，以便后续修正其宽度

            for (auto iter = m_item_widths.begin(); iter != m_item_widths.end(); ++iter)
            {
                if (iter->IsDoubleLineExclusive())
                {
                    // 如果前面有未配对的单行项目，它现在被迫单独成列，应该占两行
                    if (has_first_item)
                    {
                        int col_width = max_width0; // 占两行时，宽度使用 MaxWidth()
                        m_item_rects[*first_iter].SetRect(current_x, 0, current_x + col_width, m_window_height);
                        first_iter->is_double_line = true;

                        current_x += col_width + item_space;
                        has_first_item = false;
                    }

                    // 处理当前的独占双行项目，单独成列，占两行
                    int col_width = iter->item_width.MaxWidth();
                    m_item_rects[*iter].SetRect(current_x, 0, current_x + col_width, m_window_height);
                    iter->is_double_line = true;

                    current_x += col_width + item_space;
                }
                else
                {
                    iter->is_double_line = false;
                    if (!has_first_item)
                    {
                        // 当前列的第一个项目（上半部分），占一行
                        width0 = iter->item_width.TotalWidth();
                        max_width0 = iter->item_width.MaxWidth();

                        first_iter = iter;
                        // 暂时设置 rect，right 边界会在配对成功或循环结束时根据列最大宽度进行修正
                        m_item_rects[*iter].SetRect(current_x, y_pos_up, current_x + width0, y_pos_up + item_height);

                        has_first_item = true;
                    }
                    else
                    {
                        // 当前列的第二个项目（下半部分），占一行，与第一个项目组成一列
                        int col_width = max(width0, iter->item_width.TotalWidth());

                        // 修正第一个项目的右边界，确保同一列的两个项目宽度一致
                        m_item_rects[*first_iter].right = current_x + col_width;

                        // 设置第二个项目的矩形区域
                        m_item_rects[*iter].SetRect(current_x, y_pos_down, current_x + col_width, y_pos_down + item_height);

                        current_x += col_width + item_space;
                        has_first_item = false;
                    }
                }
            }

            // 循环结束后，如果还有未配对的单行项目（例如项目总数为奇数，且最后一个不是独占双行），它单独成列，应该占两行
            if (has_first_item)
            {
                int col_width = max_width0; // 占两行时，宽度使用 MaxWidth()
                m_item_rects[*first_iter].SetRect(current_x, 0, current_x + col_width, m_window_height);
                first_iter->is_double_line = true;

                current_x += col_width + item_space;
            }

            // 保存总宽度
            m_window_width = current_x;
        }
    }
    else        //任务栏在桌面两侧时
    {
        m_window_width = 0;
        m_window_height = 0;
        CRect item_rect;
        for (auto iter = m_item_widths.begin(); iter != m_item_widths.end(); ++iter)
        {
            //所有标签中最大的宽度即为窗口宽度
            if (m_window_width < iter->item_width.TotalWidth())
                m_window_width = iter->item_width.TotalWidth();

            int item_height = TASKBAR_WND_HEIGHT / 2;
            if (iter->IsDoubleLineExclusive())
                item_height = TASKBAR_WND_HEIGHT;

            //计算矩形区域
            item_rect.top = m_window_height;
            item_rect.bottom = item_rect.top + item_height;
            int item_width = min(m_window_width, m_rcTaskbar.Width() - DPI(2));
            item_rect.left = 0;
            item_rect.right = item_rect.left + item_width;
            m_item_rects[*iter] = item_rect;

            m_window_height += item_height;
            m_window_height += DPI(theApp.m_taskbar_data.item_space);
        }
    }

    // 如果窗口尺寸发生变化，则重新调整任务栏窗口位置
    if (last_window_width != m_window_width || last_window_height != m_window_height) {
        WidthChanged();
    }

    m_rect.right = m_rect.left + m_window_width;
    m_rect.bottom = m_rect.top + m_window_height;

}

void CTaskBarDlg::SetToolTipsTopMost()
{
    m_tool_tips.SetWindowPos(&wndTopMost, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW);
}

void CTaskBarDlg::UpdateToolTips()
{
    //WM_MONITOR_INFO_UPDATED 是每次采样完成的通知；即使关闭原生提示，
    //任务栏内容也必须因插件文本和未参与旧签名的指标而失效。
    m_monitor_data_updated = true;
    if (theApp.m_taskbar_data.show_tool_tip && IsWindow(m_tool_tips.GetSafeHwnd()))
    {
        CString tip_info;
        tip_info = GetMouseTipsInfo();
        m_tool_tips.UpdateTipText(tip_info, this);
    }
    //B4/S3:每秒记录一次采样(供超级悬停面板使用)
    CSpeedGraphTip::Instance().AddSample(theApp.m_out_speed, theApp.m_in_speed,
        theApp.m_cpu_usage, max(theApp.m_gpu_usage, 0));
}

bool CTaskBarDlg::IsItemShow(DisplayItem item)
{
    return (theApp.m_taskbar_data.display_item.Contains(item));
}

bool CTaskBarDlg::IsShowCpuMemory()
{
    return (theApp.m_taskbar_data.display_item.Contains(TDI_CPU) || theApp.m_taskbar_data.display_item.Contains(TDI_MEMORY));
}

bool CTaskBarDlg::IsShowNetSpeed()
{
    return (theApp.m_taskbar_data.display_item.Contains(TDI_UP) || theApp.m_taskbar_data.display_item.Contains(TDI_DOWN));
}

bool CTaskBarDlg::IsTaskbarCloseToIconEnable(bool taskbar_wnd_on_left)
{
    //在Windows11中，当任务栏窗口显示在右侧时，如果勾选“任务栏窗口靠近图标而不是靠近任务栏窗口”会导致和图标重叠
    //因为"MSTaskSwWClass"窗口的矩形区域不再是任务栏图标最小化按钮所在区域
    return theApp.m_win_version.IsWindows11OrLater() &&
        CWindowsSettingHelper::IsTaskbarCenterAlign() && taskbar_wnd_on_left;
}

BOOL CTaskBarDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // TODO:  在此添加额外的初始化
    SetWindowText(TASKBAR_WINDOW_NAME);
    // 检测系统是否安装了 MicrosoftWindows.Client.WebExperience (aka Windows Web Experience Pack)
    theApp.m_taskbar_data.is_windows_web_experience_detected =
        WindowsWebExperienceDetector::IsDetected();
    // 根据任务栏窗口的设置禁用必要的渲染选项，仅透明且支持D2D渲染时才会使用D2D渲染
    DisableRenderFeatureIfNecessary(m_supported_render_enums);
    //设置隐藏任务栏图标
    ModifyStyleEx(0, WS_EX_TOOLWINDOW);

    m_pDC = GetDC();
    m_hTaskbar = FindTaskbarHandle(m_is_secondary_display); //查找任务栏的句柄
    if (!::IsWindow(m_hTaskbar) || !::GetWindowRect(m_hTaskbar, m_rcTaskbar))
        return TRUE;    //宿主检查 IsReadyToShow，不显示无效 Shell 上的半初始化窗口。

    //设置窗口透明色
    ApplyWindowTransparentColor();

    InitTaskbarWnd();
    const HWND taskbar_parent = GetParentHwnd();
    if (!::IsWindow(taskbar_parent))
        return TRUE;
    //SetParent 对非 WS_CHILD 的 popup 对话框设置的是 GA_PARENT 链；
    //GetParent 只查 owner，用它判定会永远误报失败并触发重建循环。
    //成功判定以 SetParent 返回值与 GetAncestor(GA_PARENT) 为准。
    const HWND prev_parent = ::SetParent(this->m_hWnd, taskbar_parent);
    m_error_code = ::GetLastError();
    m_connot_insert_to_task_bar = prev_parent == nullptr
        || ::GetAncestor(this->m_hWnd, GA_PARENT) != taskbar_parent;
    if (m_connot_insert_to_task_bar)
    {
        CCommon::WriteLog(CCommon::IntToString(static_cast<int>(reinterpret_cast<UINT_PTR>(prev_parent)))
            + L" SetParent failed, GA_PARENT=" +
            CCommon::IntToString(static_cast<int>(reinterpret_cast<UINT_PTR>(
                ::GetAncestor(this->m_hWnd, GA_PARENT)))) +
            L" error_code=" + CCommon::IntToString(static_cast<int>(m_error_code)),
            theApp.m_log_path.c_str());
    }

    //根据已经确定的任务栏最小化窗口区域得到屏幕并获得所在屏幕的DPI（Windows 8.1及其以上）
    if (theApp.m_win_version.IsWindows8Point1OrLater())
    {
        UINT dpi_x, dpi_y;
        if (theApp.DPIFromRect(GetRectForDpiCheck(), &dpi_x, &dpi_y))
            SetDPI(dpi_x);
        else
            SetDPI(theApp.GetDpi());
    }
    else
    {
        SetDPI(theApp.GetDpi());
    }

    //设置字体
    SetTextFont();
    m_pDC->SelectObject(&m_font);

    CheckTaskbarOnTopOrBottom();
    CalculateWindowSize();
    m_rect.SetRectEmpty();
    m_rect.bottom = m_window_height;
    m_rect.right = m_rect.left + m_window_width;
    AdjustWindowPos(true);

    SetBackgroundColor(theApp.m_taskbar_data.back_color);

    //初始化鼠标提示
    if (IsWindow(GetSafeHwnd()) && m_tool_tips.Create(this, TTS_ALWAYSTIP) && IsWindow(m_tool_tips.GetSafeHwnd()))
    {
        m_tool_tips.SetMaxTipWidth(600);
        m_tool_tips.AddTool(this, _T(""));
        SetToolTipsTopMost();       //设置提示信息总是置顶
    }

    SetTimer(TASKBAR_TIMER, 1000, NULL);

    m_initialization_succeeded = !m_rect.IsRectEmpty();
    return TRUE;  // return TRUE unless you set the focus to a control
                  // 异常: OCX 属性页应返回 FALSE
}

void CTaskBarDlg::OnCancel()
{
    // TODO: 在此添加专用代码和/或调用基类
    //SaveConfig();

    //S6: 先强制隐藏悬停面板(含解除钉住),防止置顶面板在窗口销毁后残留
    CSpeedGraphTip::Instance().ForceHide();
    m_graph_tip_showing = false;

    //关闭所有以任务栏窗口为父窗口的窗口
    for (const auto& item : CBaseDialog::AllUniqueHandels())
    {
        HWND parent = ::GetParent(item.second);
        if (parent == GetSafeHwnd())
        {
            ::SendMessage(item.second, WM_COMMAND, IDCANCEL, 0);
        }
    }

    DestroyWindow();
    ResetTaskbarPos();

    //CDialogEx::OnCancel();
}

namespace
{
    CMenu* FindMenuContainingCommand(CMenu* menu, UINT command_id)
    {
        if (menu == nullptr)
            return nullptr;

        for (int index = 0; index < menu->GetMenuItemCount(); ++index)
        {
            if (menu->GetMenuItemID(index) == command_id)
                return menu;
        }

        for (int index = 0; index < menu->GetMenuItemCount(); ++index)
        {
            CMenu* sub_menu = menu->GetSubMenu(index);
            CMenu* owner = FindMenuContainingCommand(sub_menu, command_id);
            if (owner != nullptr)
                return owner;
        }
        return nullptr;
    }

    bool FindSubmenuByHandle(CMenu* menu, HMENU submenu_handle, CMenu*& parent_menu, int& position)
    {
        if (menu == nullptr)
            return false;

        for (int index = 0; index < menu->GetMenuItemCount(); ++index)
        {
            CMenu* sub_menu = menu->GetSubMenu(index);
            if (sub_menu != nullptr && sub_menu->GetSafeHmenu() == submenu_handle)
            {
                parent_menu = menu;
                position = index;
                return true;
            }
            if (sub_menu != nullptr && FindSubmenuByHandle(sub_menu, submenu_handle, parent_menu, position))
                return true;
        }
        return false;
    }

    void SetMenuDefaultItemByCommand(CMenu* menu, UINT command_id)
    {
        CMenu* owner = FindMenuContainingCommand(menu, command_id);
        if (owner != nullptr)
            owner->SetDefaultItem(command_id, FALSE);
    }

    void EnableMenuItemByCommand(CMenu* menu, UINT command_id, UINT flags)
    {
        CMenu* owner = FindMenuContainingCommand(menu, command_id);
        if (owner != nullptr)
            owner->EnableMenuItem(command_id, MF_BYCOMMAND | flags);
    }

    void CheckMenuItemByCommand(CMenu* menu, UINT command_id, UINT flags)
    {
        CMenu* owner = FindMenuContainingCommand(menu, command_id);
        if (owner != nullptr)
            owner->CheckMenuItem(command_id, MF_BYCOMMAND | flags);
    }
}

void CTaskBarDlg::OnRButtonUp(UINT nFlags, CPoint point)
{
    const HWND source_hwnd=GetSafeHwnd();
    m_menu_popuped=true;
    CSpeedGraphTip::Instance().ForceHide();
    m_graph_tip_showing=false;
    if (::IsWindow(m_tool_tips.GetSafeHwnd()))
        m_tool_tips.Pop();
    IPluginItem* item=CheckClickedItem(point) && m_clicked_item.IsPlugin() ? m_clicked_item.PluginItem() : nullptr;
    ITMPlugin* plugin=item != nullptr ? theApp.m_plugins.GetPluginByItem(item) : nullptr;
    if (plugin != nullptr)
    {
        try
        {
            const int api=plugin->GetAPIVersion();
            if (!::IsWindow(source_hwnd)) return;
            if (api>=3)
            {
                const int handled=item->OnMouseEvent(IPluginItem::MT_RCLICKED,point.x,point.y,
                    static_cast<void*>(source_hwnd),IPluginItem::MF_TASKBAR_WND);
                //回调可重入关闭/重建窗口，后续不得读取已销毁的 this。
                if (!::IsWindow(source_hwnd)) return;
                if (handled!=0) { m_menu_popuped=false; return; }
            }
        }
        catch (CException* exception) { exception->Delete(); }
        catch (...) { CCommon::WriteLog("Plugin right-click callback failed.",theApp.m_log_path.c_str()); }
        if (!::IsWindow(source_hwnd)) return;
    }
    theApp.UpdateTrayPluginMenu();
    if (!::IsWindow(source_hwnd)) return;
    CMenu* source=plugin != nullptr ? &theApp.m_taskbar_menu_plugin : &theApp.m_taskbar_menu;
    CMenu* menu=FindMenuContainingCommand(source,ID_DISPLAY_SETTINGS);
    if (menu==nullptr) { m_menu_popuped=false; return; }
    try
    {
        if (plugin != nullptr)
        {
            CMenu* parent{};
            int position{};
            if (FindSubmenuByHandle(menu,theApp.m_taskbar_menu_plugin_sub_menu.GetSafeHmenu(),parent,position))
            {
                const wchar_t* name=plugin->GetInfo(ITMPlugin::TMI_NAME);
                const std::wstring title=name!=nullptr ? std::wstring(name) : std::wstring{};
                if (!::IsWindow(source_hwnd)) return;
                parent->ModifyMenu(position,MF_BYPOSITION | MF_POPUP | MF_STRING,
                    reinterpret_cast<UINT_PTR>(theApp.m_taskbar_menu_plugin_sub_menu.GetSafeHmenu()),title.c_str());
            }
        }
        theApp.UpdatePluginMenu(&theApp.m_taskbar_menu_plugin_sub_menu,plugin,2);
    }
    catch (CException* exception) { exception->Delete(); }
    catch (...) { CCommon::WriteLog("Plugin context menu callback failed.",theApp.m_log_path.c_str()); }
    if (!::IsWindow(source_hwnd)) return;
    CPoint cursor;
    GetCursorPos(&cursor);
    UINT command{};
    {
        CFlagLocker tracking(theApp.m_tray_plugin_menu_active);
        command=menu->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD,
            cursor.x,cursor.y,this);
    }
    if (!::IsWindow(source_hwnd)) return;
    m_menu_popuped=false;
    if (command==0)
        return;
    if ((command>=ID_PLUGIN_COMMAND_START && command<=ID_PLUGIN_COMMAND_MAX)
        || (command>=ID_SHOW_PLUGIN_ITEM_START && command<=ID_SHOW_PLUGIN_ITEM_MAX))
        ::PostMessage(source_hwnd,WM_COMMAND,command,0);
    else if (theApp.m_pMainWnd != nullptr && ::IsWindow(theApp.m_pMainWnd->GetSafeHwnd()))
        ::PostMessage(theApp.m_pMainWnd->GetSafeHwnd(),WM_COMMAND,command,0);
}


void CTaskBarDlg::ApplyMenuCheckStates(CMenu* menu)
{
    if (menu == nullptr)
        return;
    //「悬停显示监控面板」勾选 = 已启用
    CheckMenuItemByCommand(menu, ID_SHOW_SPEED_GRAPH_TIP,
        theApp.m_taskbar_data.show_speed_graph_tip ? MF_CHECKED : MF_UNCHECKED);
    //「显示任务栏面板」勾选 = 当前正在显示（文案已固定为「显示」，状态只靠勾选表达）
    CheckMenuItemByCommand(menu, ID_SHOW_TASK_BAR_WND,
        theApp.m_cfg_data.m_show_task_bar_wnd ? MF_CHECKED : MF_UNCHECKED);
}

void CTaskBarDlg::OnInitMenu(CMenu* pMenu)
{
    CDialogEx::OnInitMenu(pMenu);

    // TODO: 在此处添加消息处理程序代码

    EnableMenuItemByCommand(pMenu, ID_SELECT_ALL_CONNECTION,
        theApp.m_general_data.show_all_interface ? MF_GRAYED : MF_ENABLED);
    EnableMenuItemByCommand(pMenu, ID_CHECK_UPDATE,
        theApp.IsCheckingForUpdate() ? MF_GRAYED : MF_ENABLED);

    //状态型开关：勾选反映当前状态（任务栏菜单与托盘菜单共用这套标记）
    ApplyMenuCheckStates(pMenu);

    //设置默认菜单项
    switch (theApp.m_taskbar_data.double_click_action)
    {
    case DoubleClickAction::CONNECTION_INFO:
        SetMenuDefaultItemByCommand(pMenu, ID_NETWORK_INFO);
        break;
    case DoubleClickAction::HISTORY_TRAFFIC:
        SetMenuDefaultItemByCommand(pMenu, ID_TRAFFIC_HISTORY);
        break;
    case DoubleClickAction::SHOW_MORE_INFO:
        //该项已从精简菜单移除，仅作为双击动作保留；菜单存在时才设为默认项。
        if (FindMenuContainingCommand(pMenu, ID_SHOW_CPU_MEMORY2) != nullptr)
            SetMenuDefaultItemByCommand(pMenu, ID_SHOW_CPU_MEMORY2);
        else
            pMenu->SetDefaultItem(-1);
        break;
    case DoubleClickAction::OPTIONS:
        SetMenuDefaultItemByCommand(pMenu, ID_OPTIONS2);
        break;
    case DoubleClickAction::TASK_MANAGER:
        SetMenuDefaultItemByCommand(pMenu, ID_OPEN_TASK_MANAGER);
        break;
    default:
        pMenu->SetDefaultItem(-1);
        break;
    }

    //设置插件命令的勾选状态；插件返回数量必须同时受资源 ID 区间约束。
    ITMPlugin* plugin = theApp.m_plugins.GetPluginByItem(m_clicked_item.PluginItem());
    if (plugin != nullptr && plugin->GetAPIVersion() >= 5)
    {
        const int command_count = max(0, min(plugin->GetCommandCount(),
            ID_PLUGIN_COMMAND_MAX - ID_PLUGIN_COMMAND_START + 1));
        for (int index = 0; index < command_count; index++)
        {
            //插件回调可能在插件代码里崩溃（/EHa 下可捕获）：保持菜单可用
            try
            {
                bool checked = (plugin->IsCommandChecked(index) != 0);
                CheckMenuItemByCommand(pMenu, ID_PLUGIN_COMMAND_START + index,
                    checked ? MF_CHECKED : MF_UNCHECKED);
            }
            catch (...)
            {
                LogPluginCallbackFailure(L"ITMPlugin::IsCommandChecked");
            }
        }
    }

    ::SendMessage(theApp.m_pMainWnd->GetSafeHwnd(), WM_TASKBAR_MENU_POPED_UP, 0, 0); //通知主窗口菜单已弹出
}

BOOL CTaskBarDlg::PreTranslateMessage(MSG* pMsg)
{
    // TODO: 在此添加专用代码和/或调用基类
    //屏蔽按回车键和ESC键退出
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_ESCAPE) return TRUE;
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN) return TRUE;

    //悬停面板开启时不再弹出原生 tooltip，避免两个悬浮卡互相遮挡
    //（面板本身已包含网速/CPU/内存/温度等全部信息，tooltip 属于重复展示）
    const bool panel_active = theApp.m_taskbar_data.show_speed_graph_tip;

    if (theApp.m_taskbar_data.show_tool_tip && !panel_active && !m_menu_popuped
        && IsWindow(m_tool_tips.GetSafeHwnd())
        && (pMsg->message == WM_LBUTTONDOWN || pMsg->message == WM_LBUTTONUP || pMsg->message == WM_MOUSEMOVE))
    {
        m_tool_tips.RelayEvent(pMsg);
    }

    if (pMsg->message == WM_KEYDOWN)
    {
        bool ctrl = (GetKeyState(VK_CONTROL) & 0x80);
        bool shift = (GetKeyState(VK_SHIFT) & 0x8000);
        bool alt = (GetKeyState(VK_MENU) & 0x8000);
        ITMPlugin* plugin = theApp.m_plugins.GetPluginByItem(m_clicked_item.PluginItem());
        if (plugin != nullptr && plugin->GetAPIVersion() >= 4)
        {
            if (m_clicked_item.PluginItem()->OnKeboardEvent(pMsg->wParam, ctrl, shift, alt, (void*)GetSafeHwnd(), IPluginItem::KF_TASKBAR_WND) != 0)
                return TRUE;
        }
    }

    return CDialogEx::PreTranslateMessage(pMsg);
}

void CTaskBarDlg::OnMouseMove(UINT nFlags, CPoint point)
{
    //B4/S3:悬停任务栏窗口时显示超级监控面板(可在设置中关闭)
    if (!m_menu_popuped && theApp.m_taskbar_data.show_speed_graph_tip)
    {
        CPoint screen_pt = point;
        ClientToScreen(&screen_pt);
        CRect wnd_rect = m_rect;
        GetWindowRect(&wnd_rect);
        CSpeedGraphTip::Instance().Show(screen_pt, wnd_rect, GetDPI());
        m_graph_tip_showing = true;
    }

    CDialogEx::OnMouseMove(nFlags, point);
}

void CTaskBarDlg::OnLButtonDblClk(UINT nFlags, CPoint point)
{
    // TODO: 在此添加消息处理程序代码和/或调用默认值
    if (CheckClickedItem(point) && m_clicked_item.IsPlugin() && m_clicked_item.PluginItem() != nullptr)
    {
        ITMPlugin* plugin = theApp.m_plugins.GetPluginByItem(m_clicked_item.PluginItem());
        if (plugin != nullptr && plugin->GetAPIVersion() >= 3)
        {
            if (m_clicked_item.PluginItem()->OnMouseEvent(IPluginItem::MT_DBCLICKED, point.x, point.y, (void*)GetSafeHwnd(), IPluginItem::MF_TASKBAR_WND) != 0)
                return;
        }
    }
    switch (theApp.m_taskbar_data.double_click_action)
    {
    case DoubleClickAction::CONNECTION_INFO:
        SendMessage(WM_COMMAND, ID_NETWORK_INFO);        //双击后弹出“连接详情”对话框
        break;
    case DoubleClickAction::HISTORY_TRAFFIC:
        SendMessage(WM_COMMAND, ID_TRAFFIC_HISTORY);        //双击后弹出“历史流量统计”对话框
        break;
    case DoubleClickAction::SHOW_MORE_INFO:
        PostMessage(WM_COMMAND, ID_SHOW_CPU_MEMORY2);       //切换显示CPU和内存利用率
        break;
    case DoubleClickAction::OPTIONS:
        SendMessage(WM_COMMAND, ID_OPTIONS2);       //双击后弹出“选项设置”对话框
        break;
    case DoubleClickAction::TASK_MANAGER:
        ShellExecuteW(NULL, _T("open"), (theApp.m_system_dir + L"\\Taskmgr.exe").c_str(), NULL, NULL, SW_NORMAL);       //打开任务管理器
        break;
    case DoubleClickAction::SEPCIFIC_APP:
        ShellExecuteW(NULL, _T("open"), (theApp.m_taskbar_data.double_click_exe).c_str(), NULL, NULL, SW_NORMAL);   //打开指定程序，默认任务管理器
        break;
    default:
        break;
    }
    //CDialogEx::OnLButtonDblClk(nFlags, point);
}

void CTaskBarDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == TASKBAR_RENDER_RETRY_TIMER)
    {
        KillTimer(TASKBAR_RENDER_RETRY_TIMER);
        m_render_retry_pending = false;
        Invalidate(FALSE);
        return;
    }
    // TODO: 在此添加消息处理程序代码和/或调用默认值
    if (nIDEvent == TASKBAR_TIMER)
    {
        if (m_menu_popuped)     //显示了右键菜单时，不显示鼠标提示
        {
            m_tool_tips.Pop();
        }

        //离开任务栏窗口、悬停面板及两者间的窄过渡区后隐藏(钉住时不隐藏)
        if (m_graph_tip_showing)
        {
            CPoint pt;
            GetCursorPos(&pt);
            CRect rc;
            GetWindowRect(&rc);
            auto& graph_tip = CSpeedGraphTip::Instance();
            if (!graph_tip.IsCursorInHoverRegion(pt, rc))
            {
                graph_tip.Hide();
                if (!graph_tip.IsVisible())
                    m_graph_tip_showing = false;
            }
            else if (rc.PtInRect(pt))
            {
                //仅在任务栏窗口内时跟随鼠标定位；面板/过渡区内停留不移动面板。
                graph_tip.Show(pt, rc, GetDPI());
            }
        }

        //每秒钟重新计算窗口的宽度；菜单弹出期间跳过，避免菜单操作时面板宽度跳动
        if (!m_menu_popuped)
        {
            CalculateWindowSize();
            if (m_is_width_changed)
                AdjustWindowPos();
        }
    }

    CDialogEx::OnTimer(nIDEvent);
}

BOOL CTaskBarDlg::OnCommand(WPARAM wParam, LPARAM lParam)
{
    // TODO: 在此添加专用代码和/或调用基类
    UINT uMsg = LOWORD(wParam);
    if (uMsg == ID_SELECT_ALL_CONNECTION || uMsg == ID_SELETE_CONNECTION
        || (uMsg > ID_SELECT_ALL_CONNECTION && uMsg <= ID_SELETE_CONNECTION_MAX))
    {
        ::SendMessage(theApp.m_pMainWnd->GetSafeHwnd(), WM_COMMAND, wParam, lParam);    //如果点击了“选择网络连接”子菜单项，将消息转发到主窗口
        return TRUE;
    }

    //B3/B4 新增开关命令转发到主窗口处理
    if (uMsg == ID_NOTIFY_ICON_SHOW_SPEED || uMsg == ID_SHOW_SPEED_GRAPH_TIP)
    {
        ::SendMessage(theApp.m_pMainWnd->GetSafeHwnd(), WM_COMMAND, wParam, lParam);
        return TRUE;
    }

    //选择了“显示项目”中的插件项目
    if (uMsg >= ID_SHOW_PLUGIN_ITEM_START && uMsg <= ID_SHOW_PLUGIN_ITEM_MAX)
    {
        IPluginItem* item = theApp.m_plugins.GetItemByIndex(uMsg - ID_SHOW_PLUGIN_ITEM_START);
        if (item != nullptr)
        {
            const wchar_t* item_id = nullptr;
            try
            {
                item_id = item->GetItemId();
            }
            catch (...)
            {
                LogPluginCallbackFailure(L"IPluginItem::GetItemId");
            }
            if (item_id != nullptr)
            {
                bool displayed = theApp.m_taskbar_data.plugin_display_item.Contains(item_id);
                theApp.m_taskbar_data.plugin_display_item.SetStrContained(item_id, !displayed);
                ::PostMessage(theApp.m_pMainWnd->GetSafeHwnd(), WM_REOPEN_TASKBAR_WND, 0, 0);
            }
        }
    }
    //选择了插件命令
    if (uMsg >= ID_PLUGIN_COMMAND_START && uMsg <= ID_PLUGIN_COMMAND_MAX)
    {
        int index = uMsg - ID_PLUGIN_COMMAND_START;
        if (m_clicked_item.IsPlugin() && m_clicked_item.PluginItem() != nullptr)
        {
            ITMPlugin* plugin = theApp.m_plugins.GetPluginByItem(m_clicked_item.PluginItem());
            if (plugin != nullptr && plugin->GetAPIVersion() >= 5)
            {
                const int command_count = max(0, min(plugin->GetCommandCount(),
                    ID_PLUGIN_COMMAND_MAX - ID_PLUGIN_COMMAND_START + 1));
                if (index < command_count)
                {
                    try
                    {
                        plugin->OnPluginCommand(index, (void*)GetSafeHwnd(), nullptr);
                    }
                    catch (...)
                    {
                        LogPluginCallbackFailure(L"ITMPlugin::OnPluginCommand");
                    }
                }
            }
        }
    }

    return CDialogEx::OnCommand(wParam, lParam);
}

BOOL CTaskBarDlg::OnEraseBkgnd(CDC* pDC)
{
    //OnPaint 使用自己的缓冲区整幅绘制；让 MFC 先擦除会提交白/黑中间帧。
    (void)pDC;
    return TRUE;
}

void CTaskBarDlg::OnPaint()
{
    if (m_in_paint)
    {
        CPaintDC nested_dc(this);
        return;
    }
    CFlagLocker paint_lock(m_in_paint);
    CPaintDC dc(this); // device context for painting
                       // TODO: 在此处添加消息处理程序代码
                       // 不为绘图消息调用 CDialogEx::OnPaint()

    try
    {
        ShowInfo(&dc);
    }
    catch (CD3D10Exception1& ex)
    {
        DrawCommonHelper::DefaultD2DDrawCommonExceptionHandler{ex}(
            [p_window_support_wrapper = &this->m_taskbar_draw_common_window_support](CHResultException& ex)
            {
                return DrawCommonHelper::HandleIfNeedRecreate(
                    ex,
                    [p_window_support_wrapper]()
                    { p_window_support_wrapper->Get().RequestD3D10Device1Recreate(); });
            });
    }
    catch (CD2D1Exception& ex)
    {
        DrawCommonHelper::DefaultD2DDrawCommonExceptionHandler{ex}(
            [p_device_context_support_wrapper = &this->m_d2d1_device_context_support](CHResultException& ex)
            {
                return DrawCommonHelper::HandleIfD2D1DeviceNeedRecreate(
                    ex,
                    [&]()
                    { p_device_context_support_wrapper->Get().RequestD2D1DeviceRecreate(ex.GetHResult()); });
            });
    }
    catch (CDCompositionException& ex)
    {
        DrawCommonHelper::DefaultD2DDrawCommonExceptionHandler{ex}(
            [p_device_context_support_wrapper = &this->m_d2d1_device_context_support](CHResultException& ex)
            {
                p_device_context_support_wrapper->Get().RequestDCompositionDeviceRecreate(ex.GetHResult());
                return true;
            });
    }
    catch (CHResultException& ex)
    {
        DrawCommonHelper::DefaultD2DDrawCommonExceptionHandler{ex}();
    }
    catch (std::runtime_error& ex)
    {
        auto* log = ex.what();
        CCommon::WriteLog(log, theApp.m_log_path.c_str());
        // 目前只有它会主动抛异常，所有异常全部算它头上
        DrawCommonHelper::DefaultD2DDrawCommonExceptionHandler::IncreaseErrorCountManually();
        DrawCommonHelper::DefaultD2DDrawCommonExceptionHandler::HandleErrorCountIncrement();
    }
    catch (CException* exception)
    {
        exception->Delete();
        DrawCommonHelper::MarkFrameSubmitFailed(E_FAIL);
        CCommon::WriteLog("MFC plugin paint callback failed.", theApp.m_log_path.c_str());
    }
    catch (...)
    {
        DrawCommonHelper::MarkFrameSubmitFailed(E_FAIL);
        CCommon::WriteLog("Plugin paint callback failed; preserving the previous frame.", theApp.m_log_path.c_str());
    }

    const auto frame_result = DrawCommonHelper::FinishFrameSubmit();
    if (frame_result.active && !frame_result.succeeded)
    {
        if (m_consecutive_frame_failures < 7)
            ++m_consecutive_frame_failures;
        if (m_consecutive_frame_failures == 3
            && m_supported_render_enums.GetAutoFitEnum() != DrawCommonHelper::RenderType::DEFAULT)
        {
            theApp.m_taskbar_data.disable_d2d = true;
            CCommon::WriteLog(L"Repeated frame submission failure; requesting a GDI window rebuild.", theApp.m_log_path.c_str());
            if (theApp.m_pMainWnd != nullptr && ::IsWindow(theApp.m_pMainWnd->GetSafeHwnd()))
                ::PostMessage(theApp.m_pMainWnd->GetSafeHwnd(), WM_REOPEN_TASKBAR_WND,
                    reinterpret_cast<WPARAM>(GetSafeHwnd()), 0);
        }
        const unsigned int delay = DrawCommonHelper::FrameRetryDelay(m_consecutive_frame_failures);
        if (delay != 0 && !m_render_retry_pending)
            m_render_retry_pending = SetTimer(TASKBAR_RENDER_RETRY_TIMER, delay, nullptr) != 0;
    }
    else if (frame_result.succeeded)
    {
        m_consecutive_frame_failures = 0;
        if (m_render_retry_pending)
            KillTimer(TASKBAR_RENDER_RETRY_TIMER);
        m_render_retry_pending = false;
    }
}

void CTaskBarDlg::AddHisToList(CommonDisplayItem item_type, int current_usage_percent)
{
    int& data_count{ m_history_data_count[item_type] };
    std::list<int>& list = m_map_history_data[item_type];
    //将数累加到加链表的头部，直到添加的数据数量达到TASKBAR_GRAPH_STEP的倍数时计算平均数
    if (data_count % TASKBAR_GRAPH_STEP == 0)
    {
        //计算前面累加的TASKBAR_GRAPH_STEP个数据的平均数
        if (!list.empty())
            list.front() /= TASKBAR_GRAPH_STEP;
        //将新的数据添加到末尾
        list.push_front(current_usage_percent);
    }
    else
    {
        //数累加到加链表的头部
        list.front() += current_usage_percent;
    }
    size_t graph_max_length = m_item_rects[item_type].Width();
    //判断是否超过最大长度，如果超过，将链表尾部数据移除
    if (list.size() > graph_max_length)
    {
        list.pop_back();
    }
    data_count++;
}

int CTaskBarDlg::CalculateNetspeedPercent(unsigned __int64 net_speed)
{
    int percet = 0;
    unsigned __int64 max_value{ theApp.m_taskbar_data.GetNetspeedFigureMaxValueInBytes() };

    if (net_speed >= max_value)
        percet = 100;
    else if (max_value > 0)
        percet = net_speed * 100 / max_value;
    return percet;
}

bool CTaskBarDlg::CheckClickedItem(CPoint point)
{
    for (const auto& item : m_item_rects)
    {
        if (item.second.PtInRect(point))
        {
            m_clicked_item = item.first;
            return true;
        }
    }
    m_clicked_item = TDI_UP;
    return false;
}

void CTaskBarDlg::TryDrawGraph(IDrawCommon& drawer, const CRect& value_rect, CommonDisplayItem item_type)
{
    std::list<int>& list = m_map_history_data[item_type];
    COLORREF graph_color = theApp.m_taskbar_data.GetUsageGraphColor();
    if (theApp.m_taskbar_data.show_graph_dashed_box)
        drawer.DrawRectOutLine(value_rect, graph_color, 1, true);
    int i{ -1 };
    for (int value : list)
    {
        i++;
        if (i == 0)     //不绘制链表头部的数据，因为在累加中，还未取平均数
            continue;
        if (i >= value_rect.Width())
            break;
        //限制范围
        if (value > 100)
            value = 100;
        if (value < 0)
            value = 0;
        //从右往左画线
        CPoint start_point = CPoint(value_rect.right - i, value_rect.bottom);
        int height = value * value_rect.Height() / 100;
        drawer.DrawLine(start_point, height, graph_color);
    }
}

void CTaskBarDlg::OnClose()
{
    // TODO: 在此添加消息处理程序代码和/或调用默认值
    //S6: 强制隐藏悬停面板(含解除钉住),防止置顶面板残留
    CSpeedGraphTip::Instance().ForceHide();
    m_graph_tip_showing = false;

    const HWND main_hwnd = theApp.m_pMainWnd != nullptr ? theApp.m_pMainWnd->GetSafeHwnd() : nullptr;
    const UINT_PTR instance_token = GetInstanceToken();
    CDialogEx::OnClose();
    //在 HWND 已销毁后再通知宿主，宿主可安全清理 m_tBarDlg，不会在本窗口
    //OnClose 调用栈内删除 this。
    if (::IsWindow(main_hwnd))
        ::PostMessage(main_hwnd, WM_TASKBAR_WND_CLOSED, instance_token, 0);
}

void CTaskBarDlg::OnLButtonUp(UINT nFlags, CPoint point)
{
    // TODO: 在此添加消息处理程序代码和/或调用默认值
    if (CheckClickedItem(point) && m_clicked_item.IsPlugin() && m_clicked_item.PluginItem() != nullptr)
    {
        ITMPlugin* plugin = theApp.m_plugins.GetPluginByItem(m_clicked_item.PluginItem());
        if (plugin != nullptr && plugin->GetAPIVersion() >= 3)
        {
            if (m_clicked_item.PluginItem()->OnMouseEvent(IPluginItem::MT_LCLICKED, point.x, point.y, (void*)GetSafeHwnd(), IPluginItem::MF_TASKBAR_WND) != 0)
                return;
        }
    }

    CDialogEx::OnLButtonUp(nFlags, point);
}

afx_msg LRESULT CTaskBarDlg::OnExitmenuloop(WPARAM wParam, LPARAM lParam)
{
    m_menu_popuped = false;
    return 0;
}


afx_msg LRESULT CTaskBarDlg::OnTabletQuerysystemgesturestatus(WPARAM wParam, LPARAM lParam)
{
    return 0;
}


BOOL CTaskBarDlg::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
    if (zDelta >= 120 || zDelta <= -120)
    {
        CPoint point = pt;
        ScreenToClient(&point);
        ITMPlugin* plugin{};
        bool is_plugin_item_clicked = (CheckClickedItem(point) && m_clicked_item.IsPlugin() && m_clicked_item.PluginItem() != nullptr);
        if (is_plugin_item_clicked)
        {
            plugin = theApp.m_plugins.GetPluginByItem(m_clicked_item.PluginItem());
            if (plugin != nullptr && plugin->GetAPIVersion() >= 3)
            {
                IPluginItem::MouseEventType type;
                if (zDelta > 0)
                    type = IPluginItem::MT_WHEEL_UP;
                else
                    type = IPluginItem::MT_WHEEL_DOWN;
                if (m_clicked_item.PluginItem()->OnMouseEvent(type, point.x, point.y, (void*)GetSafeHwnd(), IPluginItem::MF_TASKBAR_WND) != 0)
                    return TRUE;
            }
        }
    }

    return CDialogEx::OnMouseWheel(nFlags, zDelta, pt);
}
