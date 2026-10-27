// BEGIN DALAMUD CUSTOM [FONT]

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui.h"
#ifndef IMGUI_DISABLE
#include "imgui_internal.h"

namespace ImStb
{
#include "imstb_textedit.h"
}

float ImGui::InputTextCalcCharWidth(ImGuiInputTextState* obj, int line_start_idx, int char_idx, float newline_width)
{
    ImGuiContext& g = *obj->Ctx;
    const char* text_end = obj->TextSrc + obj->TextLen;
    const char* row = obj->TextSrc + line_start_idx;
    unsigned int c;
    const int c_len = ImTextCharFromUtf8(&c, row + char_idx, text_end);
    if (c == '\n')
        return newline_width;
    ImTextDesc desc;
    GetTextDesc(&desc, obj->WrapWidth, ImTextLayoutFlags_StopOnNewLine | ImTextLayoutFlags_WrapKeepBlanks);
    ImTextLayout* layout;
    if (CreateTextLayout(g.Font, &desc, row, text_end, &layout) < 0)
        return 0.0f;
    ImTextCaret caret0, caret1;
    layout->lpVtbl->GetCaret(layout, char_idx, false, &caret0);
    layout->lpVtbl->GetCaret(layout, char_idx + c_len, true, &caret1);
    layout->lpVtbl->Release(layout);
    return ImMax(caret1.InlinePos - caret0.InlinePos, 0.0f);
}

static ImTextLayout* InputTextLineLayout(const char* buf, const char* buf_end, int offset, float wrap_width, int* out_bol, int* out_eol, int* out_eol_len)
{
    ImGuiContext& g = *GImGui;
    offset = ImClamp(offset, 0, (int)(buf_end - buf));
    const char* bol = buf + offset;
    while (bol > buf && bol[-1] != '\n')
        bol--;
    const char* lf = (const char*)ImMemchr(buf + offset, '\n', buf_end - (buf + offset));
    const char* eol = lf == NULL ? buf_end : (lf > bol && lf[-1] == '\r') ? lf - 1 : lf;
    *out_bol = (int)(bol - buf);
    *out_eol = (int)(eol - buf);
    *out_eol_len = lf == NULL ? 0 : (int)(lf + 1 - eol);
    ImTextDesc desc;
    ImGui::GetTextDesc(&desc, wrap_width, ImTextLayoutFlags_WrapKeepBlanks);
    ImTextLayout* layout;
    ImGui::CreateTextLayout(g.Font, &desc, bol, eol, &layout);
    return layout;
}

static int InputTextLayoutLineCount(ImTextLayout* layout)
{
    return layout != NULL ? ImMax(layout->lpVtbl->GetLines(layout, NULL), 1) : 1;
}

static int InputTextRowOf(ImGuiTextIndex* line_index, int offset)
{
    int lo = 0, hi = line_index->Offsets.Size;
    while (lo < hi)
    {
        const int mid = (lo + hi) / 2;
        if (line_index->Offsets[mid] < offset)
            lo = mid + 1;
        else
            hi = mid;
    }
    return ImMin(lo, ImMax(line_index->Offsets.Size - 1, 0));
}

static int InputTextStepGrapheme(const char* buf, const char* buf_end, int offset, int dir, float wrap_width)
{
    const int len = (int)(buf_end - buf);
    int bol, eol, eol_len;
    ImTextLayout* layout = InputTextLineLayout(buf, buf_end, offset, wrap_width, &bol, &eol, &eol_len);
    int result;
    if (dir > 0 && offset >= eol)
        result = eol + eol_len;
    else if (dir < 0 && offset > eol) // CR(here)LF
        result = eol;
    else if (dir < 0 && offset <= bol)
        result = bol == 0 ? 0 : (bol >= 2 && buf[bol - 2] == '\r') ? bol - 2 : bol - 1;
    else if (layout == NULL)
        result = ImClamp(offset + dir, 0, len);
    else
    {
        ImTextCaret caret, moved;
        layout->lpVtbl->GetCaret(layout, offset - bol, false, &caret);
        layout->lpVtbl->Move(layout, &caret, ImTextMoveFlags_Grapheme, dir, 0, -1.0f, &moved);
        result = bol + moved.TextOffset;
    }
    if (layout != NULL)
        layout->lpVtbl->Release(layout);
    return result;
}

