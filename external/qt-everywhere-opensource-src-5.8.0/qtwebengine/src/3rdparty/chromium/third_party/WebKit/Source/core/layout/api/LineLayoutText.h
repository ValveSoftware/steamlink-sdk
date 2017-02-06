// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LineLayoutText_h
#define LineLayoutText_h

#include "core/layout/LayoutText.h"
#include "core/layout/api/LineLayoutItem.h"
#include "platform/LayoutUnit.h"
#include "platform/text/TextPath.h"
#include "wtf/Forward.h"

namespace blink {

class LayoutText;

class LineLayoutText : public LineLayoutItem {
public:
    explicit LineLayoutText(LayoutText* layoutObject) : LineLayoutItem(layoutObject) { }

    explicit LineLayoutText(const LineLayoutItem& item)
        : LineLayoutItem(item)
    {
        ASSERT_WITH_SECURITY_IMPLICATION(!item || item.isText());
    }

    explicit LineLayoutText(std::nullptr_t) : LineLayoutItem(nullptr) { }

    LineLayoutText() { }

    InlineTextBox* firstTextBox() const
    {
        return toText()->firstTextBox();
    }

    InlineTextBox* lastTextBox() const
    {
        return toText()->lastTextBox();
    }

    InlineTextBox* createInlineTextBox(int start, unsigned short length)
    {
        return toText()->createInlineTextBox(start, length);
    }

    void extractTextBox(InlineTextBox* inlineTextBox)
    {
        toText()->extractTextBox(inlineTextBox);
    }

    void attachTextBox(InlineTextBox* inlineTextBox)
    {
        toText()->attachTextBox(inlineTextBox);
    }

    void removeTextBox(InlineTextBox* inlineTextBox)
    {
        toText()->removeTextBox(inlineTextBox);
    }

    bool isWordBreak() const
    {
        return toText()->isWordBreak();
    }

    bool isAllCollapsibleWhitespace() const
    {
        return toText()->isAllCollapsibleWhitespace();
    }

    UChar characterAt(unsigned offset) const
    {
        return toText()->characterAt(offset);
    }

    UChar uncheckedCharacterAt(unsigned offset) const
    {
        return toText()->uncheckedCharacterAt(offset);
    }

    UChar32 codepointAt(unsigned offset) const
    {
        return toText()->codepointAt(offset);
    }

    bool is8Bit() const
    {
        return toText()->is8Bit();
    }

    const LChar* characters8() const
    {
        return toText()->characters8();
    }

    const UChar* characters16() const
    {
        return toText()->characters16();
    }

    bool hasEmptyText() const
    {
        return toText()->hasEmptyText();
    }

    unsigned textLength() const
    {
        return toText()->textLength();
    }

    unsigned resolvedTextLength() const
    {
        return toText()->resolvedTextLength();
    }

    const String& text() const
    {
        return toText()->text();
    }

    bool canUseSimpleFontCodePath() const
    {
        return toText()->canUseSimpleFontCodePath();
    }

    float width(unsigned from, unsigned len, const Font& font, LayoutUnit xPos, TextDirection textDirection, HashSet<const SimpleFontData*>* fallbackFonts, FloatRect* glyphBounds) const
    {
        return toText()->width(from, len, font, xPos, textDirection, fallbackFonts, glyphBounds);
    }

    float width(unsigned from, unsigned len, LayoutUnit xPos, TextDirection textDirection, bool firstLine, HashSet<const SimpleFontData*>* fallbackFonts = nullptr, FloatRect* glyphBounds = nullptr) const
    {
        return toText()->width(from, len, xPos, textDirection, firstLine, fallbackFonts, glyphBounds);
    }

    float hyphenWidth(const Font& font, TextDirection textDirection)
    {
        return toText()->hyphenWidth(font, textDirection);
    }

    void selectionStartEnd(int& spos, int& epos) const
    {
        return toText()->selectionStartEnd(spos, epos);
    }

    unsigned textStartOffset() const
    {
        return toText()->textStartOffset();
    }

    float minLogicalWidth() const
    {
        return toText()->minLogicalWidth();
    }

private:
    LayoutText* toText() { return toLayoutText(layoutObject()); }
    const LayoutText* toText() const { return toLayoutText(layoutObject()); }
};

} // namespace blink

#endif // LineLayoutText_h
