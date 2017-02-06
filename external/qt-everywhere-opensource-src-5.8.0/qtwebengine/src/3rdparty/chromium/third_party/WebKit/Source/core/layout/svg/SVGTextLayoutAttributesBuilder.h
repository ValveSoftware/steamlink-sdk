/*
 * Copyright (C) Research In Motion Limited 2010-2012. All rights reserved.
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
 */

#ifndef SVGTextLayoutAttributesBuilder_h
#define SVGTextLayoutAttributesBuilder_h

#include "core/layout/svg/SVGCharacterData.h"
#include "platform/heap/Handle.h"
#include "wtf/Allocator.h"
#include "wtf/Vector.h"

namespace blink {

class LayoutBoxModelObject;
class LayoutSVGText;
class SVGTextPositioningElement;

// SVGTextLayoutAttributesBuilder performs the first layout phase for SVG text.
//
// It extracts the x/y/dx/dy/rotate values from the SVGTextPositioningElements in the DOM.
// These values are propagated to the corresponding LayoutSVGInlineText layoutObjects.
// The first layout phase only extracts the relevant information needed in LayoutBlockFlowLine
// to create the InlineBox tree based on text chunk boundaries & BiDi information.
// The second layout phase is carried out by SVGTextLayoutEngine.
class SVGTextLayoutAttributesBuilder {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(SVGTextLayoutAttributesBuilder);
public:
    explicit SVGTextLayoutAttributesBuilder(LayoutSVGText&);

    void buildLayoutAttributes();

    struct TextPosition {
        DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    public:
        TextPosition(SVGTextPositioningElement* newElement = nullptr, unsigned newStart = 0, unsigned newLength = 0)
            : element(newElement)
            , start(newStart)
            , length(newLength)
        {
        }

        DECLARE_TRACE();

        Member<SVGTextPositioningElement> element;
        unsigned start;
        unsigned length;
    };

private:
    void buildCharacterDataMap(LayoutSVGText&);
    void buildLayoutAttributes(LayoutSVGText&) const;
    void collectTextPositioningElements(LayoutBoxModelObject&);
    void fillCharacterDataMap(const TextPosition&);

    LayoutSVGText& m_textRoot;
    unsigned m_characterCount;
    HeapVector<TextPosition> m_textPositions;
    SVGCharacterDataMap m_characterDataMap;
};

} // namespace blink

WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::SVGTextLayoutAttributesBuilder::TextPosition);

#endif
