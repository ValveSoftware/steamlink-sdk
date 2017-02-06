/*
 * Copyright (C) Research In Motion Limited 2010-2011. All rights reserved.
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

#include "core/layout/svg/SVGTextLayoutAttributesBuilder.h"

#include "core/layout/api/LineLayoutSVGInlineText.h"
#include "core/layout/svg/LayoutSVGInline.h"
#include "core/layout/svg/LayoutSVGInlineText.h"
#include "core/layout/svg/LayoutSVGText.h"
#include "core/svg/SVGTextPositioningElement.h"

namespace blink {

namespace {

void updateLayoutAttributes(LayoutSVGInlineText& text, unsigned& valueListPosition, const SVGCharacterDataMap& allCharactersMap)
{
    SVGCharacterDataMap& characterDataMap = text.characterDataMap();
    characterDataMap.clear();

    LineLayoutSVGInlineText textLineLayout(&text);
    for (SVGInlineTextMetricsIterator iterator(textLineLayout); !iterator.isAtEnd(); iterator.next()) {
        if (iterator.metrics().isEmpty())
            continue;

        auto it = allCharactersMap.find(valueListPosition + 1);
        if (it != allCharactersMap.end())
            characterDataMap.set(iterator.characterOffset() + 1, it->value);

        // Increase the position in the value/attribute list with one for each
        // "character unit" (that will be displayed.)
        valueListPosition++;
    }
}

} // namespace

SVGTextLayoutAttributesBuilder::SVGTextLayoutAttributesBuilder(LayoutSVGText& textRoot)
    : m_textRoot(textRoot)
    , m_characterCount(0)
{
}

void SVGTextLayoutAttributesBuilder::buildLayoutAttributes()
{
    m_characterDataMap.clear();

    if (m_textPositions.isEmpty()) {
        m_characterCount = 0;
        collectTextPositioningElements(m_textRoot);
    }

    if (!m_characterCount)
        return;

    buildCharacterDataMap(m_textRoot);

    unsigned valueListPosition = 0;
    LayoutObject* child = m_textRoot.firstChild();
    while (child) {
        if (child->isSVGInlineText()) {
            updateLayoutAttributes(toLayoutSVGInlineText(*child), valueListPosition, m_characterDataMap);
        } else if (child->isSVGInline()) {
            // Visit children of text content elements.
            if (LayoutObject* inlineChild = toLayoutSVGInline(child)->firstChild()) {
                child = inlineChild;
                continue;
            }
        }
        child = child->nextInPreOrderAfterChildren(&m_textRoot);
    }
}

static inline unsigned countCharactersInTextNode(const LayoutSVGInlineText& text)
{
    unsigned numCharacters = 0;
    for (const SVGTextMetrics& metrics : text.metricsList()) {
        if (metrics.isEmpty())
            continue;
        numCharacters++;
    }
    return numCharacters;
}

static SVGTextPositioningElement* positioningElementFromLayoutObject(LayoutObject& layoutObject)
{
    ASSERT(layoutObject.isSVGText() || layoutObject.isSVGInline());

    Node* node = layoutObject.node();
    ASSERT(node);
    ASSERT(node->isSVGElement());

    return isSVGTextPositioningElement(*node) ? toSVGTextPositioningElement(node) : nullptr;
}

void SVGTextLayoutAttributesBuilder::collectTextPositioningElements(LayoutBoxModelObject& start)
{
    ASSERT(!start.isSVGText() || m_textPositions.isEmpty());
    SVGTextPositioningElement* element = positioningElementFromLayoutObject(start);
    unsigned atPosition = m_textPositions.size();
    if (element)
        m_textPositions.append(TextPosition(element, m_characterCount));

    for (LayoutObject* child = start.slowFirstChild(); child; child = child->nextSibling()) {
        if (child->isSVGInlineText()) {
            m_characterCount += countCharactersInTextNode(toLayoutSVGInlineText(*child));
            continue;
        }

        if (child->isSVGInline()) {
            collectTextPositioningElements(toLayoutSVGInline(*child));
            continue;
        }
    }

    if (!element)
        return;

    // Compute the length of the subtree after all children have been visited.
    TextPosition& position = m_textPositions[atPosition];
    ASSERT(!position.length);
    position.length = m_characterCount - position.start;
}

void SVGTextLayoutAttributesBuilder::buildCharacterDataMap(LayoutSVGText& textRoot)
{
    // Fill character data map using text positioning elements in top-down order.
    for (const TextPosition& position : m_textPositions)
        fillCharacterDataMap(position);

    // Handle x/y default attributes.
    SVGCharacterData& data = m_characterDataMap.add(1, SVGCharacterData()).storedValue->value;
    if (!data.hasX())
        data.x = 0;
    if (!data.hasY())
        data.y = 0;
}

namespace {

class AttributeListsIterator {
    STACK_ALLOCATED();
public:
    AttributeListsIterator(SVGTextPositioningElement*);

    bool hasAttributes() const
    {
        return m_xListRemaining || m_yListRemaining
            || m_dxListRemaining || m_dyListRemaining
            || m_rotateListRemaining;
    }
    void updateCharacterData(size_t index, SVGCharacterData&);

private:
    SVGLengthContext m_lengthContext;
    Member<SVGLengthList> m_xList;
    unsigned m_xListRemaining;
    Member<SVGLengthList> m_yList;
    unsigned m_yListRemaining;
    Member<SVGLengthList> m_dxList;
    unsigned m_dxListRemaining;
    Member<SVGLengthList> m_dyList;
    unsigned m_dyListRemaining;
    Member<SVGNumberList> m_rotateList;
    unsigned m_rotateListRemaining;
};

AttributeListsIterator::AttributeListsIterator(SVGTextPositioningElement* element)
    : m_lengthContext(element)
    , m_xList(element->x()->currentValue())
    , m_xListRemaining(m_xList->length())
    , m_yList(element->y()->currentValue())
    , m_yListRemaining(m_yList->length())
    , m_dxList(element->dx()->currentValue())
    , m_dxListRemaining(m_dxList->length())
    , m_dyList(element->dy()->currentValue())
    , m_dyListRemaining(m_dyList->length())
    , m_rotateList(element->rotate()->currentValue())
    , m_rotateListRemaining(m_rotateList->length())
{
}

inline void AttributeListsIterator::updateCharacterData(size_t index, SVGCharacterData& data)
{
    if (m_xListRemaining) {
        data.x = m_xList->at(index)->value(m_lengthContext);
        --m_xListRemaining;
    }
    if (m_yListRemaining) {
        data.y = m_yList->at(index)->value(m_lengthContext);
        --m_yListRemaining;
    }
    if (m_dxListRemaining) {
        data.dx = m_dxList->at(index)->value(m_lengthContext);
        --m_dxListRemaining;
    }
    if (m_dyListRemaining) {
        data.dy = m_dyList->at(index)->value(m_lengthContext);
        --m_dyListRemaining;
    }
    if (m_rotateListRemaining) {
        data.rotate = m_rotateList->at(std::min(index, m_rotateList->length() - 1))->value();
        // The last rotation value spans the whole scope.
        if (m_rotateListRemaining > 1)
            --m_rotateListRemaining;
    }
}

} // namespace

void SVGTextLayoutAttributesBuilder::fillCharacterDataMap(const TextPosition& position)
{
    AttributeListsIterator attrLists(position.element);
    for (unsigned i = 0; attrLists.hasAttributes() && i < position.length; ++i) {
        SVGCharacterData& data = m_characterDataMap.add(position.start + i + 1, SVGCharacterData()).storedValue->value;
        attrLists.updateCharacterData(i, data);
    }
}

DEFINE_TRACE(SVGTextLayoutAttributesBuilder::TextPosition)
{
    visitor->trace(element);
}

} // namespace blink
