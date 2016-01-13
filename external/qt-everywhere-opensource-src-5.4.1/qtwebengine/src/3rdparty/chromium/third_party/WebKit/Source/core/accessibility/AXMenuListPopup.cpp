/*
 * Copyright (C) 2010 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/accessibility/AXMenuListPopup.h"

#include "core/accessibility/AXMenuListOption.h"
#include "core/accessibility/AXObjectCache.h"
#include "core/html/HTMLSelectElement.h"

namespace WebCore {

using namespace HTMLNames;

AXMenuListPopup::AXMenuListPopup()
{
}

bool AXMenuListPopup::isVisible() const
{
    return false;
}

bool AXMenuListPopup::isOffScreen() const
{
    if (!m_parent)
        return true;

    return m_parent->isCollapsed();
}

bool AXMenuListPopup::isEnabled() const
{
    if (!m_parent)
        return false;

    return m_parent->isEnabled();
}

bool AXMenuListPopup::computeAccessibilityIsIgnored() const
{
    return accessibilityIsIgnoredByDefault();
}

AXMenuListOption* AXMenuListPopup::menuListOptionAXObject(HTMLElement* element) const
{
    ASSERT(element);
    if (!isHTMLOptionElement(*element))
        return 0;

    AXObject* object = document()->axObjectCache()->getOrCreate(MenuListOptionRole);
    ASSERT_WITH_SECURITY_IMPLICATION(object->isMenuListOption());

    AXMenuListOption* option = toAXMenuListOption(object);
    option->setElement(element);

    return option;
}

bool AXMenuListPopup::press() const
{
    if (!m_parent)
        return false;

    m_parent->press();
    return true;
}

void AXMenuListPopup::addChildren()
{
    if (!m_parent)
        return;

    Node* selectNode = m_parent->node();
    if (!selectNode)
        return;

    m_haveChildren = true;

    const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& listItems = toHTMLSelectElement(selectNode)->listItems();
    unsigned length = listItems.size();
    for (unsigned i = 0; i < length; i++) {
        AXMenuListOption* option = menuListOptionAXObject(listItems[i]);
        if (option) {
            option->setParent(this);
            m_children.append(option);
        }
    }
}

void AXMenuListPopup::childrenChanged()
{
    AXObjectCache* cache = axObjectCache();
    if (!cache)
        return;
    for (size_t i = m_children.size(); i > 0 ; --i) {
        AXObject* child = m_children[i - 1].get();
        // FIXME: How could children end up in here that have no actionElement(), the check
        // in menuListOptionAXObject would seem to prevent that.
        if (child->actionElement()) {
            child->detachFromParent();
            cache->remove(child->axObjectID());
        }
    }

    m_children.clear();
    m_haveChildren = false;
}

void AXMenuListPopup::didUpdateActiveOption(int optionIndex)
{
    // We defer creation of the children until updating the active option so that we don't
    // create AXObjects for <option> elements while they're in the middle of removal.
    if (!m_haveChildren)
        addChildren();

    ASSERT_ARG(optionIndex, optionIndex >= 0);
    ASSERT_ARG(optionIndex, optionIndex < static_cast<int>(m_children.size()));

    AXObjectCache* cache = axObjectCache();
    RefPtr<AXObject> child = m_children[optionIndex].get();

    cache->postNotification(child.get(), document(), AXObjectCache::AXFocusedUIElementChanged, true, PostSynchronously);
    cache->postNotification(child.get(), document(), AXObjectCache::AXMenuListItemSelected, true, PostSynchronously);
}

} // namespace WebCore
