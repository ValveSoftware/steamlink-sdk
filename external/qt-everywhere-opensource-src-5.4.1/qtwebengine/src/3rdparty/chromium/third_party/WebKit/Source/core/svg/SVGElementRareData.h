/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#ifndef SVGElementRareData_h
#define SVGElementRareData_h

#include "core/css/StylePropertySet.h"
#include "core/css/resolver/StyleResolver.h"
#include "wtf/HashSet.h"
#include "wtf/Noncopyable.h"
#include "wtf/StdLibExtras.h"

namespace WebCore {

class CSSCursorImageValue;
class SVGCursorElement;
class SVGElement;

class SVGElementRareData : public NoBaseWillBeGarbageCollectedFinalized<SVGElementRareData> {
    WTF_MAKE_NONCOPYABLE(SVGElementRareData); WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED;
public:
    SVGElementRareData(SVGElement* owner)
#if ENABLE(OILPAN)
        : m_owner(owner)
        , m_cursorElement(nullptr)
#else
        : m_cursorElement(nullptr)
#endif
        , m_cursorImageValue(nullptr)
        , m_correspondingElement(nullptr)
        , m_instancesUpdatesBlocked(false)
        , m_useOverrideComputedStyle(false)
        , m_needsOverrideComputedStyleUpdate(false)
    {
    }

    WillBeHeapHashSet<RawPtrWillBeWeakMember<SVGElement> >& elementInstances() { return m_elementInstances; }
    const WillBeHeapHashSet<RawPtrWillBeWeakMember<SVGElement> >& elementInstances() const { return m_elementInstances; }

    bool instanceUpdatesBlocked() const { return m_instancesUpdatesBlocked; }
    void setInstanceUpdatesBlocked(bool value) { m_instancesUpdatesBlocked = value; }

    SVGCursorElement* cursorElement() const { return m_cursorElement; }
    void setCursorElement(SVGCursorElement* cursorElement) { m_cursorElement = cursorElement; }

    SVGElement* correspondingElement() { return m_correspondingElement.get(); }
    void setCorrespondingElement(SVGElement* correspondingElement) { m_correspondingElement = correspondingElement; }

    CSSCursorImageValue* cursorImageValue() const { return m_cursorImageValue; }
    void setCursorImageValue(CSSCursorImageValue* cursorImageValue) { m_cursorImageValue = cursorImageValue; }

    MutableStylePropertySet* animatedSMILStyleProperties() const { return m_animatedSMILStyleProperties.get(); }
    MutableStylePropertySet* ensureAnimatedSMILStyleProperties()
    {
        if (!m_animatedSMILStyleProperties)
            m_animatedSMILStyleProperties = MutableStylePropertySet::create(SVGAttributeMode);
        return m_animatedSMILStyleProperties.get();
    }

    RenderStyle* overrideComputedStyle(Element* element, RenderStyle* parentStyle)
    {
        ASSERT(element);
        if (!m_useOverrideComputedStyle)
            return 0;
        if (!m_overrideComputedStyle || m_needsOverrideComputedStyleUpdate) {
            // The style computed here contains no CSS Animations/Transitions or SMIL induced rules - this is needed to compute the "base value" for the SMIL animation sandwhich model.
            m_overrideComputedStyle = element->document().ensureStyleResolver().styleForElement(element, parentStyle, DisallowStyleSharing, MatchAllRulesExcludingSMIL);
            m_needsOverrideComputedStyleUpdate = false;
        }
        ASSERT(m_overrideComputedStyle);
        return m_overrideComputedStyle.get();
    }

    bool useOverrideComputedStyle() const { return m_useOverrideComputedStyle; }
    void setUseOverrideComputedStyle(bool value) { m_useOverrideComputedStyle = value; }
    void setNeedsOverrideComputedStyleUpdate() { m_needsOverrideComputedStyleUpdate = true; }

    void trace(Visitor* visitor)
    {
#if ENABLE(OILPAN)
        visitor->trace(m_animatedSMILStyleProperties);
        visitor->trace(m_elementInstances);
        visitor->trace(m_correspondingElement);
        visitor->trace(m_owner);
        visitor->registerWeakMembers<SVGElementRareData, &SVGElementRareData::processWeakMembers>(this);
#endif
    }

    void processWeakMembers(Visitor* visitor)
    {
#if ENABLE(OILPAN)
        ASSERT(m_owner);
        if (!visitor->isAlive(m_cursorElement))
            m_cursorElement = nullptr;

        if (!visitor->isAlive(m_cursorImageValue)) {
            // The owning SVGElement is still alive and if it is pointing to an SVGCursorElement
            // we unregister it when the CSSCursorImageValue dies.
            if (m_cursorElement) {
                m_cursorElement->removeReferencedElement(m_owner);
                m_cursorElement = nullptr;
            }
            m_cursorImageValue = nullptr;
        }
        ASSERT(!m_cursorElement || visitor->isAlive(m_cursorElement));
        ASSERT(!m_cursorImageValue || visitor->isAlive(m_cursorImageValue));
#endif
    }

private:
#if ENABLE(OILPAN)
    Member<SVGElement> m_owner;
#endif
    WillBeHeapHashSet<RawPtrWillBeWeakMember<SVGElement> > m_elementInstances;
    RawPtrWillBeWeakMember<SVGCursorElement> m_cursorElement;
    RawPtrWillBeWeakMember<CSSCursorImageValue> m_cursorImageValue;
    RefPtrWillBeMember<SVGElement> m_correspondingElement;
    bool m_instancesUpdatesBlocked : 1;
    bool m_useOverrideComputedStyle : 1;
    bool m_needsOverrideComputedStyleUpdate : 1;
    RefPtrWillBeMember<MutableStylePropertySet> m_animatedSMILStyleProperties;
    RefPtr<RenderStyle> m_overrideComputedStyle;
};

}

#endif