static int InputTextMoveRows(const char* buf, const char* buf_end, int offset, bool prefer_previous_line, float wrap_width, int rows, float* io_goal_x, bool* out_previous_line)
{
    int bol, eol, eol_len;
    ImTextLayout* layout = InputTextLineLayout(buf, buf_end, offset, wrap_width, &bol, &eol, &eol_len);
    ImTextCaret caret{};
    if (layout != NULL)
        layout->lpVtbl->GetCaret(layout, offset - bol, prefer_previous_line, &caret);
    else
        caret.TextOffset = ImMin(offset, eol) - bol;
    const float goal = *io_goal_x >= 0.0f ? *io_goal_x : caret.InlinePos;
    const int dir = rows < 0 ? -1 : 1;
    for (int n = ImAbs(rows); n > 0; n--)
    {
        const bool within = dir > 0 ? caret.LineIndex + 1 < InputTextLayoutLineCount(layout) : caret.LineIndex > 0;
        if (within && layout != NULL)
        {
            ImTextCaret moved;
            layout->lpVtbl->Move(layout, &caret, ImTextMoveFlags_BlockRelative, 0, dir, goal, &moved);
            caret = moved;
            continue;
        }

        if (dir > 0 ? eol_len == 0 : bol == 0)
        {
            if (dir > 0)
            {
                if (layout != NULL)
                    layout->lpVtbl->GetCaret(layout, eol - bol, false, &caret);
                else
                    caret.TextOffset = eol - bol;
            }
            break;
        }
        if (layout != NULL)
            layout->lpVtbl->Release(layout);
        layout = InputTextLineLayout(buf, buf_end, dir > 0 ? eol + eol_len : bol - 1, wrap_width, &bol, &eol, &eol_len);
        if (layout == NULL)
        {
            caret = ImTextCaret{};
            continue;
        }
        ImTextCaret first;
        layout->lpVtbl->GetCaret(layout, 0, false, &first);
        layout->lpVtbl->Move(layout, &first, dir > 0 ? ImTextMoveFlags_BlockFirst : ImTextMoveFlags_BlockLast, 0, 0, goal, &caret);
    }
    if (layout != NULL)
        layout->lpVtbl->Release(layout);
    *io_goal_x = goal;
    *out_previous_line = caret.PreviousLine;
    return bol + caret.TextOffset;
}

static int InputTextMoveLineStart(const char* buf, const char* buf_end, int offset, bool prefer_previous_line, float wrap_width)
{
    int bol, eol, eol_len;
    ImTextLayout* layout = InputTextLineLayout(buf, buf_end, offset, wrap_width > 0.0f ? wrap_width : 0.0f, &bol, &eol, &eol_len);
    int result = bol;
    if (layout != NULL)
    {
        if (wrap_width > 0.0f)
        {
            ImTextCaret caret, moved;
            layout->lpVtbl->GetCaret(layout, offset - bol, prefer_previous_line, &caret);
            layout->lpVtbl->Move(layout, &caret, ImTextMoveFlags_InlineBegin, 0, 0, -1.0f, &moved);
            if (bol + moved.TextOffset != offset)
                result = bol + moved.TextOffset;
        }
        layout->lpVtbl->Release(layout);
    }
    return result;
}

static int InputTextMoveLineEnd(const char* buf, const char* buf_end, int offset, bool prefer_previous_line, float wrap_width)
{
    int bol, eol, eol_len;
    ImTextLayout* layout = InputTextLineLayout(buf, buf_end, offset, wrap_width > 0.0f ? wrap_width : 0.0f, &bol, &eol, &eol_len);
    int result = eol;
    if (layout != NULL)
    {
        if (wrap_width > 0.0f)
        {
            ImTextCaret caret, moved;
            layout->lpVtbl->GetCaret(layout, offset - bol, prefer_previous_line, &caret);
            layout->lpVtbl->Move(layout, &caret, ImTextMoveFlags_InlineEnd, 0, 0, -1.0f, &moved);
            if (bol + moved.TextOffset != offset)
                result = bol + moved.TextOffset;
        }
        layout->lpVtbl->Release(layout);
    }
    return result;
}

