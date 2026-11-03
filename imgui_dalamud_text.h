// BEGIN DALAMUD CUSTOM [FONT]

#ifndef IMGUI_VERSION
#error "include imgui.h instead."
#endif
#ifdef IMGUI_DALAMUD_TEXT_H_INCLUDED
#error "include imgui.h instead."
#endif
#define IMGUI_DALAMUD_TEXT_H_INCLUDED

#ifndef IMGUI_SHAPE_TEX_DEFAULT_SIZE
#define IMGUI_SHAPE_TEX_DEFAULT_SIZE    2048
#endif

typedef long ImHRESULT;             // HRESULT
struct ImGUID;                      // GUID (layout compatible)
struct ImTextDesc;
struct ImTextLayout;
struct ImTextMetrics;
struct ImTextTransform;
typedef int ImTextLayoutFlags;      // -> enum ImTextLayoutFlags_
typedef int ImTextRenderFlags;      // -> enum ImTextRenderFlags_
typedef int ImTextMoveFlags;        // -> enum ImTextMoveFlags_

typedef ImHRESULT (*ImTextLayoutFn)(ImFont* font, const ImTextDesc* desc, const char* text, const char* text_end, ImTextLayout** out_layout);
typedef void (*ImFontMetricsFn)(ImFont* font, float size, float density, ImTextMetrics* out_metrics);

namespace ImGui
{
    IMGUI_API void          SetTextCallbacks(ImTextLayoutFn layout, ImFontMetricsFn metrics);
    IMGUI_API ImTextMetrics GetFontMetrics();
    IMGUI_API ImTextureData* GetShapeTexture();
    IMGUI_API void          SetShapeTextureSize(int width, int height);

    // Draws a layout as an item. Returns the clicked run's UserData this frame, or 0.
    IMGUI_API ImU64         TextLayout(ImTextLayout* layout, ImGuiID id = 0);

    // font NULL = the draw list's. desc NULL = the draw list's font size and pixel density, no wrapping.
    IMGUI_API void          AddTextEx(ImDrawList* draw_list, ImFont* font, const ImTextDesc* desc, const ImVec2& pos, ImU32 col, const char* text_begin, const char* text_end = NULL, const ImVec4* clip_rect = NULL, ImTextRenderFlags render_flags = 0, const ImTextTransform* transform = NULL);
    IMGUI_API void          AddTextLayout(ImDrawList* draw_list, ImTextLayout* layout, const ImVec2& pos, ImU32 col, const ImVec4* clip_rect = NULL, ImTextRenderFlags render_flags = 0, const ImTextTransform* transform = NULL);

    // PushTexture/PopTexture but cheaper for drawing text.
    IMGUI_API bool          BeginTextTexture(ImDrawList* draw_list, ImTextureRef tex_ref);
    IMGUI_API void          EndTextTexture(ImDrawList* draw_list, bool pushed);
}

// For ImTextDesc.
enum ImTextLayoutFlags_
{
    ImTextLayoutFlags_None              = 0,
    ImTextLayoutFlags_WrapKeepBlanks    = 1 << 1,
    ImTextLayoutFlags_StopOnNewLine     = 1 << 2,
    ImTextLayoutFlags_Ellipsis          = 1 << 3,
    ImTextLayoutFlags_PasswordMask      = 1 << 4,
};

// For drawing a layout.
enum ImTextRenderFlags_
{
    ImTextRenderFlags_None              = 0,
    ImTextRenderFlags_CpuFineClip       = 1 << 0,
    ImTextRenderFlags_NoPixelSnap       = 1 << 1,
    ImTextRenderFlags_CompleteGlyphs    = 1 << 2,   // Disable async rasterization
};

// Effectively Matrix2x2.
struct ImTextTransform
{
    ImVec2      AxisX;
    ImVec2      AxisY;
};

struct ImTextDesc
{
    const char* Locale;             // NULL = user default.
    float       Size;               // Size in logical pixels.
    float       Density;            // Device pixels per logical pixel.
    float       LineHeight;         // > 0: in pixels. < 0: value * Size. 0: ImTextMetrics::LineHeight.
    float       WrapWidth;          // <= 0: no wrapping.
    float       MaxWidth;           // FLT_MAX: unlimited.
    ImTextLayoutFlags Flags;
    ImWchar32   MaskChar;           // 0 = '*'. For ImTextLayoutFlags_PasswordMask
};

struct ImTextMetrics
{
    float       Ascent;             // Above the baseline.
    float       Descent;            // Below the baseline.
    float       LineHeight;         // Ascent + Descent + line gap.
    float       UnderlinePosition;  // Below the baseline.
    float       UnderlineThickness;
};

// Positions are relative to the layout origin at top left, untransformed.
struct ImTextLine
{
    int         TextBegin;          // Byte offsets.
    int         TextEnd;            // Before the line break and trailing whitespaces.
    int         TextNext;           // Where the next line starts.
    float       Baseline;           // From the line box start, along the block axis.
    ImVec2      Pos;                // Line box.
    ImVec2      Size;
};

