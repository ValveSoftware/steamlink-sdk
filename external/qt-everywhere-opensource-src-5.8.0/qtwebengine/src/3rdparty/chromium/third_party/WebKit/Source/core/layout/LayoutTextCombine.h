/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef LayoutTextCombine_h
#define LayoutTextCombine_h

#include "core/layout/LayoutText.h"
#include "platform/fonts/Font.h"

namespace blink {

// LayoutTextCombine uses different coordinate systems for layout and inlineTextBox,
// because it is treated as 1em-box character in vertical flow for the layout,
// while its inline box is in horizontal flow.
class LayoutTextCombine final : public LayoutText {
public:
    LayoutTextCombine(Node*, PassRefPtr<StringImpl>);

    void updateFont();
    bool isCombined() const { return m_isCombined; }
    float combinedTextWidth(const Font& font) const { return font.getFontDescription().computedSize(); }
    const Font& originalFont() const { return parent()->style()->font(); }
    void transformToInlineCoordinates(GraphicsContext&, const LayoutRect& boxRect, bool clip = false) const;
    LayoutUnit inlineWidthForLayout() const;

    const char* name() const override { return "LayoutTextCombine"; }

private:
    bool isCombineText() const override { return true; }
    float width(unsigned from, unsigned length, const Font&, LayoutUnit xPosition, TextDirection, HashSet<const SimpleFontData*>* fallbackFonts = nullptr, FloatRect* glyphBounds = nullptr) const override;
    void styleDidChange(StyleDifference, const ComputedStyle* oldStyle) override;
    void setTextInternal(PassRefPtr<StringImpl>) override;
    void updateIsCombined();

    float m_combinedTextWidth;
    float m_scaleX;
    bool m_isCombined : 1;
    bool m_needsFontUpdate : 1;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutTextCombine, isCombineText());

inline LayoutUnit LayoutTextCombine::inlineWidthForLayout() const
{
    ASSERT(!m_needsFontUpdate);
    return LayoutUnit::fromFloatCeil(m_combinedTextWidth);
}

} // namespace blink

#endif // LayoutTextCombine_h
