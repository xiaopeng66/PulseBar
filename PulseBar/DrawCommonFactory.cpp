#include "stdafx.h"
#include "DrawCommonFactory.h"

auto GetInterfaceFromAllInvolvedDrawCommonObjects(
    AllInvolvedDrawCommonObjectsStorage& ref_object_storage,
    DrawCommonHelper::RenderType render_type,
    std::initializer_list<TaggedAllInvolvedDrawCommonObjectsInitializer> initializer_list)
    -> std::tuple<IDrawBuffer*, IDrawCommon*>
{
    auto& ref_draw_buffer_and_draw_common_objects = *static_cast<InvolvedDrawCommonStorages*>(
        static_cast<void*>(std::addressof(ref_object_storage)));
    auto p_draw_buffer = static_cast<IDrawBuffer*>(
        static_cast<void*>(
            std::addressof(ref_draw_buffer_and_draw_common_objects.m_draw_buffer_union_storage)));
    auto p_draw_common = static_cast<IDrawCommon*>(
        static_cast<void*>(
            std::addressof(ref_draw_buffer_and_draw_common_objects.m_draw_common_union_storage)));

    for (auto& initializer : initializer_list)
    {
        if (initializer.first == render_type)
        {
            try
            {
                initializer.second(p_draw_buffer, p_draw_common);
            }
            catch (...)
            {
                DrawCommonHelper::MarkFrameSubmitFailed(E_FAIL);
                throw;
            }
            return {ref_object_storage.m_unique_draw_buffer.get(), ref_object_storage.m_unique_draw_common.get()};
        }
    }
    throw std::runtime_error{TRAFFICMONITOR_ERROR_STR("No matching render type for initializer")};
}