/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
#include "core/accessibility/AXImageMapLink.h"

#include "core/accessibility/AXObjectCache.h"
#include "core/accessibility/AXRenderObject.h"

namespace WebCore {

using namespace HTMLNames;

AXImageMapLink::AXImageMapLink()
    : m_areaElement(nullptr)
    , m_mapElement(nullptr)
{
}

AXImageMapLink::~AXImageMapLink()
{
}

void AXImageMapLink::detachFromParent()
{
    AXMockObject::detachFromParent();
    m_areaElement = nullptr;
    m_mapElement = nullptr;
}

PassRefPtr<AXImageMapLink> AXImageMapLink::create()
{
    return adoptRef(new AXImageMapLink());
}

AXObject* AXImageMapLink::parentObject() const
{
    if (m_parent)
        return m_parent;

    if (!m_mapElement.get() || !m_mapElement->renderer())
        return 0;

    return m_mapElement->document().axObjectCache()->getOrCreate(m_mapElement->renderer());
}

AccessibilityRole AXImageMapLink::roleValue() const
{
    if (!m_areaElement)
        return LinkRole;

    const AtomicString& ariaRole = getAttribute(roleAttr);
    if (!ariaRole.isEmpty())
        return AXObject::ariaRoleToWebCoreRole(ariaRole);

    return LinkRole;
}

Element* AXImageMapLink::actionElement() const
{
    return anchorElement();
}

Element* AXImageMapLink::anchorElement() const
{
    return m_areaElement.get();
}

KURL AXImageMapLink::url() const
{
    if (!m_areaElement.get())
        return KURL();

    return m_areaElement->href();
}

String AXImageMapLink::accessibilityDescription() const
{
    const AtomicString& ariaLabel = getAttribute(aria_labelAttr);
    if (!ariaLabel.isEmpty())
        return ariaLabel;
    const AtomicString& alt = getAttribute(altAttr);
    if (!alt.isEmpty())
        return alt;

    return String();
}

String AXImageMapLink::title() const
{
    const AtomicString& title = getAttribute(titleAttr);
    if (!title.isEmpty())
        return title;
    const AtomicString& summary = getAttribute(summaryAttr);
    if (!summary.isEmpty())
        return summary;

    return String();
}

LayoutRect AXImageMapLink::elementRect() const
{
    if (!m_mapElement.get() || !m_areaElement.get())
        return LayoutRect();

    RenderObject* renderer;
    if (m_parent && m_parent->isAXRenderObject())
        renderer = toAXRenderObject(m_parent)->renderer();
    else
        renderer = m_mapElement->renderer();

    if (!renderer)
        return LayoutRect();

    return m_areaElement->computeRect(renderer);
}

} // namespace WebCore
