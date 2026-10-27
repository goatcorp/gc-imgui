// BEGIN DALAMUD CUSTOM [FONT]

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui.h"
#ifndef IMGUI_DISABLE
#include "imgui_internal.h"

void ImGui::GetTextDesc(ImTextDesc* out_desc, float wrap_width, ImTextLayoutFlags flags, const ImDrawList* draw_list)
{
    const ImDrawListSharedData* data = draw_list != NULL ? draw_list->_Data : &GImGui->DrawListSharedData;
    const float density = draw_list != NULL ? draw_list->_InvFringeScale : GImGui->CurrentPixelDensity;
    *out_desc = {};
    out_desc->Size = data->FontSize;
    out_desc->Density = density > 0.0f ? density : 1.0f;
    out_desc->LineHeight = -1.0f;
    out_desc->WrapWidth = wrap_width;
    out_desc->MaxWidth = FLT_MAX;
    out_desc->Flags = flags;
    if (data->Context != NULL && data->Context->InputTextPasswordMaskCount > 0)
        out_desc->Flags |= ImTextLayoutFlags_PasswordMask;
}

void ImGui::SetTextCallbacks(ImTextLayoutFn layout, ImFontMetricsFn metrics)
{
    ImGuiContext& g = *GImGui;
    g.DrawListSharedData.TextLayoutFn = layout;
    g.DrawListSharedData.FontMetricsFn = metrics;
}

ImHRESULT ImGui::CreateTextLayout(ImFont* font, const ImTextDesc* desc, const char* text, const char* text_end, ImTextLayout** out_layout)
{
    ImGuiContext& g = *GImGui;
    IM_ASSERT(g.DrawListSharedData.TextLayoutFn != NULL && "Text layout callback is not set");
    *out_layout = NULL;
    if (font == NULL)
        return (ImHRESULT)0x80004003; // E_POINTER
    if (text_end == NULL)
        text_end = text + ImStrlen(text);
    ImTextDesc default_desc;
    if (desc == NULL)
    {
        GetTextDesc(&default_desc);
        desc = &default_desc;
    }
    const ImHRESULT hr = g.DrawListSharedData.TextLayoutFn(font, desc, text, text_end, out_layout);
    if (hr < 0 && *out_layout != NULL)
    {
        (*out_layout)->lpVtbl->Release(*out_layout);
        *out_layout = NULL;
    }
    return hr;
}

ImTextMetrics ImGui::GetFontMetrics()
{
    ImGuiContext& g = *GImGui;
    ImTextMetrics metrics{};
    if (g.Font != NULL && g.DrawListSharedData.FontMetricsFn != NULL)
        g.DrawListSharedData.FontMetricsFn(g.Font, g.FontSize, g.CurrentPixelDensity > 0.0f ? g.CurrentPixelDensity : 1.0f, &metrics);
    return metrics;
}

ImVec2 ImGui::CalcTextSizeEx(ImFont* font, const ImTextDesc* desc, const char* text_begin, const char* text_end_display, const char* text_end, const char** out_remaining, ImVec2* out_offset)
{
    if (text_end == NULL)
        text_end = text_begin + ImStrlen(text_begin);
    if (text_end_display == NULL)
        text_end_display = text_end;
    ImTextDesc default_desc;
    if (desc == NULL)
    {
        GetTextDesc(&default_desc);
        desc = &default_desc;
    }
    ImTextLayout* layout = NULL;
    if (text_begin == text_end || CreateTextLayout(font, desc, text_begin, text_end, &layout) < 0)
    {
        ImGuiContext& g = *GImGui;
        float line_height = desc->LineHeight > 0.0f ? desc->LineHeight : -desc->LineHeight * desc->Size;
        if (desc->LineHeight == 0.0f)
        {
            ImTextMetrics metrics{};
            if (font != NULL && g.DrawListSharedData.FontMetricsFn != NULL)
                g.DrawListSharedData.FontMetricsFn(font, desc->Size, desc->Density, &metrics);
            line_height = metrics.LineHeight > 0.0f ? metrics.LineHeight : desc->Size;
        }
        if (out_remaining != NULL)
            *out_remaining = text_begin;
        if (out_offset != NULL)
            *out_offset = ImVec2(0.0f, line_height);
        return ImVec2(0.0f, line_height);
    }

    const int text_length = layout->lpVtbl->GetTextLength(layout);
    const int display_offset = ImMin((int)(text_end_display - text_begin), text_length);
    ImTextCaret caret;
    layout->lpVtbl->GetCaret(layout, display_offset, true, &caret);
    const ImTextLine* lines = NULL;
    const int line_count = layout->lpVtbl->GetLines(layout, &lines);

    ImVec2 size(caret.InlinePos, 0.0f);
    for (int line_n = 0; line_n < caret.LineIndex && line_n < line_count; line_n++)
        size.x = ImMax(size.x, lines[line_n].Size.x);
    if (caret.LineIndex >= 0 && caret.LineIndex < line_count && display_offset >= lines[caret.LineIndex].TextEnd)
        size.x = ImMax(size.x, lines[caret.LineIndex].Size.x);
    size.y = caret.Pos.y + ((caret.InlinePos > 0.0f || caret.LineIndex == 0) ? caret.Length : 0.0f);
    if (out_offset != NULL)
        *out_offset = ImVec2(caret.InlinePos, caret.Pos.y + caret.Length);
    if (out_remaining != NULL)
        *out_remaining = text_begin + text_length;
    layout->lpVtbl->Release(layout);
    return size;
}

