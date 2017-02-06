// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/svg/SVGElementRareData.h"

#include "core/css/CSSCursorImageValue.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Document.h"
#include "core/svg/SVGCursorElement.h"

namespace blink {

MutableStylePropertySet* SVGElementRareData::ensureAnimatedSMILStyleProperties()
{
    if (!m_animatedSMILStyleProperties)
        m_animatedSMILStyleProperties = MutableStylePropertySet::create(SVGAttributeMode);
    return m_animatedSMILStyleProperties.get();
}

ComputedStyle* SVGElementRareData::overrideComputedStyle(Element* element, const ComputedStyle* parentStyle)
{
    ASSERT(element);
    if (!m_useOverrideComputedStyle)
        return nullptr;
    if (!m_overrideComputedStyle || m_needsOverrideComputedStyleUpdate) {
        // The style computed here contains no CSS Animations/Transitions or SMIL induced rules - this is needed to compute the "base value" for the SMIL animation sandwhich model.
        m_overrideComputedStyle = element->document().ensureStyleResolver().styleForElement(element, parentStyle, DisallowStyleSharing, MatchAllRulesExcludingSMIL);
        m_needsOverrideComputedStyleUpdate = false;
    }
    ASSERT(m_overrideComputedStyle);
    return m_overrideComputedStyle.get();
}

DEFINE_TRACE(SVGElementRareData)
{
    visitor->trace(m_outgoingReferences);
    visitor->trace(m_incomingReferences);
    visitor->trace(m_animatedSMILStyleProperties);
    visitor->trace(m_elementInstances);
    visitor->trace(m_correspondingElement);
    visitor->trace(m_owner);
    visitor->template registerWeakMembers<SVGElementRareData, &SVGElementRareData::processWeakMembers>(this);
}

void SVGElementRareData::processWeakMembers(Visitor* visitor)
{
    ASSERT(m_owner);
    if (!ThreadHeap::isHeapObjectAlive(m_cursorElement))
        m_cursorElement = nullptr;

    if (!ThreadHeap::isHeapObjectAlive(m_cursorImageValue)) {
        // The owning SVGElement is still alive and if it is pointing to an SVGCursorElement
        // we unregister it when the CSSCursorImageValue dies.
        if (m_cursorElement) {
            m_cursorElement->removeReferencedElement(m_owner);
            m_cursorElement = nullptr;
        }
        m_cursorImageValue = nullptr;
    }
    ASSERT(!m_cursorElement || ThreadHeap::isHeapObjectAlive(m_cursorElement));
    ASSERT(!m_cursorImageValue || ThreadHeap::isHeapObjectAlive(m_cursorImageValue));
}

AffineTransform* SVGElementRareData::animateMotionTransform()
{
    return &m_animateMotionTransform;
}

} // namespace blink