// For GetCaret, HitTest, or Move.
struct ImTextCaret
{
    ImU64       UserData;           // Coresponding to the run/object under the caret or from a hit-test.
    ImVec2      Pos;                // Start of the caret line.
    float       Length;             // Along the block axis.
    float       InlinePos;          // Along the inline axis.
    int         TextOffset;
    int         LineIndex;          // Line.
    int         RunIndex;           // Run of the grapheme at the caret, or -1.
    int         ObjectIndex;        // Coresponding to the run/object under the caret or from a hit-test.
    bool        PreviousLine;       // True if the stored values indicate that the caret is at the end of the line at a wrap point.
    bool        Inside;             // True if the point is inside a grapheme's box from a hit-test.
};

enum ImTextMoveFlags_
{
    // Inline axis.
    ImTextMoveFlags_InlineKeep      = 0,           // SEEK_CUR
    ImTextMoveFlags_InlineBegin     = 1 << 0,      // SEEK_SET
    ImTextMoveFlags_InlineEnd       = 2 << 0,      // SEEK_END
    ImTextMoveFlags_InlineMask_     = 3 << 0,

    // Block axis.
    ImTextMoveFlags_BlockRelative   = 0,           // SEEK_CUR
    ImTextMoveFlags_BlockFirst      = 1 << 2,      // SEEK_SET
    ImTextMoveFlags_BlockLast       = 2 << 2,      // SEEK_END
    ImTextMoveFlags_BlockMask_      = 3 << 2,

    // Unit of dx.
    ImTextMoveFlags_Codepoint       = 0,
    ImTextMoveFlags_Grapheme        = 1 << 4,
    ImTextMoveFlags_WordStart       = 2 << 4,
    ImTextMoveFlags_WordEnd         = 3 << 4,
    ImTextMoveFlags_UnitMask_       = 3 << 4,

    ImTextMoveFlags_BlockClampKeepInline = 1 << 6, // If set, "Up" on the first line keeps the caret; otherwise effectively "Home".
    ImTextMoveFlags_DxWithinLine    = 1 << 7,      // If set, "Left" on the first offset within a line does nothing; otherwise goes to the end of the previous line.

    ImTextMoveFlags_Begin           = ImTextMoveFlags_InlineBegin | ImTextMoveFlags_BlockFirst,
    ImTextMoveFlags_End             = ImTextMoveFlags_InlineEnd | ImTextMoveFlags_BlockLast,
};

// Immutable laid out text.
struct ImTextLayoutVtbl
{
    ImHRESULT       (*QueryInterface)(ImTextLayout* self, const ImGUID* iid, void** out_object);
    unsigned long   (*AddRef)(ImTextLayout* self);
    unsigned long   (*Release)(ImTextLayout* self);

    void            (*GetBoundingBox)(ImTextLayout* self, ImVec4* out_layout, ImVec4* out_ink);    // out_layout: from advances and line heights. out_ink: what is drawn. Either may be NULL.
    int             (*GetTextLength)(ImTextLayout* self);
    int             (*GetEllipsisOffset)(ImTextLayout* self);                                       // -1 if no ellipsizing is required.
    int             (*GetLines)(ImTextLayout* self, const ImTextLine** out_lines);                  // out_lines is owned by the layout.

    void            (*GetCaret)(ImTextLayout* self, int text_offset, bool prefer_previous_line, ImTextCaret* out_caret); // text_offset: At a byte offset. prefer_previous_line: at a wrap point, the end of the previous line.
    void            (*HitTest)(ImTextLayout* self, ImVec2 point, ImTextCaret* out_caret);
    void            (*Move)(ImTextLayout* self, const ImTextCaret* from, ImTextMoveFlags flags, int dx, int dy, float preferred_inline_pos, ImTextCaret* out_caret); // preferred_inline_pos: < 0 = from->InlinePos.
    int             (*GetRangeRects)(ImTextLayout* self, int text_begin, int text_end, ImVec4* out_rects, int capacity); // One rect per line. On a line the range continues onto the next one from (through its line break, or past its wrap point), up to the wrap width (the layout box without wrapping), and the trailing blanks past it with ImTextLayoutFlags_WrapKeepBlanks. Returns the number of rects in the range; stores up to capacity of them.

    void            (*Render)(ImTextLayout* self, ImDrawList* draw_list, ImVec2 pos, const ImTextTransform* transform, ImU32 col, const ImVec4* clip_rect, ImTextRenderFlags flags);
    void            (*RenderRange)(ImTextLayout* self, ImDrawList* draw_list, ImVec2 pos, const ImTextTransform* transform, ImU32 col, const ImVec4* clip_rect, ImTextRenderFlags flags, int text_begin, int text_end);

    int             (*GetRunCount)(ImTextLayout* self);
    int             (*GetRunRects)(ImTextLayout* self, int run_index, const ImVec4** out_rects);   // One rect per line the run spans. out_rects is owned by the layout.
};
struct ImTextLayout { const ImTextLayoutVtbl* lpVtbl; };

// END DALAMUD CUSTOM [FONT]
