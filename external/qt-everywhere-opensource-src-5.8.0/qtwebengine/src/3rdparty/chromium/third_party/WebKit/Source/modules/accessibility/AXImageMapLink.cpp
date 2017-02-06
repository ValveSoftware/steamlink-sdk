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

#include "modules/accessibility/AXImageMapLink.h"

#include "core/dom/ElementTraversal.h"
#include "modules/accessibility/AXLayoutObject.h"
#include "modules/accessibility/AXObjectCacheImpl.h"

namespace blink {

using namespace HTMLNames;

AXImageMapLink::AXImageMapLink(HTMLAreaElement* area, AXObjectCacheImpl& axObjectCache)
    : AXNodeObject(area, axObjectCache)
{
}

AXImageMapLink::~AXImageMapLink()
{
}

AXImageMapLink* AXImageMapLink::create(HTMLAreaElement* area, AXObjectCacheImpl& axObjectCache)
{
    return new AXImageMapLink(area, axObjectCache);
}

HTMLMapElement* AXImageMapLink::mapElement() const
{
    HTMLAreaElement* area = areaElement();
    if (!area)
        return nullptr;
    return Traversal<HTMLMapElement>::firstAncestor(*area);
}

AXObject* AXImageMapLink::computeParent() const
{
    ASSERT(!isDetached());
    if (m_parent)
        return m_parent;

    if (!mapElement())
        return nullptr;

    return axObjectCache().getOrCreate(mapElement()->layoutObject());
}

AccessibilityRole AXImageMapLink::roleValue() const
{
    const AtomicString& ariaRole = getAttribute(roleAttr);
    if (!ariaRole.isEmpty())
        return AXObject::ariaRoleToWebCoreRole(ariaRole);

    return LinkRole;
}

bool AXImageMapLink::computeAccessibilityIsIgnored(IgnoredReasons* ignoredReasons) const
{
    return accessibilityIsIgnoredByDefault(ignoredReasons);
}

Element* AXImageMapLink::actionElement() const
{
    return anchorElement();
}

Element* AXImageMapLink::anchorElement() const
{
    return getNode() ? toElement(getNode()) : nullptr;
}

KURL AXImageMapLink::url() const
{
    if (!areaElement())
        return KURL();

    return areaElement()->href();
}

LayoutRect AXImageMapLink::elementRect() const
{
    HTMLAreaElement* area = areaElement();
    HTMLMapElement* map = mapElement();
    if (!area || !map)
        return LayoutRect();

    LayoutObject* layoutObject;
    if (m_parent && m_parent->isAXLayoutObject())
        layoutObject = toAXLayoutObject(m_parent)->getLayoutObject();
    else
        layoutObject = map->layoutObject();

    if (!layoutObject)
        return LayoutRect();

    return area->computeAbsoluteRect(layoutObject);
}

DEFINE_TRACE(AXImageMapLink)
{
    AXNodeObject::trace(visitor);
}

} // namespace blink
