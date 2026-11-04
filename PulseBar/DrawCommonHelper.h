#pragma once
#include "IDrawCommon.h"

//用于双缓冲绘图的类
class CDrawDoubleBuffer final : public IDrawBuffer
{
public:
    CDrawDoubleBuffer(CDC* pDC, CRect rect)
        : m_pDC(pDC), m_rect(rect)
    {
        if (m_pDC != nullptr)
        {
            m_memDC.CreateCompatibleDC(NULL);
            m_memBitmap.CreateCompatibleBitmap(pDC, rect.Width(), rect.Height());
            m_pOldBit = m_memDC.SelectObject(&m_memBitmap);
        }
    }

    ~CDrawDoubleBuffer()
    {
        if (m_pDC != nullptr)
        {
            m_pDC->BitBlt(m_rect.left, m_rect.top, m_rect.Width(), m_rect.Height(), &m_memDC, 0, 0, SRCCOPY);
            m_memDC.SelectObject(m_pOldBit);
            m_memBitmap.DeleteObject();
            m_memDC.DeleteDC();
        }
    }

    CDC* GetMemDC()
    {
        return &m_memDC;
    }

private:
    CDC* m_pDC{};
    CDC m_memDC;
    CBitmap m_memBitmap;
    CBitmap* m_pOldBit{};
    CRect m_rect;
};

//优化版:线程相关 DC 缓存复用 MemDC,尺寸变化时才重建 Bitmap。
//任务栏窗口以 GDI 模式每帧构造本对象,原版每帧都要
//CreateCompatibleDC + CreateCompatibleBitmap + DeleteDC(约10次/秒)。
//MemDC 的创建成本高且与线程相关,这里用 thread_local 缓存复用;
//Bitmap 只要尺寸不变也复用(必要时 ResizeDIB 由重新创建替代,但仅在尺寸变化时)。
class CDrawDoubleBufferCached final : public IDrawBuffer
{
public:
    CDrawDoubleBufferCached(CDC* pDC, CRect rect)
        : m_pDC(pDC), m_rect(rect)
    {
        if (m_pDC != nullptr)
        {
            Cache& cache = GetCache();
            if (cache.memDC.GetSafeHdc() == nullptr)
            {
                cache.memDC.CreateCompatibleDC(NULL);
            }
            //尺寸变化(或首次)时才重建位图
            if (cache.width != rect.Width() || cache.height != rect.Height())
            {
                if (cache.pOldBit != nullptr)
                    cache.memDC.SelectObject(cache.pOldBit);
                cache.bitmap.DeleteObject();
                cache.bitmap.CreateCompatibleBitmap(pDC, rect.Width(), rect.Height());
                cache.pOldBit = cache.memDC.SelectObject(&cache.bitmap);
                cache.width = rect.Width();
                cache.height = rect.Height();
            }
            else if (cache.pOldBit == nullptr)
            {
                cache.pOldBit = cache.memDC.SelectObject(&cache.bitmap);
            }
            m_memDC = &cache.memDC;
            m_pCache = &cache;
        }
    }

    ~CDrawDoubleBufferCached()
    {
        if (m_pDC != nullptr && m_pCache != nullptr)
        {
            //把内存图输出到屏幕
            m_pDC->BitBlt(m_rect.left, m_rect.top, m_rect.Width(), m_rect.Height(), m_memDC, 0, 0, SRCCOPY);
            //MemDC 与 Bitmap 保留在缓存中供下一帧复用,不销毁
        }
    }

    CDC* GetMemDC()
    {
        return m_memDC;
    }

private:
    struct Cache
    {
        CDC memDC;
        CBitmap bitmap;
        CBitmap* pOldBit{};
        int width{};
        int height{};
    };
    //MemDC 与线程关联(每线程一个 GDI 句柄表),用 thread_local 保证安全
    static Cache& GetCache()
    {
        thread_local Cache cache;
        return cache;
    }

    CDC* m_pDC{};
    CDC* m_memDC{};
    Cache* m_pCache{};
    CRect m_rect;
};

namespace DrawCommonHelper
{
    UINT ProccessTextFormat(CRect rect, CSize text_length, IDrawCommon::Alignment align, bool multi_line) noexcept;

    //根据图片拉伸模式，计算绘制图片的实际位置
    //image_size[int]：图片的原始大小
    //start_point[int][out]：绘制区域的起始位置
    //size[int][out]：绘制区域的大小
    //stretch_mode[int]：拉伸模式
    void ImageDrawAreaConvert(CSize image_size, CPoint& start_point, CSize& size, IDrawCommon::StretchMode stretch_mode);

};