static int InputTextLocate(const char* buf, const char* buf_end, float wrap_width, float x, float y, bool* out_previous_line)
{
    ImGuiContext& g = *GImGui;
    *out_previous_line = false;
    float top = 0.0f;
    y = ImMax(y, 0.0f);
    for (const char* bol = buf; ; )
    {
        const char* eol = (const char*)ImMemchr(bol, '\n', buf_end - bol);
        if (eol == NULL)
            eol = buf_end;
        int line_bol, line_eol, line_eol_len;
        ImTextLayout* layout = NULL;
        int rows = 1;
        if (wrap_width > 0.0f && eol > bol)
        {
            layout = InputTextLineLayout(buf, buf_end, (int)(bol - buf), wrap_width, &line_bol, &line_eol, &line_eol_len);
            rows = InputTextLayoutLineCount(layout);
        }
        if (y < top + rows * g.FontSize || eol >= buf_end)
        {
            if (layout == NULL)
                layout = InputTextLineLayout(buf, buf_end, (int)(bol - buf), wrap_width, &line_bol, &line_eol, &line_eol_len);
            if (layout == NULL)
                return (int)(bol - buf);
            ImTextCaret hit;
            layout->lpVtbl->HitTest(layout, ImVec2(x, y - top), &hit);
            layout->lpVtbl->Release(layout);
            *out_previous_line = hit.PreviousLine;
            return (int)(bol - buf) + hit.TextOffset;
        }
        if (layout != NULL)
            layout->lpVtbl->Release(layout);
        top += rows * g.FontSize;
        bol = eol + 1;
    }
}

float ImGui::InputTextCaretX(const char* buf, const char* buf_end, int offset, bool prefer_previous_line, float wrap_width)
{
    int bol, eol, eol_len;
    ImTextLayout* layout = InputTextLineLayout(buf, buf_end, offset, wrap_width, &bol, &eol, &eol_len);
    if (layout == NULL)
        return 0.0f;
    ImTextCaret caret;
    layout->lpVtbl->GetCaret(layout, offset - bol, prefer_previous_line, &caret);
    layout->lpVtbl->Release(layout);
    return caret.InlinePos;
}

int ImGui::InputTextBuildWrappedLineIndex(ImGuiTextIndex* line_index, const char* buf, const char* buf_end, float wrap_width, int max_output_buffer_size)
{
    int size = 0;
    for (const char* s = buf; s < buf_end; )
    {
        int bol, eol, eol_len;
        ImTextLayout* layout = InputTextLineLayout(buf, buf_end, (int)(s - buf), wrap_width, &bol, &eol, &eol_len);
        const ImTextLine* lines = NULL;
        const int line_count = layout != NULL && eol > bol ? layout->lpVtbl->GetLines(layout, &lines) : 0;
        if (line_count == 0 && size++ <= max_output_buffer_size)
            line_index->Offsets.push_back(bol);
        for (int n = 0; n < line_count; n++)
            if (size++ <= max_output_buffer_size)
                line_index->Offsets.push_back(bol + lines[n].TextBegin);
        if (layout != NULL)
            layout->lpVtbl->Release(layout);
        s = buf + eol + eol_len;
    }
    return size;
}

void ImGui::InputTextRenderWrapped(ImDrawList* draw_list, ImGuiTextIndex* line_index, const char* buf, const char* buf_end, float wrap_width, int line_n0, int line_n1, const ImVec2& origin, ImU32 col, const ImVec4& clip_rect)
{
    ImGuiContext& g = *GImGui;
    if (line_n0 >= line_n1)
        return;
    int bol, eol, eol_len;
    ImTextLayout* layout = InputTextLineLayout(buf, buf_end, (int)(line_index->get_line_begin(buf, line_n0) - buf), wrap_width, &bol, &eol, &eol_len);
    for (int row = InputTextRowOf(line_index, bol); row < line_n1; )
    {
        if (layout != NULL)
        {
            layout->lpVtbl->Render(layout, draw_list, ImVec2(origin.x, origin.y + row * g.FontSize), NULL, col, &clip_rect, ImTextRenderFlags_CpuFineClip);
            row += InputTextLayoutLineCount(layout);
            layout->lpVtbl->Release(layout);
            layout = NULL;
        }
        else
            row++;
        if (eol_len == 0)
            break;
        layout = InputTextLineLayout(buf, buf_end, eol + eol_len, wrap_width, &bol, &eol, &eol_len);
    }
    if (layout != NULL)
        layout->lpVtbl->Release(layout);
}