const char* ImGui::CalcWrappedLineEnd(const char* line_begin, const char* text_end, float wrap_width, ImTextLayoutFlags flags)
{
    ImGuiContext& g = *GImGui;
    if (line_begin >= text_end)
        return text_end;
    ImTextDesc desc;
    GetTextDesc(&desc, wrap_width, flags | ImTextLayoutFlags_StopOnNewLine);
    ImTextLayout* layout;
    if (CreateTextLayout(g.Font, &desc, line_begin, text_end, &layout) < 0)
        return text_end;
    const ImTextLine* lines = NULL;
    const char* end = (layout->lpVtbl->GetLines(layout, &lines) > 0) ? line_begin + lines[0].TextEnd : text_end;
    layout->lpVtbl->Release(layout);
    return end;
}

void ImGui::RenderTextEllipsisEx(ImDrawList* draw_list, const ImVec2& pos_min, const ImVec2& pos_max, float ellipsis_max_x, const char* text, const char* text_end_full, const ImVec2* text_size_if_known, const ImTextTransform* transform, const ImVec2& transformed_pos_min)
{
    ImGuiContext& g = *GImGui;
    if (text_end_full == NULL)
        text_end_full = FindRenderedTextEnd(text);
    const ImVec2 text_size = text_size_if_known ? *text_size_if_known : CalcTextSize(text, text_end_full, false, 0.0f);

    ImTextDesc desc;
    GetTextDesc(&desc);
    if (text_size.x > pos_max.x - pos_min.x)
    {
        desc.Flags |= ImTextLayoutFlags_Ellipsis;
        desc.MaxWidth = ImMax(ImMax(pos_max.x, ellipsis_max_x) - pos_min.x, 1.0f);
    }
    ImFont* font = draw_list->_Data->Font;
    ImTextLayout* layout;
    if (text != text_end_full && CreateTextLayout(font, &desc, text, text_end_full, &layout) >= 0)
    {
        const ImVec2 shift = transformed_pos_min - pos_min;
        const ImVec4 clip_rect(pos_min.x + shift.x, pos_min.y + shift.y, pos_max.x + shift.x, pos_max.y + shift.y);
        layout->lpVtbl->Render(layout, draw_list, transformed_pos_min, transform, GetColorU32(ImGuiCol_Text), &clip_rect, ImTextRenderFlags_CpuFineClip);
        layout->lpVtbl->Release(layout);
    }

    if (g.LogEnabled)
        LogRenderedText(&pos_min, text, text_end_full);
}

void ImGui::AddTextEx(ImDrawList* draw_list, ImFont* font, const ImTextDesc* desc, const ImVec2& pos, ImU32 col, const char* text_begin, const char* text_end, const ImVec4* clip_rect, ImTextRenderFlags render_flags, const ImTextTransform* transform)
{
    if ((col & IM_COL32_A_MASK) == 0)
        return;
    if (text_end == NULL)
        text_end = text_begin + ImStrlen(text_begin);
    if (text_begin == text_end)
        return;
    ImDrawListSharedData* data = draw_list->_Data;
    if (font == NULL)
        font = data->Font;
    IM_ASSERT(font != NULL && "No font set. Set io.FontDefault or pass a font.");
    IM_ASSERT(data->TextLayoutFn != NULL && "No text layout callback. Call ImGui::SetTextCallbacks().");
    ImTextDesc default_desc;
    if (desc == NULL)
    {
        GetTextDesc(&default_desc, 0.0f, ImTextLayoutFlags_None, draw_list);
        desc = &default_desc;
    }
    ImTextLayout* layout = NULL;
    if (data->TextLayoutFn(font, desc, text_begin, text_end, &layout) < 0 || layout == NULL)
    {
        if (layout != NULL)
            layout->lpVtbl->Release(layout);
        return;
    }
    layout->lpVtbl->Render(layout, draw_list, pos, transform, col, clip_rect, render_flags);
    layout->lpVtbl->Release(layout);
}

void ImGui::AddTextLayout(ImDrawList* draw_list, ImTextLayout* layout, const ImVec2& pos, ImU32 col, const ImVec4* clip_rect, ImTextRenderFlags render_flags, const ImTextTransform* transform)
{
    if ((col & IM_COL32_A_MASK) == 0 || layout == NULL)
        return;
    layout->lpVtbl->Render(layout, draw_list, pos, transform, col, clip_rect, render_flags);
}

