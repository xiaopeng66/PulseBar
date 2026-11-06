#include "stdafx.h"
#include "DrawCommonHelper.h"
#include <limits>

UINT DrawCommonHelper::ProccessTextFormat(CRect rect, CSize text_length, IDrawCommon::Alignment align, bool multi_line) noexcept
{
    UINT result; // CDC::DrawText()函数的文本格式
    if (multi_line)
        result = DT_EDITCONTROL | DT_WORDBREAK | DT_NOPREFIX;
    else
        result = DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX;

    if (text_length.cx > rect.Width()) //如果文本宽度超过了矩形区域的宽度，设置了居中时左对齐
    {
        if (align == IDrawCommon::Alignment::RIGHT)
            result |= DT_RIGHT;
    }
    else
    {
        switch (align)
        {
        case IDrawCommon::Alignment::RIGHT:
            result |= DT_RIGHT;
            break;
        case IDrawCommon::Alignment::CENTER:
            result |= DT_CENTER;
            break;
        }
    }
    return result;
}

void DrawCommonHelper::ImageDrawAreaConvert(CSize image_size, CPoint& start_point, CSize& size, IDrawCommon::StretchMode stretch_mode)
{
    if (image_size.cx<=0 || image_size.cy<=0 || size.cx<0 || size.cy<0)
    { size=CSize(0,0); return; }
    if (size.cx==0 || size.cy==0)
    { size=image_size; return; }
    if (stretch_mode!=IDrawCommon::StretchMode::FILL && stretch_mode!=IDrawCommon::StretchMode::FIT)
        return;
    const auto bounded = [](long long value) {
        const long long high=(std::numeric_limits<int>::max)();
        const long long low=(std::numeric_limits<int>::min)();
        return static_cast<int>(value>high ? high : (value<low ? low : value));
    };
    const long long source_width=image_size.cx, source_height=image_size.cy;
    const int target_width=size.cx, target_height=size.cy;
    const bool wider=source_width*target_height > source_height*target_width;
    if ((stretch_mode==IDrawCommon::StretchMode::FILL && wider)
        || (stretch_mode==IDrawCommon::StretchMode::FIT && !wider))
    {
        const long long scaled=source_width*target_height/source_height;
        size.cx=bounded(scaled>0 ? scaled : 1);
        start_point.x=bounded(static_cast<long long>(start_point.x)+(static_cast<long long>(target_width)-size.cx)/2);
    }
    else
    {
        const long long scaled=source_height*target_width/source_width;
        size.cy=bounded(scaled>0 ? scaled : 1);
        start_point.y=bounded(static_cast<long long>(start_point.y)+(static_cast<long long>(target_height)-size.cy)/2);
    }
}