void ImGui::InputTextRenderSelection(ImDrawList* draw_list, ImGuiTextIndex* line_index, const char* buf, const char* buf_end, float wrap_width, int sel_begin, int sel_end, int line_n0, int line_n1, const ImVec2& origin, float offy_up, float offy_dn, float eol_width, const ImRect& clip_rect, ImU32 col)
{
    ImGuiContext& g = *GImGui;
    if (line_n0 >= line_n1 || sel_begin >= sel_end)
        return;
    ImVector<ImVec4> rects;
    int bol, eol, eol_len;
    ImTextLayout* layout = InputTextLineLayout(buf, buf_end, (int)(line_index->get_line_begin(buf, line_n0) - buf), wrap_width, &bol, &eol, &eol_len);
    for (int row = InputTextRowOf(line_index, bol); row < line_n1; )
    {
        const int sb = ImMax(sel_begin, bol);
        const int se = ImMin(sel_end, eol);
        const bool eol_selected = sel_begin <= eol && sel_end > eol && eol_len > 0; // So we can see selected empty lines
        rects.resize(0);
        if (sb < se && layout != NULL)
        {
            rects.resize(layout->lpVtbl->GetRangeRects(layout, sb - bol, se - bol, NULL, 0));
            layout->lpVtbl->GetRangeRects(layout, sb - bol, se - bol, rects.Data, rects.Size);
        }
        if (eol_selected)
        {
            ImTextCaret end{};
            if (layout != NULL)
                layout->lpVtbl->GetCaret(layout, eol - bol, true, &end);
            if (rects.Size > 0)
                rects.back().z += eol_width;
            else
                rects.push_back(ImVec4(end.InlinePos, end.LineIndex * g.FontSize, end.InlinePos + eol_width, (end.LineIndex + 1) * g.FontSize));
        }
        for (const ImVec4& r : rects)
        {
            const int r_row = row + (int)(r.y / g.FontSize + 0.5f);
            if (r_row < line_n0 || r_row >= line_n1 || r.z <= r.x)
                continue;
            ImRect rect;
            rect.Min.x = origin.x + r.x;
            rect.Min.y = origin.y + r_row * g.FontSize;
            rect.Max.x = origin.x + r.z;
            rect.Max.y = rect.Min.y + offy_dn + g.FontSize;
            rect.Min.y += offy_up;
            rect.ClipWith(clip_rect);
            draw_list->AddRectFilled(rect.Min, rect.Max, col);
        }
        row += InputTextLayoutLineCount(layout);
        if (layout != NULL)
            layout->lpVtbl->Release(layout);
        layout = NULL;
        if (eol_len == 0)
            break;
        layout = InputTextLineLayout(buf, buf_end, eol + eol_len, wrap_width, &bol, &eol, &eol_len);
    }
    if (layout != NULL)
        layout->lpVtbl->Release(layout);
}

// Same as stb_textedit's.
static bool InputTextHasSelection(const ImStbTexteditState* s) { return s->select_start != s->select_end; }

static void InputTextClamp(const ImGuiInputTextState* str, ImStbTexteditState* s)
{
    const int n = str->TextLen;
    if (InputTextHasSelection(s))
    {
        s->select_start = ImMin(s->select_start, n);
        s->select_end = ImMin(s->select_end, n);
        if (s->select_start == s->select_end)
            s->cursor = s->select_start;
    }
    s->cursor = ImMin(s->cursor, n);
}

static void InputTextPrepSelectionAtCursor(ImStbTexteditState* s)
{
    if (!InputTextHasSelection(s))
        s->select_start = s->select_end = s->cursor;
    else
        s->cursor = s->select_end;
}

static void InputTextMoveToFirst(ImStbTexteditState* s)
{
    if (!InputTextHasSelection(s))
        return;
    s->cursor = s->select_end = s->select_start = ImMin(s->select_start, s->select_end);
    s->has_preferred_x = 0;
}

static void InputTextMoveToLast(const ImGuiInputTextState* str, ImStbTexteditState* s)
{
    if (!InputTextHasSelection(s))
        return;
    s->cursor = s->select_start = s->select_end = ImMin(ImMax(s->select_start, s->select_end), str->TextLen);
    s->has_preferred_x = 0;
}

