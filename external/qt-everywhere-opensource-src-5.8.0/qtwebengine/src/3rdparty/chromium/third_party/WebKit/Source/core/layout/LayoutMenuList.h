/*
 * This file is part of the select element layoutObject in WebCore.
 *
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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

#ifndef LayoutMenuList_h
#define LayoutMenuList_h

#include "core/CoreExport.h"
#include "core/layout/LayoutFlexibleBox.h"
#include "platform/geometry/LayoutRect.h"

namespace blink {

class HTMLSelectElement;
class LayoutText;

class CORE_EXPORT LayoutMenuList final : public LayoutFlexibleBox {
public:
    explicit LayoutMenuList(Element*);
    ~LayoutMenuList() override;

    HTMLSelectElement* selectElement() const;
    void didSetSelectedIndex(int optionIndex);
    String text() const;

    const char* name() const override { return "LayoutMenuList"; }

    LayoutUnit clientPaddingLeft() const;
    LayoutUnit clientPaddingRight() const;

private:
    bool isOfType(LayoutObjectType type) const override { return type == LayoutObjectMenuList || LayoutFlexibleBox::isOfType(type); }
    bool isChildAllowed(LayoutObject*, const ComputedStyle&) const override;

    void addChild(LayoutObject* newChild, LayoutObject* beforeChild = nullptr) override;
    void removeChild(LayoutObject*) override;
    bool createsAnonymousWrapper() const override { return true; }

    void updateFromElement() override;

    LayoutRect controlClipRect(const LayoutPoint&) const override;
    bool hasControlClip() const override { return true; }

    void computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const override;
    void computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop, LogicalExtentComputedValues&) const override;

    void styleDidChange(StyleDifference, const ComputedStyle* oldStyle) override;

    bool hasLineIfEmpty() const override { return true; }

    // Flexbox defines baselines differently than regular blocks.
    // For backwards compatibility, menulists need to do the regular block behavior.
    int baselinePosition(FontBaseline baseline, bool firstLine, LineDirectionMode direction, LinePositionMode position) const override
    {
        return LayoutBlock::baselinePosition(baseline, firstLine, direction, position);
    }
    int firstLineBoxBaseline() const override { return LayoutBlock::firstLineBoxBaseline(); }
    int inlineBlockBaseline(LineDirectionMode direction) const override { return LayoutBlock::inlineBlockBaseline(direction); }

    void createInnerBlock();
    void adjustInnerStyle();
    void setText(const String&);
    void setTextFromOption(int optionIndex);
    void updateInnerBlockHeight();
    void updateOptionsWidth() const;
    float computeTextWidth(const TextRun&, const ComputedStyle&) const;
    void setIndexToSelectOnCancel(int listIndex);

    void didUpdateActiveOption(int optionIndex);

    LayoutText* m_buttonText;
    LayoutBlock* m_innerBlock;

    bool m_isEmpty : 1;
    bool m_hasUpdatedActiveOption : 1;
    LayoutUnit m_innerBlockHeight;
    // m_optionsWidth is calculated and cached on demand.
    // updateOptionsWidth() should be called before reading them.
    mutable int m_optionsWidth;

    int m_lastActiveIndex;

    RefPtr<ComputedStyle> m_optionStyle;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutMenuList, isMenuList());

} // namespace blink

#endif
