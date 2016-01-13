/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MIDIMessageEvent_h
#define MIDIMessageEvent_h

#include "modules/EventModules.h"
#include "wtf/Uint8Array.h"

namespace WebCore {

struct MIDIMessageEventInit : public EventInit {
    MIDIMessageEventInit()
        : receivedTime(0.0)
    {
    };

    double receivedTime;
    RefPtr<Uint8Array> data;
};

class MIDIMessageEvent FINAL : public Event {
public:
    static PassRefPtrWillBeRawPtr<MIDIMessageEvent> create()
    {
        return adoptRefWillBeNoop(new MIDIMessageEvent());
    }

    static PassRefPtrWillBeRawPtr<MIDIMessageEvent> create(double receivedTime, PassRefPtr<Uint8Array> data)
    {
        return adoptRefWillBeNoop(new MIDIMessageEvent(receivedTime, data));
    }

    static PassRefPtrWillBeRawPtr<MIDIMessageEvent> create(const AtomicString& type, const MIDIMessageEventInit& initializer)
    {
        return adoptRefWillBeNoop(new MIDIMessageEvent(type, initializer));
    }

    double receivedTime() { return m_receivedTime; }
    PassRefPtr<Uint8Array> data() { return m_data; }

    virtual const AtomicString& interfaceName() const OVERRIDE { return EventNames::MIDIMessageEvent; }

    virtual void trace(Visitor* visitor) OVERRIDE { Event::trace(visitor); }

private:
    MIDIMessageEvent()
        : m_receivedTime(0)
    {
        ScriptWrappable::init(this);
    }

    MIDIMessageEvent(double receivedTime, PassRefPtr<Uint8Array> data)
        : Event(EventTypeNames::midimessage, true, false)
        , m_receivedTime(receivedTime)
        , m_data(data)
    {
        ScriptWrappable::init(this);
    }

    MIDIMessageEvent(const AtomicString& type, const MIDIMessageEventInit& initializer)
        : Event(type, initializer)
        , m_receivedTime(initializer.receivedTime)
        , m_data(initializer.data)
    {
        ScriptWrappable::init(this);
    }

    double m_receivedTime;
    RefPtr<Uint8Array> m_data;
};

} // namespace WebCore

#endif // MIDIMessageEvent_h
