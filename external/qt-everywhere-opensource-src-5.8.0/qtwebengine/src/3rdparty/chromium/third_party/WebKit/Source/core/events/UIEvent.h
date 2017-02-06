/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#ifndef UIEvent_h
#define UIEvent_h

#include "core/CoreExport.h"
#include "core/events/Event.h"
#include "core/events/EventDispatchMediator.h"
#include "core/events/UIEventInit.h"
#include "core/frame/DOMWindow.h"
#include "core/input/InputDeviceCapabilities.h"

namespace blink {

// FIXME: Get rid of this type alias.
using AbstractView = DOMWindow;

class CORE_EXPORT UIEvent : public Event {
    DEFINE_WRAPPERTYPEINFO();
public:
    static UIEvent* create()
    {
        return new UIEvent;
    }
    static UIEvent* create(const AtomicString& type, bool canBubble, bool cancelable, AbstractView* view, int detail)
    {
        return new UIEvent(type, canBubble, cancelable, ComposedMode::Scoped, view, detail);
    }
    static UIEvent* create(const AtomicString& type, const UIEventInit& initializer)
    {
        return new UIEvent(type, initializer);
    }
    ~UIEvent() override;

    void initUIEvent(const AtomicString& type, bool canBubble, bool cancelable, AbstractView*, int detail);
    void initUIEventInternal(const AtomicString& type, bool canBubble, bool cancelable, EventTarget* relatedTarget, AbstractView*, int detail, InputDeviceCapabilities* sourceCapabilities);

    AbstractView* view() const { return m_view.get(); }
    int detail() const { return m_detail; }
    InputDeviceCapabilities* sourceCapabilities() const { return m_sourceCapabilities.get(); }

    const AtomicString& interfaceName() const override;
    bool isUIEvent() const final;

    virtual int which() const;

    DECLARE_VIRTUAL_TRACE();

protected:
    UIEvent();
    // TODO(crbug.com/563542): Remove of this ctor in favor of making platformTimeStamp (and perhaps sourceCapabilities) required in all constructions sites
    UIEvent(const AtomicString& type, bool canBubble, bool cancelable, ComposedMode, AbstractView*, int detail, InputDeviceCapabilities* sourceCapabilities = nullptr);
    UIEvent(const AtomicString& type, bool canBubble, bool cancelable, ComposedMode, double platformTimeStamp, AbstractView*, int detail, InputDeviceCapabilities* sourceCapabilities);
    UIEvent(const AtomicString&, const UIEventInit&);

private:
    Member<AbstractView> m_view;
    int m_detail;
    Member<InputDeviceCapabilities> m_sourceCapabilities;
};

} // namespace blink

#endif // UIEvent_h
