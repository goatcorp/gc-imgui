// ReSharper disable CppClangTidyClangDiagnosticPadded
#include <algorithm>
#include <fstream>
#ifndef _MSC_VER
#error "class layout assumed to be in MSVC"
#endif

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "example_text.h"
#include "imgui_internal.h"
#include <bit>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <winerror.h>

#pragma warning(push)
#pragma warning(disable: 4505) // unreferenced function with internal linkage has been removed
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "imstb_truetype.h"
#pragma warning(pop)

namespace {
    struct GlyphImage {
        ImTextureData* Page = nullptr;
        int X = 0, Y = 0, W = 0, H = 0;
        int OffX = 0, OffY = 0; // top-left relative to the pen on the baseline, in device pixels
    };
}

class ExampleFont {
    static std::list<ExampleFont> s_instances;

    std::vector<uint8_t> m_data;
    std::list<ImTextureData> m_pages;
    std::vector<uint8_t> m_alphaBuffer;
    std::unordered_map<std::uint64_t, GlyphImage> m_images;
    stbtt_fontinfo m_info = {};
    float m_ascent = 0, m_descent = 0, m_lineGap = 0;
    int m_pageCursorX = 0, m_pageCursorY = 0, m_pageCursorRowHeight = 0;

public:
    static ExampleFont* New(std::vector<uint8_t> buf) {
        return &s_instances.emplace_back(std::move(buf));
    }

    static ExampleFont* From(ImFont* font) {
        for (auto& p : s_instances)
            if (reinterpret_cast<ImFont*>(&p) == font)
                return &p;
        return nullptr;
    }

    static void Destroy(ExampleFont* font) {
        std::erase_if(s_instances, [font](const auto& p) { return &p == font; });
    }

    ExampleFont(std::vector<uint8_t> buf) {
        m_data = std::move(buf);

        if (!stbtt_InitFont(&m_info, m_data.data(), stbtt_GetFontOffsetForIndex(m_data.data(), 0)))
            throw std::runtime_error("Failed to initialize font");

        int ascent, descent, lineGap;
        stbtt_GetFontVMetrics(&m_info, &ascent, &descent, &lineGap);
        m_ascent = static_cast<float>(ascent);
        m_descent = static_cast<float>(descent);
        m_lineGap = static_cast<float>(lineGap);
    }

    ExampleFont(const ExampleFont&) = delete;
    ExampleFont(ExampleFont&&) = delete;
    ExampleFont& operator=(const ExampleFont&) = delete;
    ExampleFont& operator=(ExampleFont&&) = delete;

    ~ExampleFont() {
        for (auto& tex : m_pages) {
            if (ImGui::GetCurrentContext() != nullptr)
                ImGui::UnregisterUserTexture(&tex);
        }
    }

    [[nodiscard]] float Scale(float size) const { return stbtt_ScaleForPixelHeight(&m_info, size); }

    [[nodiscard]] int Ascent(float scale) const { return static_cast<int>(ImTrunc(m_ascent * scale + 0.5f)); }

    [[nodiscard]] int FindGlyph(uint32_t c) const { return stbtt_FindGlyphIndex(&m_info, static_cast<int>(c)); }

    [[nodiscard]] float Advance(int glyph, float scale) const {
        int advance, lsb;
        stbtt_GetGlyphHMetrics(&m_info, glyph, &advance, &lsb);
        return advance * scale;
    }

    [[nodiscard]] ImTextMetrics Metrics(float size) const {
        const auto scale = Scale(size);
        return
        {
            .Ascent = m_ascent * scale,
            .Descent = -m_descent * scale,
            .LineHeight = (m_ascent - m_descent + m_lineGap) * scale,
            .UnderlinePosition = ImMax(1.0f, ImTrunc(size / 10.0f)),
            .UnderlineThickness = ImMax(1.0f, ImTrunc(size / 16.0f)),
        };
    }

    const auto& GetGlyphImage(int glyph, float device_scale) {
        const auto key = (static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(device_scale)) << 32) | static_cast<std::uint32_t>(glyph);
        if (const auto it = m_images.find(key); it != m_images.end())
            return it->second;

        int x0, y0, x1, y1;
        stbtt_GetGlyphBitmapBox(&m_info, glyph, device_scale, device_scale, &x0, &y0, &x1, &y1);

