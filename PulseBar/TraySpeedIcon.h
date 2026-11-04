#pragma once

//托盘区网速显示:把上传/下载速度渲染成通知区图标。
//上半部分绿色显示上传，下半部分红色显示下载；数值自动选 K/M 单位。
class CTraySpeedIcon
{
public:
    CTraySpeedIcon();
    ~CTraySpeedIcon();

    //返回当前网速图标（含缓存命中）；失败返回 nullptr，调用方负责使用品牌图标回退。
    //size: 通知区图标的实际像素尺寸，由 DPI 决定。
    HICON GetSpeedIcon(unsigned __int64 up_speed, unsigned __int64 down_speed, int size);

    //丢弃当前图标(切回静态托盘图标前调用)
    void DestroyIconInternal() { Destroy(); }

private:
    void Destroy();

    HICON m_hIcon{};            //当前已生成的图标
    unsigned __int64 m_last_up{};    //上次渲染的上传速度
    unsigned __int64 m_last_down{};  //上次渲染的下载速度
    int m_last_size{};               //上次渲染的尺寸
    CFont m_font;                    //渲染数值的小字体
    int m_font_size{};               //当前字体对应的尺寸
};
