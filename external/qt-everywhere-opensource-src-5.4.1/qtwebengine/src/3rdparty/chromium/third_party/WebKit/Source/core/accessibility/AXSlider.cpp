/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/accessibility/AXSlider.h"

#include "core/accessibility/AXObjectCache.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/shadow/ShadowElementNames.h"
#include "core/rendering/RenderObject.h"

namespace WebCore {

using namespace HTMLNames;

AXSlider::AXSlider(RenderObject* renderer)
    : AXRenderObject(renderer)
{
}

PassRefPtr<AXSlider> AXSlider::create(RenderObject* renderer)
{
    return adoptRef(new AXSlider(renderer));
}

AccessibilityOrientation AXSlider::orientation() const
{
    // Default to horizontal in the unknown case.
    if (!m_renderer)
        return AccessibilityOrientationHorizontal;

    RenderStyle* style = m_renderer->style();
    if (!style)
        return AccessibilityOrientationHorizontal;

    ControlPart styleAppearance = style->appearance();
    switch (styleAppearance) {
    case SliderThumbHorizontalPart:
    case SliderHorizontalPart:
    case MediaSliderPart:
    case MediaFullScreenVolumeSliderPart:
        return AccessibilityOrientationHorizontal;

    case SliderThumbVerticalPart:
    case SliderVerticalPart:
    case MediaVolumeSliderPart:
        return AccessibilityOrientationVertical;

    default:
        return AccessibilityOrientationHorizontal;
    }
}

void AXSlider::addChildren()
{
    ASSERT(!m_haveChildren);

    m_haveChildren = true;

    AXObjectCache* cache = m_renderer->document().axObjectCache();

    AXSliderThumb* thumb = static_cast<AXSliderThumb*>(cache->getOrCreate(SliderThumbRole));
    thumb->setParent(this);

    // Before actually adding the value indicator to the hierarchy,
    // allow the platform to make a final decision about it.
    if (thumb->accessibilityIsIgnored())
        cache->remove(thumb->axObjectID());
    else
        m_children.append(thumb);
}

const AtomicString& AXSlider::getAttribute(const QualifiedName& attribute) const
{
    return element()->getAttribute(attribute);
}

AXObject* AXSlider::elementAccessibilityHitTest(const IntPoint& point) const
{
    if (m_children.size()) {
        ASSERT(m_children.size() == 1);
        if (m_children[0]->elementRect().contains(point))
            return m_children[0].get();
    }

    return axObjectCache()->getOrCreate(m_renderer);
}

void AXSlider::setValue(const String& value)
{
    HTMLInputElement* input = element();

    if (input->value() == value)
        return;

    input->setValue(value);

    // Fire change event manually, as RenderSlider::setValueForPosition does.
    input->dispatchFormControlChangeEvent();
}

HTMLInputElement* AXSlider::element() const
{
    return toHTMLInputElement(m_renderer->node());
}


AXSliderThumb::AXSliderThumb()
{
}

PassRefPtr<AXSliderThumb> AXSliderThumb::create()
{
    return adoptRef(new AXSliderThumb());
}

LayoutRect AXSliderThumb::elementRect() const
{
    if (!m_parent)
        return LayoutRect();

    RenderObject* sliderRenderer = m_parent->renderer();
    if (!sliderRenderer || !sliderRenderer->isSlider())
        return LayoutRect();
    return toElement(sliderRenderer->node())->userAgentShadowRoot()->getElementById(ShadowElementNames::sliderThumb())->boundingBox();
}

bool AXSliderThumb::computeAccessibilityIsIgnored() const
{
    return accessibilityIsIgnoredByDefault();
}

} // namespace WebCore