        auto image = GlyphImage{.W = x1 - x0, .H = y1 - y0, .OffX = x0, .OffY = y0};
        if (image.W > 0 && image.H > 0) {
            if (!m_pages.empty() && m_pageCursorX + image.W + 1 > m_pages.back().Width) {
                m_pageCursorX = 0;
                m_pageCursorY += m_pageCursorRowHeight;
                m_pageCursorRowHeight = 0;
            }
            if (m_pages.empty() || m_pageCursorY + image.H + 1 > m_pages.back().Height)
                AddPage();
            auto& tex = m_pages.back();
            image.Page = &tex;
            image.X = m_pageCursorX;
            image.Y = m_pageCursorY;
            m_pageCursorX += image.W + 1;
            m_pageCursorRowHeight = ImMax(m_pageCursorRowHeight, image.H + 1);

            m_alphaBuffer.resize(0);
            m_alphaBuffer.resize(static_cast<size_t>(image.W) * image.H);
            stbtt_MakeGlyphBitmap(&m_info, m_alphaBuffer.data(), image.W, image.H, image.W, device_scale, device_scale, glyph);
            for (auto y = 0; y < image.H; ++y)
                for (auto x = 0; x < image.W; ++x)
                    *static_cast<ImU32*>(tex.GetPixelsAt(image.X + x, image.Y + y)) = IM_COL32(255, 255, 255, m_alphaBuffer[static_cast<size_t>(y) * image.W + x]);
            ImTextureDataQueueUpload(&tex, image.X, image.Y, image.W, image.H);
        }
        return m_images[key] = image;
    }

private:
    void AddPage() {
        const auto& shape_tex = *ImGui::GetShapeTexture();
        auto& page = m_pages.emplace_back();
        page.Create(ImTextureFormat_RGBA32, shape_tex.Width, shape_tex.Height);
        ImGui::RenderShapeTexData(&page);
        ImGui::RegisterUserTexture(&page);

        m_pageCursorX = 0;
        m_pageCursorY = ImGui::GetShapeTexDataSize().y + 1;
        m_pageCursorRowHeight = 0;
    }
};

std::list<ExampleFont> ExampleFont::s_instances; // NOLINT(bugprone-throwing-static-initialization)