ImU64 ImGui::TextLayout(ImTextLayout* layout, ImGuiID id)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems || layout == NULL)
        return 0;

    ImVec4 box;
    layout->lpVtbl->GetBoundingBox(layout, &box, NULL);
    const ImVec2 pos = window->DC.CursorPos;
    const ImRect bb(pos, pos + ImVec2(box.z, box.w));
    ItemSize(bb.GetSize(), 0.0f);
    if (id == 0)
        id = window->GetIDFromRectangle(bb);
    if (!ItemAdd(bb, id))
        return 0;

    ImU64 clicked = 0;
    if (ItemHoverable(bb, id, g.LastItemData.ItemFlags))
    {
        ImTextCaret hit;
        layout->lpVtbl->HitTest(layout, g.IO.MousePos - pos, &hit);
        if (hit.Inside && hit.UserData != 0)
        {
            SetMouseCursor(ImGuiMouseCursor_Hand);
            if (hit.RunIndex >= 0)
            {
                const ImVec4* rects = NULL;
                const int rect_count = layout->lpVtbl->GetRunRects(layout, hit.RunIndex, &rects);
                for (int n = 0; n < rect_count; n++)
                    window->DrawList->AddRectFilled(pos + ImVec2(rects[n].x, rects[n].y), pos + ImVec2(rects[n].z, rects[n].w), GetColorU32(ImGuiCol_HeaderHovered));
            }
            if (IsMouseClicked(ImGuiMouseButton_Left))
                clicked = hit.UserData;
        }
    }
    layout->lpVtbl->Render(layout, window->DrawList, pos, NULL, GetColorU32(ImGuiCol_Text), NULL, ImTextRenderFlags_None);
    return clicked;
}

bool ImGui::BeginTextTexture(ImDrawList* draw_list, ImTextureRef tex_ref)
{
    if (draw_list->_CmdHeader.TexRef == tex_ref)
        return false;

    const ImTextureData* shape_tex = draw_list->_Data->ShapeTex;
    const ImTextureData* next = tex_ref._TexData;
    const ImVector<ImTextureRef>& stack = draw_list->_TextureStack;
    const bool current_has_block = stack.Size > 0 && (!(draw_list->_CmdHeader.TexRef == stack.back()) || (shape_tex != NULL && draw_list->_CmdHeader.TexRef._TexData == shape_tex));
    const bool next_has_block = shape_tex != NULL && next != NULL && next->Width == shape_tex->Width && next->Height == shape_tex->Height;
    if (current_has_block && next_has_block)
    {
        // Skip full push handling because both textures got the shape block
        draw_list->_CmdHeader.TexRef = tex_ref;
        draw_list->_OnChangedTexture();
        return false;
    }
    draw_list->PushTexture(tex_ref);
    return true;
}

void ImGui::EndTextTexture(ImDrawList* draw_list, bool pushed)
{
    if (pushed)
        draw_list->PopTexture();
}

ImTextureData* ImGui::CreateShapeTexture(ImDrawListSharedData* shared_data, int width, int height)
{
    IM_ASSERT(shared_data->ShapeTex == NULL);
    const ImVec2i size = GetShapeTexDataSize();
    IM_ASSERT(width >= size.x && height >= size.y && "Shape texture too small for the shape block.");
    ImTextureData* tex = IM_NEW(ImTextureData)();
    tex->Create(ImTextureFormat_RGBA32, width, height);
    RenderShapeTexData(tex, &shared_data->ShapeTexUvs);
    shared_data->ShapeTex = tex;
    shared_data->TexUvWhitePixel = shared_data->ShapeTexUvs.WhitePixel;
    shared_data->TexUvLines = shared_data->ShapeTexUvs.Lines;
    shared_data->TexUvCorners = shared_data->ShapeTexUvs.Corners;
    RegisterUserTexture(tex);
    return tex;
}

void ImGui::SetShapeTextureSize(int width, int height)
{
    ImGuiContext& g = *GImGui;
    ImTextureData* tex = g.DrawListSharedData.ShapeTex;
    if (tex != NULL && tex->Width == width && tex->Height == height)
        return;
    IM_ASSERT((tex == NULL || tex->Status == ImTextureStatus_WantCreate) && "Call SetShapeTextureSize() before the first frame is rendered.");
    DestroyShapeTexture(&g.DrawListSharedData);
    CreateShapeTexture(&g.DrawListSharedData, width, height);
}

void ImGui::DestroyShapeTexture(ImDrawListSharedData* shared_data)
{
    ImTextureData* tex = shared_data->ShapeTex;
    if (tex == NULL)
        return;
    UnregisterUserTexture(tex);
    shared_data->ShapeTex = NULL;
    IM_DELETE(tex);
}

ImTextureData* ImGui::GetShapeTexture()
{
    return GImGui->DrawListSharedData.ShapeTex;
}

#endif // #ifndef IMGUI_DISABLE
// END DALAMUD CUSTOM [FONT]
