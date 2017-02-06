/*
 * Copyright (C) 2006, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef EventListener_h
#define EventListener_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "wtf/text/WTFString.h"

namespace blink {

class Event;
class ExecutionContext;

class CORE_EXPORT EventListener : public GarbageCollectedFinalized<EventListener> {
public:
    enum ListenerType {
        JSEventListenerType,
        ImageEventListenerType,
        CPPEventListenerType,
        ConditionEventListenerType,
        NativeEventListenerType,
    };

    virtual ~EventListener() { }
    virtual bool operator==(const EventListener&) const = 0;
    virtual void handleEvent(ExecutionContext*, Event*) = 0;
    virtual const String& code() const { return emptyString(); }
    virtual bool belongsToTheCurrentWorld() const { return false; }

    bool isAttribute() const { return virtualisAttribute(); }
    ListenerType type() const { return m_type; }

    DEFINE_INLINE_VIRTUAL_TRACE() { }

protected:
    explicit EventListener(ListenerType type)
            : m_type(type)
    {
    }

private:
    virtual bool virtualisAttribute() const { return false; }

    ListenerType m_type;
};

} // namespace blink

#endif