namespace {
    constexpr bool IsBlank(uint32_t c) { return c == ' ' || c == '\t' || c == 0x3000; }
    constexpr bool IsOtherLineBreak(uint32_t c) { return c == '\r' || c == 0x0B || c == 0x0C || c == 0x85 || c == 0x2028 || c == 0x2029; }
    constexpr bool IsWordChar(uint32_t c) { return c >= 0x80 || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
    constexpr bool IsWide(uint32_t c) { return c >= 0x2E80; }

    enum class ItemKind : uint8_t { Glyph, Blank, Newline, Control, Ellipsis };

    struct Item {
        int Offset = 0, Length = 0; // bytes
        uint32_t Cp = 0;
        int Glyph = 0;
        int Line = 0;
        float X = 0.0f, Advance = 0.0f;
        ItemKind Kind = ItemKind::Glyph;

        [[nodiscard]] bool IsDrawn() const { return Kind == ItemKind::Glyph || Kind == ItemKind::Ellipsis; }
        [[nodiscard]] int End() const { return Offset + Length; }

        [[nodiscard]] int OffsetAt(float u) const { return u < X + Advance * 0.5f ? Offset : -1; }
    };

    struct Line {
        int ItemBegin = 0, ItemEnd = 0; // including trailing blanks and the newline
        int TextBegin = 0, TextEnd = 0, TextNext = 0;
        float Width = 0.0f; // up to TextEnd
        float FullWidth = 0.0f; // including trailing blanks
        float BlockPos = 0.0f, BlockSize = 0.0f, Baseline = 0.0f;
        bool EndsWithNewline = false;

        [[nodiscard]] bool ContainsBlock(float v) const { return v >= BlockPos && v < BlockPos + BlockSize; }
    };

    struct __declspec(novtable) IImTextLayout {
        virtual ImHRESULT __cdecl QueryInterface(const ImGUID* iid, void** out_object) = 0;
        virtual uint32_t __cdecl AddRef() = 0;
        virtual uint32_t __cdecl Release() = 0;
        virtual void __cdecl GetBoundingBox(ImVec4* out_layout, ImVec4* out_ink) = 0;
        virtual int __cdecl GetTextLength() = 0;
        virtual int __cdecl GetEllipsisOffset() = 0;
        virtual int __cdecl GetLines(const ImTextLine** out_lines) = 0;
        virtual void __cdecl GetCaret(int text_offset, bool prefer_previous_line, ImTextCaret* out_caret) = 0;
        virtual void __cdecl HitTest(ImVec2 point, ImTextCaret* out_caret) = 0;
        virtual void __cdecl Move(const ImTextCaret* from, ImTextMoveFlags flags, int dx, int dy, float preferred_inline_pos, ImTextCaret* out_caret) = 0;
        virtual int __cdecl GetRangeRects(int text_begin, int text_end, ImVec4* out_rects, int capacity) = 0;
        virtual void __cdecl Render(ImDrawList* draw_list, ImVec2 pos, const ImTextTransform* transform, ImU32 col, const ImVec4* clip_rect, ImTextRenderFlags flags) = 0;
        virtual void __cdecl RenderRange(ImDrawList* draw_list, ImVec2 pos, const ImTextTransform* transform, ImU32 col, const ImVec4* clip_rect, ImTextRenderFlags flags, int text_begin, int text_end) = 0;
        virtual int __cdecl GetRunCount() = 0;
        virtual int __cdecl GetRunRects(int run_index, const ImVec4** out_rects) = 0;

        ImTextLayout* AsNative() { return reinterpret_cast<ImTextLayout*>(this); }
        virtual ~IImTextLayout() = default;
    };

    static_assert(sizeof(IImTextLayout) == sizeof(ImTextLayout));

    class ExampleTextLayout final : public IImTextLayout {
        uint32_t m_refCount = 1;
        ExampleFont& m_font;
        ImTextDesc m_desc;
        float m_scale; // font units to logical pixels
        std::vector<uint32_t> m_codepoints;
        std::vector<int> m_codepointOffsets;
        std::vector<Item> m_items;
        std::vector<Line> m_lines;
        std::vector<ImTextLine> m_linesPublic;
        int m_textBytes = 0, m_textLength = 0, m_ellipsisByteOffset = -1;
        ImVec2 size;

        ExampleTextLayout(ExampleFont& in_font, const auto& in_desc, std::string_view text)
            : m_font(in_font)
            , m_desc(in_desc)
            , m_scale(in_font.Scale(in_desc.Size)) {
            Build(text);
        }

    public:
        ~ExampleTextLayout() override = default;

        static ExampleTextLayout* New(ExampleFont& font, const auto& desc, std::string_view text) {
            return IM_NEW(ExampleTextLayout)(font, desc, text);
        }

        ImHRESULT __cdecl QueryInterface(const ImGUID*, void** out_object) override {
            if (out_object != nullptr)
                *out_object = nullptr;
            return E_NOINTERFACE;
        }

        uint32_t __cdecl AddRef() override {
            return ++m_refCount;
        }

        uint32_t __cdecl Release() override {
            const auto refs = --m_refCount;
            if (refs == 0)
                IM_DELETE(this);
            return refs;
        }

        void __cdecl GetBoundingBox(ImVec4* out_layout, ImVec4* out_ink) override {
            const ImVec4 box(0.0f, 0.0f, size.x, size.y);
            if (out_layout != nullptr)
                *out_layout = box;
            if (out_ink != nullptr)
                *out_ink = box;
        }

        int __cdecl GetTextLength() override {
            return m_textLength;
        }

        int __cdecl GetEllipsisOffset() override {
            return m_ellipsisByteOffset;
        }

        int __cdecl GetLines(const ImTextLine** out) override {
            if (out != nullptr)
                *out = m_linesPublic.empty() ? nullptr : m_linesPublic.data();
            return static_cast<int>(m_linesPublic.size());
        }

        void __cdecl GetCaret(int text_offset, bool prefer_previous_line, ImTextCaret* out_caret) override {
            *out_caret = MakeCaret(text_offset, LineOf(text_offset, prefer_previous_line));
        }

        void __cdecl HitTest(ImVec2 point, ImTextCaret* out_caret) override {
            const auto u = point.x, v = point.y;
            const auto it = std::ranges::find_if(m_lines, [v](const auto& l) { return v < l.BlockPos + l.BlockSize; });
            const auto line_index = it != m_lines.end()
                                        ? static_cast<int>(std::distance(m_lines.begin(), it))
                                        : static_cast<int>(m_lines.size()) - 1;
            const auto& line = m_lines[line_index];
            auto offset = -1, under = -1;
            for (auto k = line.ItemBegin; k < line.ItemEnd; ++k) {
                const auto& item = m_items[k];
                if (item.Kind == ItemKind::Newline)
                    break;
                if (u >= item.X && u < item.X + item.Advance)
                    under = k;
                if (offset < 0)
                    offset = item.OffsetAt(u);
            }
            if (offset < 0)
                offset = line.EndsWithNewline ? line.TextEnd : ImMax(line.TextEnd, line.TextNext == line.TextBegin ? line.TextEnd : LineEndOffset(line));

            *out_caret = MakeCaret(offset, line_index);
            out_caret->Inside = under >= 0 && line.ContainsBlock(v) && u >= 0.0f;
        }

        void __cdecl Move(const ImTextCaret* from, ImTextMoveFlags flags, int dx, int dy, float preferred_inline_pos, ImTextCaret* out_caret) override {
            const auto line_count = static_cast<int>(m_lines.size());
            const auto block = flags & ImTextMoveFlags_BlockMask_;
            auto inline_ = flags & ImTextMoveFlags_InlineMask_;
            int line_index;
            int target;
            if (block == ImTextMoveFlags_BlockRelative && dy == 0 && inline_ == ImTextMoveFlags_InlineKeep) {
                line_index = ImClamp(from->LineIndex, 0, line_count - 1);
                target = from->TextOffset;
            } else {
                line_index = block == ImTextMoveFlags_BlockFirst ? 0 : block == ImTextMoveFlags_BlockLast ? line_count - 1 : from->LineIndex + dy;
                if (line_index < 0 || line_index >= line_count) {
                    const auto past = line_index >= line_count;
                    line_index = past ? line_count - 1 : 0;
                    if ((flags & ImTextMoveFlags_BlockClampKeepInline) == 0)
                        inline_ = past ? ImTextMoveFlags_InlineEnd : ImTextMoveFlags_InlineBegin;
                }

                const auto& line = m_lines[line_index];
                if (inline_ == ImTextMoveFlags_InlineBegin)
                    target = line.TextBegin;
                else if (inline_ == ImTextMoveFlags_InlineEnd)
                    target = line.EndsWithNewline ? line.TextEnd : LineEndOffset(line);
                else {
                    const auto pos = preferred_inline_pos >= 0.0f ? preferred_inline_pos : from->InlinePos;
                    target = -1;
                    for (auto k = line.ItemBegin; k < line.ItemEnd && target < 0; ++k)
                        target = m_items[k].Kind == ItemKind::Newline ? m_items[k].Offset : m_items[k].OffsetAt(pos);
                    if (target < 0)
                        target = LineEndOffset(line);
                }
            }

            if (dx == 0) {
                *out_caret = MakeCaret(target, line_index);
                return;
            }
            auto offset = ByteOffsetStepBy(target, flags & ImTextMoveFlags_UnitMask_, dx);
            if (flags & ImTextMoveFlags_DxWithinLine) {
                const auto& line = m_lines[line_index];
                offset = ImClamp(offset, line.TextBegin, line.EndsWithNewline ? line.TextEnd : LineEndOffset(line));
                *out_caret = MakeCaret(offset, line_index);
                return;
            }
            *out_caret = MakeCaret(offset, LineOf(offset, false));
        }

        int __cdecl GetRangeRects(int text_begin, int text_end, ImVec4* out_rects, int capacity) override {
            const auto rects = out_rects != nullptr ? std::span(out_rects, static_cast<size_t>(ImMax(capacity, 0))) : std::span<ImVec4>();
            const auto edge = m_desc.WrapWidth > 0.0f ? m_desc.WrapWidth : size.x;
            const auto keep_blanks = (m_desc.Flags & ImTextLayoutFlags_WrapKeepBlanks) != 0;
            int count = 0;
            for (size_t n = 0; n < m_lines.size(); ++n) {
                const auto& line = m_lines[n];
                const auto line_end = ImMax(line.TextNext, LineEndOffset(line));
                if (text_end <= line.TextBegin || text_begin >= line_end)
                    continue;
                const auto u0 = InlinePosAt(line, ImMax(text_begin, line.TextBegin));
                const auto continues = line.EndsWithNewline ? text_end >= line_end : text_end > line_end && n + 1 < m_lines.size();
                const auto u1 = continues
                                    ? ImMax(u0, keep_blanks ? ImMax(edge, line.FullWidth) : edge)
                                    : text_end >= line_end
                                    ? line.FullWidth
                                    : InlinePosAt(line, text_end);
                if (static_cast<size_t>(count) < rects.size())
                    rects[count] = ImVec4(u0, line.BlockPos, u1, line.BlockPos + line.BlockSize);
                ++count;
            }
            return count;
        }

        void __cdecl Render(ImDrawList* draw_list, ImVec2 pos, const ImTextTransform* transform, ImU32 col, const ImVec4* clip_rect, ImTextRenderFlags flags) override { RenderRange(draw_list, pos, transform, col, clip_rect, flags, 0, -1); }

        void __cdecl RenderRange(ImDrawList* draw_list, ImVec2 pos, const ImTextTransform* transform, ImU32 col, const ImVec4* clip_rect, ImTextRenderFlags flags, int text_begin, int text_end) override {
            if (text_end < 0)
                text_end = INT_MAX;
            const auto snap = transform == nullptr && (flags & ImTextRenderFlags_NoPixelSnap) == 0;
            if (snap)
                pos = ImTrunc(pos);
            const auto clip = clip_rect != nullptr ? *clip_rect : draw_list->_CmdHeader.ClipRect;
            const auto fine_clip = transform != nullptr || (flags & ImTextRenderFlags_CpuFineClip) != 0;
            const auto density = m_desc.Density > 0.0f ? m_desc.Density : 1.0f;
            const auto device_scale = m_scale * density;
            ImTextureData* page = nullptr;
            bool pushed = false;
            for (const auto& item : m_items) {
                if (!item.IsDrawn() || item.Offset < text_begin || item.Offset >= text_end)
                    continue;
                const auto& line = m_lines[item.Line];
                if (transform == nullptr && (pos.y + line.BlockPos > clip.w || pos.y + line.BlockPos + line.BlockSize < clip.y))
                    continue;
                const auto& image = m_font.GetGlyphImage(item.Glyph, device_scale);
                if (!image.Page)
                    continue;
                const auto tex = image.Page;
                if (page != image.Page) {
                    if (page)
                        ImGui::EndTextTexture(draw_list, pushed);
                    pushed = ImGui::BeginTextTexture(draw_list, tex->GetTexRef());
                    page = image.Page;
                }

                // Align to device pixels
                const auto pen = (pos + ImVec2(item.X, line.BlockPos + line.Baseline)) * density;
                const auto pen_device = snap ? ImVec2(ImFloor(pen.x), ImFloor(pen.y + 0.5f)) : pen;
                const auto p0 = (pen_device + ImVec2(static_cast<float>(image.OffX), static_cast<float>(image.OffY))) / density;
                const auto p1 = (pen_device + ImVec2(static_cast<float>(image.OffX + image.W), static_cast<float>(image.OffY + image.H))) / density;
                const auto uv_scale = ImVec2(1.0f / tex->Width, 1.0f / tex->Height);
                const auto uv0 = ImVec2(static_cast<float>(image.X), static_cast<float>(image.Y)) * uv_scale;
                const auto uv1 = ImVec2(static_cast<float>(image.X + image.W), static_cast<float>(image.Y + image.H)) * uv_scale;
                EmitQuad(draw_list, p0, p1, uv0, uv1, col, fine_clip ? &clip : nullptr, pos, transform);
            }
            if (page)
                ImGui::EndTextTexture(draw_list, pushed);
        }

        int __cdecl GetRunCount() override {
            return 0;
        }

        int __cdecl GetRunRects(int, const ImVec4** out_rects) override {
            if (out_rects != nullptr)
                *out_rects = nullptr;
            return 0;
        }

    private:
        void Build(std::string_view text) {
            m_textBytes = static_cast<int>(text.size());
            const auto begin = text.data();
            const auto end = begin + text.size();
            for (auto p = begin; p < end;) {
                uint32_t c;
                const auto len = ImMax(ImTextCharFromUtf8(&c, p, end), 1);
                m_codepoints.push_back(c);
                m_codepointOffsets.push_back(static_cast<int>(p - begin));
                p += len;
            }
            m_codepointOffsets.push_back(m_textBytes);

            const auto mask = (m_desc.Flags & ImTextLayoutFlags_PasswordMask) != 0;
            const auto mask_char = m_desc.MaskChar != 0 ? m_desc.MaskChar : '*';
            std::vector<Item> src;
            src.reserve(m_codepoints.size());
            for (size_t i = 0; i < m_codepoints.size(); ++i) {
                const auto c = m_codepoints[i];
                Item item = {.Offset = m_codepointOffsets[i], .Length = m_codepointOffsets[i + 1] - m_codepointOffsets[i], .Cp = c};
                if (c == '\n' || IsOtherLineBreak(c)) {
                    item.Kind = c == '\n' ? ItemKind::Newline : ItemKind::Control;
                } else {
                    item.Kind = !mask && IsBlank(c) ? ItemKind::Blank : ItemKind::Glyph;
                    item.Glyph = m_font.FindGlyph(mask ? mask_char : c == '\t' ? ' ' : c);
                    item.Advance = m_font.Advance(item.Glyph, m_scale);
                }
                src.push_back(item);
            }

            Item ellipsis = {.Glyph = m_font.FindGlyph(0x2026), .Kind = ItemKind::Ellipsis};
            const auto ellipsis_count = ellipsis.Glyph != 0 ? 1 : 3;
            if (ellipsis.Glyph == 0)
                ellipsis.Glyph = m_font.FindGlyph('.');
            ellipsis.Advance = m_font.Advance(ellipsis.Glyph, m_scale);

            BreakLines(src, ellipsis, ellipsis_count);
        }

        void BreakLines(std::span<const Item> src, const auto& ellipsis_item, int ellipsis_count) {
            const auto keep_blanks = (m_desc.Flags & ImTextLayoutFlags_WrapKeepBlanks) != 0;
            const auto wrap = m_desc.WrapWidth;
            const auto max_width = m_desc.MaxWidth > 0.0f ? m_desc.MaxWidth : FLT_MAX;
            const auto use_ellipsis = (m_desc.Flags & ImTextLayoutFlags_Ellipsis) != 0 && max_width < FLT_MAX;
            const auto line_height = LineHeight();
            const auto ascent = m_font.Ascent(m_scale);
            const auto count = static_cast<int>(src.size());
            m_textLength = m_textBytes;
            auto block_pos = 0.0f;
            auto i = 0;
            auto stopped = false;
            while (true) {
                Line line = {.ItemBegin = static_cast<int>(m_items.size()), .TextBegin = i < count ? src[i].Offset : m_textBytes};
                auto x = 0.0f;
                auto break_at = -1;
                const auto line_first = i;
                auto ended_by_newline = false;
                for (; i < count; ++i) {
                    Item item = src[i];
                    if (item.Kind == ItemKind::Newline) {
                        item.X = x;
                        item.Advance = 0.0f;
                        m_items.push_back(item);
                        ++i;
                        ended_by_newline = true;
                        break;
                    }

                    const auto blank = item.Kind == ItemKind::Blank;
                    if (i > line_first && !blank && (src[i - 1].Kind == ItemKind::Blank || IsWide(item.Cp) || IsWide(src[i - 1].Cp)))
                        break_at = i;

                    if (!use_ellipsis && !blank && x + item.Advance > max_width) {
                        m_textLength = item.Offset;
                        stopped = true;
                        break;
                    }

                    if (wrap > 0.0f && (!blank || keep_blanks) && x + item.Advance > wrap && i > line_first) {
                        const auto cut = !blank && break_at > line_first ? break_at : i;
                        while (std::cmp_greater(m_items.size(), line.ItemBegin) && m_items.back().Offset >= src[cut].Offset)
                            m_items.pop_back();
                        i = cut;
                        break;
                    }

                    item.X = x;
                    x += item.Advance;
                    m_items.push_back(item);
                }

                line.ItemEnd = static_cast<int>(m_items.size());
                line.EndsWithNewline = ended_by_newline;
                const auto end_item = line.ItemEnd - (ended_by_newline ? 1 : 0);
                line.FullWidth = end_item > line.ItemBegin ? m_items[end_item - 1].X + m_items[end_item - 1].Advance : 0.0f;
                auto trailing = end_item;
                if (!keep_blanks)
                    while (trailing > line.ItemBegin && m_items[trailing - 1].Kind == ItemKind::Blank)
                        trailing--;
                line.TextEnd = trailing > line.ItemBegin ? m_items[trailing - 1].End() : line.TextBegin;
                if (ended_by_newline && keep_blanks)
                    line.TextEnd = m_items[line.ItemEnd - 1].Offset;
                line.Width = trailing > line.ItemBegin ? m_items[trailing - 1].X + m_items[trailing - 1].Advance : 0.0f;
                line.TextNext = i < count ? src[i].Offset : stopped ? m_textLength : m_textBytes;
                if (ended_by_newline)
                    line.TextNext = m_items[line.ItemEnd - 1].End();

                if (use_ellipsis && line.Width > max_width) {
                    const auto ellipsis_width = ellipsis_item.Advance * ellipsis_count;
                    auto keep = line.ItemBegin;
                    while (keep < end_item && m_items[keep].X + m_items[keep].Advance + ellipsis_width <= max_width)
                        ++keep;
                    const auto cut_offset = keep < end_item ? m_items[keep].Offset : line.TextEnd;
                    if (m_ellipsisByteOffset < 0)
                        m_ellipsisByteOffset = cut_offset;
                    auto ex = keep > line.ItemBegin ? m_items[keep - 1].X + m_items[keep - 1].Advance : 0.0f;
                    m_items.resize(keep);
                    for (auto e = 0; e < ellipsis_count; ++e) {
                        Item item = ellipsis_item;
                        item.Offset = cut_offset;
                        item.X = ex;
                        ex += item.Advance;
                        m_items.push_back(item);
                    }
                    line.ItemEnd = static_cast<int>(m_items.size());
                    line.TextEnd = cut_offset;
                    line.Width = line.FullWidth = ex;
                }

                line.BlockSize = line_height;
                line.Baseline = ascent + (line.BlockSize - m_desc.Size) * 0.5f;
                line.BlockPos = block_pos;
                block_pos += line.BlockSize;
                for (auto k = line.ItemBegin; k < line.ItemEnd; ++k)
                    m_items[k].Line = static_cast<int>(m_lines.size());
                m_lines.push_back(line);

                if (stopped || (m_desc.Flags & ImTextLayoutFlags_StopOnNewLine && (ended_by_newline || i < count))) {
                    if (!stopped)
                        m_textLength = line.TextNext;
                    break;
                }
                if (i >= count && !ended_by_newline)
                    break;
            }

            size = ImVec2(std::ranges::max(m_lines, {}, &Line::Width).Width, block_pos);
            for (const auto& l : m_lines) {
                m_linesPublic.push_back({
                    .TextBegin = l.TextBegin,
                    .TextEnd = l.TextEnd,
                    .TextNext = l.TextNext,
                    .Baseline = l.Baseline,
                    .Pos = ImVec2(0.0f, l.BlockPos),
                    .Size = ImVec2(l.Width, l.BlockSize),
                });
            }
        }

        [[nodiscard]] float LineHeight() const {
            return m_desc.LineHeight > 0.0f
                       ? m_desc.LineHeight
                       : m_desc.LineHeight < 0.0f
                       ? -m_desc.LineHeight * m_desc.Size
                       : m_font.Metrics(m_desc.Size).LineHeight;
        }

        [[nodiscard]] int LineEndOffset(const auto& line) const {
            if (line.ItemEnd <= line.ItemBegin)
                return line.TextBegin;
            const auto& last = m_items[line.ItemEnd - 1];
            return ImMax(line.TextEnd, last.Kind == ItemKind::Newline ? line.TextEnd : last.End());
        }

        [[nodiscard]] float InlinePosAt(const auto& line, int offset) const {
            for (auto i = line.ItemBegin; i < line.ItemEnd; ++i)
                if (m_items[i].Kind == ItemKind::Ellipsis || offset < m_items[i].End())
                    return m_items[i].X;
            return line.FullWidth;
        }

        [[nodiscard]] int LineOf(int offset, bool prefer_previous_line) const {
            const auto count = static_cast<int>(m_lines.size());
            for (auto n = 0; n + 1 < count; ++n) {
                const auto next_begin = m_lines[n + 1].TextBegin;
                if (offset < next_begin || (prefer_previous_line && !m_lines[n].EndsWithNewline && offset == next_begin))
                    return n;
            }
            return count - 1;
        }

        [[nodiscard]] ImTextCaret MakeCaret(int offset, int line_index) const {
            const auto& line = m_lines[line_index];
            const auto line_end = LineEndOffset(line);
            offset = ImClamp(offset, line.TextBegin, ImMax(line.TextNext, line_end));
            auto inline_pos = offset >= line_end && !line.EndsWithNewline ? line.FullWidth : InlinePosAt(line, offset);
            if (line.EndsWithNewline && offset >= line.TextEnd)
                inline_pos = line.Width;
            return
            {
                .UserData = 0,
                .Pos = ImVec2(inline_pos, line.BlockPos),
                .Length = line.BlockSize,
                .InlinePos = inline_pos,
                .TextOffset = offset,
                .LineIndex = line_index,
                .RunIndex = -1,
                .ObjectIndex = -1,
                .PreviousLine = line_index + 1 < static_cast<int>(m_lines.size()) && !line.EndsWithNewline && offset == m_lines[line_index + 1].TextBegin,
                .Inside = false,
            };
        }

        [[nodiscard]] int ByteOffsetToCodepointIndex(int offset) const {
            const auto it = std::ranges::upper_bound(m_codepointOffsets, offset);
            return ImMax(static_cast<int>(it - m_codepointOffsets.begin()) - 1, 0);
        }

        [[nodiscard]] int ByteOffsetStepByCodepoint(int offset, int dir) const {
            int index = ByteOffsetToCodepointIndex(offset);
            if (dir > 0)
                index = ImMin(index + 1, static_cast<int>(m_codepoints.size()));
            else if (m_codepointOffsets[index] == offset)
                index = ImMax(index - 1, 0);
            return m_codepointOffsets[index];
        }

        [[nodiscard]] uint32_t CodepointAt(int offset) const {
            const auto index = ByteOffsetToCodepointIndex(offset);
            return std::cmp_less(index, m_codepoints.size()) ? m_codepoints[index] : 0;
        }

        [[nodiscard]] int ByteOffsetStepBy(int offset, ImTextMoveFlags unit, int count) const {
            IM_ASSERT(!(unit & ~ImTextMoveFlags_UnitMask_));
            const auto limit = ImMin(m_textBytes, m_textLength);
            const auto dir = count < 0 ? -1 : 1;
            for (auto n = count < 0 ? -count : count; n > 0; n--) {
                // For this example implementation, 1 grapheme = 1 codepoint.
                if (unit == ImTextMoveFlags_Codepoint || unit == ImTextMoveFlags_Grapheme) {
                    offset = ByteOffsetStepByCodepoint(offset, dir);
                    continue;
                }

                if (dir > 0) {
                    const auto first = unit != ImTextMoveFlags_WordStart;
                    while (offset < limit && IsWordChar(CodepointAt(offset)) != first)
                        offset = ByteOffsetStepByCodepoint(offset, 1);
                    while (offset < limit && IsWordChar(CodepointAt(offset)) == first)
                        offset = ByteOffsetStepByCodepoint(offset, 1);
                } else {
                    const auto first = unit == ImTextMoveFlags_WordStart;
                    while (offset > 0 && IsWordChar(CodepointAt(ByteOffsetStepByCodepoint(offset, -1))) != first)
                        offset = ByteOffsetStepByCodepoint(offset, -1);
                    while (offset > 0 && IsWordChar(CodepointAt(ByteOffsetStepByCodepoint(offset, -1))) == first)
                        offset = ByteOffsetStepByCodepoint(offset, -1);
                }
            }
            return ImClamp(offset, 0, limit);
        }

        static ImVec2 ApplyTransform(const auto& t, const ImVec2& pos, const ImVec2& p) {
            const auto d = p - pos;
            return pos + t.AxisX * d.x + t.AxisY * d.y;
        }

        static void EmitQuad(ImDrawList* draw_list, ImVec2 p0, ImVec2 p1, ImVec2 uv0, ImVec2 uv1, ImU32 col, const ImVec4* fine_clip, const ImVec2& pos, const ImTextTransform* transform) {
            if (fine_clip != nullptr) {
                const auto& c = *fine_clip;
                if (p0.x >= c.z || p1.x <= c.x || p0.y >= c.w || p1.y <= c.y)
                    return;
                const auto size = p1 - p0, uv_size = uv1 - uv0;
                if (p0.x < c.x) {
                    uv0.x += uv_size.x * (c.x - p0.x) / size.x;
                    p0.x = c.x;
                }
                if (p1.x > c.z) {
                    uv1.x -= uv_size.x * (p1.x - c.z) / size.x;
                    p1.x = c.z;
                }
                if (p0.y < c.y) {
                    uv0.y += uv_size.y * (c.y - p0.y) / size.y;
                    p0.y = c.y;
                }
                if (p1.y > c.w) {
                    uv1.y -= uv_size.y * (p1.y - c.w) / size.y;
                    p1.y = c.w;
                }
            }
            ImVec2 q[]{p0, ImVec2(p1.x, p0.y), p1, ImVec2(p0.x, p1.y)};
            if (transform != nullptr)
                for (ImVec2& p : q)
                    p = ApplyTransform(*transform, pos, p);
            draw_list->PrimReserve(6, 4);
            draw_list->PrimQuadUV(q[0], q[1], q[2], q[3], uv0, ImVec2(uv1.x, uv0.y), uv1, ImVec2(uv0.x, uv1.y), col);
        }
    };

    ImHRESULT LayoutCallback(ImFont* font, const ImTextDesc* desc, const char* text, const char* text_end, ImTextLayout** out_layout) {
        if (!out_layout) return E_POINTER;
        *out_layout = nullptr;

        if (!font || !desc || ((!text || !text_end) && text != text_end)) return E_POINTER;
        if (desc->Size <= 0) return E_INVALIDARG;

        const auto fontImpl = ExampleFont::From(font);
        if (!fontImpl) return E_INVALIDARG;

        const auto textView = std::string_view(text, static_cast<size_t>(text_end - text));
        *out_layout = ExampleTextLayout::New(*fontImpl, *desc, textView)->AsNative();
        return S_OK;
    }

    void MetricsCallback(ImFont* font, float size, float, ImTextMetrics* out_metrics) {
        const auto f = ExampleFont::From(font);
        *out_metrics = f != nullptr ? f->Metrics(size) : ImTextMetrics();
        const auto gap = out_metrics->LineHeight - out_metrics->Ascent - out_metrics->Descent;
        out_metrics->Ascent += gap * 0.5f;
        out_metrics->Descent = out_metrics->LineHeight - out_metrics->Ascent;
    }
}

void ExampleText::Install() {
    ImGui::SetTextCallbacks(LayoutCallback, MetricsCallback);
}

ExampleFont* ExampleText::LoadFont(const char* path) {
    try {
        std::ifstream fp(path, std::ios::binary);
        fp.exceptions(std::ios::failbit | std::ios::badbit | std::ios::eofbit);

        std::vector<uint8_t> data((fp.seekg(0, std::ios::end).tellg()), 0);
        fp.seekg(0, std::ios::beg).read(reinterpret_cast<char*>(data.data()), data.size());

        return ExampleFont::New(std::move(data));
    } catch (...) {
        return nullptr;
    }
}

void ExampleText::DestroyFont(ExampleFont* font) {
    ExampleFont::Destroy(font);
}

ImFont* ExampleText::ToImFont(ExampleFont* font) {
    return reinterpret_cast<ImFont*>(font);
}