bool ImGui::InputTextMoveCursor(ImGuiInputTextState* str, ImGuiInputTextMove move, bool shift)
{
    ImStbTexteditState* s = str->Stb;
    if (s->single_line && (move == ImGuiInputTextMove_Up || move == ImGuiInputTextMove_Down))
        move = move == ImGuiInputTextMove_Up ? ImGuiInputTextMove_Left : ImGuiInputTextMove_Right;
    const char* buf = str->TextSrc;
    const char* buf_end = buf + str->TextLen;
    const bool prefer_previous_line = str->LastMoveDirectionLR == ImGuiDir_Right;
    switch (move)
    {
    case ImGuiInputTextMove_Left:
    case ImGuiInputTextMove_Right:
    {
        const int dir = move == ImGuiInputTextMove_Left ? -1 : 1;
        InputTextClamp(str, s);
        if (shift)
        {
            InputTextPrepSelectionAtCursor(s);
            s->cursor = s->select_end = InputTextStepGrapheme(buf, buf_end, s->select_end, dir, str->WrapWidth);
        }
        else if (InputTextHasSelection(s))
        {
            if (dir < 0)
                InputTextMoveToFirst(s);
            else
                InputTextMoveToLast(str, s);
        }
        else
            s->cursor = InputTextStepGrapheme(buf, buf_end, s->cursor, dir, str->WrapWidth);
        InputTextClamp(str, s);
        s->has_preferred_x = 0;
        return true;
    }
    case ImGuiInputTextMove_Up:
    case ImGuiInputTextMove_Down:
    case ImGuiInputTextMove_PageUp:
    case ImGuiInputTextMove_PageDown:
    {
        const int dir = move == ImGuiInputTextMove_Up || move == ImGuiInputTextMove_PageUp ? -1 : 1;
        const int rows = move == ImGuiInputTextMove_PageUp || move == ImGuiInputTextMove_PageDown ? s->row_count_per_page : 1;
        if (shift)
            InputTextPrepSelectionAtCursor(s);
        else if (dir < 0)
            InputTextMoveToFirst(s);
        else
            InputTextMoveToLast(str, s);
        InputTextClamp(str, s);
        float goal_x = s->has_preferred_x ? s->preferred_x : -1.0f;
        bool previous_line = false;
        s->cursor = InputTextMoveRows(buf, buf_end, s->cursor, prefer_previous_line, str->WrapWidth, dir * rows, &goal_x, &previous_line);
        s->has_preferred_x = 1;
        s->preferred_x = goal_x;
        str->LastMoveDirectionLR = (ImS8)(previous_line ? ImGuiDir_Right : ImGuiDir_Left);
        if (shift)
            s->select_end = s->cursor;
        return true;
    }
    case ImGuiInputTextMove_LineStart:
    case ImGuiInputTextMove_LineEnd:
    {
        InputTextClamp(str, s);
        if (shift)
            InputTextPrepSelectionAtCursor(s);
        else if (move == ImGuiInputTextMove_LineStart)
            InputTextMoveToFirst(s);
        else
            InputTextMoveToLast(str, s);
        const float wrap_width = s->single_line ? 0.0f : str->WrapWidth;
        s->cursor = move == ImGuiInputTextMove_LineStart
            ? InputTextMoveLineStart(buf, buf_end, s->cursor, prefer_previous_line, wrap_width)
            : InputTextMoveLineEnd(buf, buf_end, s->cursor, prefer_previous_line, wrap_width);
        if (shift)
            s->select_end = s->cursor;
        s->has_preferred_x = 0;
        return true;
    }
    }
    return false;
}

void ImGui::InputTextClick(ImGuiInputTextState* str, float x, float y, bool drag)
{
    ImStbTexteditState* s = str->Stb;
    if (s->single_line)
        y = 0.0f;
    bool previous_line = false;
    const int offset = InputTextLocate(str->TextSrc, str->TextSrc + str->TextLen, str->WrapWidth, x, y, &previous_line);
    if (drag)
    {
        if (s->select_start == s->select_end)
            s->select_start = s->cursor;
        s->cursor = s->select_end = offset;
    }
    else
    {
        s->cursor = s->select_start = s->select_end = offset;
        s->has_preferred_x = 0;
    }
    str->LastMoveDirectionLR = (ImS8)(previous_line ? ImGuiDir_Right : ImGuiDir_Left);
}

#endif // #ifndef IMGUI_DISABLE
// END DALAMUD CUSTOM [FONT]
