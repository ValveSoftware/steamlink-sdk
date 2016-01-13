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

#ifndef MIDIAccess_h
#define MIDIAccess_h

#include "bindings/v8/ScriptPromise.h"
#include "bindings/v8/ScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "modules/EventTargetModules.h"
#include "modules/webmidi/MIDIAccessInitializer.h"
#include "modules/webmidi/MIDIAccessor.h"
#include "modules/webmidi/MIDIAccessorClient.h"
#include "modules/webmidi/MIDIInput.h"
#include "modules/webmidi/MIDIOutput.h"
#include "platform/heap/Handle.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

class ExecutionContext;
struct MIDIOptions;

class MIDIAccess FINAL : public RefCountedWillBeRefCountedGarbageCollected<MIDIAccess>, public ScriptWrappable, public ActiveDOMObject, public EventTargetWithInlineData, public MIDIAccessorClient {
    REFCOUNTED_EVENT_TARGET(MIDIAccess);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(MIDIAccess);
public:
    static PassRefPtrWillBeRawPtr<MIDIAccess> create(PassOwnPtr<MIDIAccessor> accessor, bool sysexEnabled, const Vector<MIDIAccessInitializer::PortDescriptor>& ports, ExecutionContext* executionContext)
    {
        RefPtrWillBeRawPtr<MIDIAccess> access = adoptRefWillBeRefCountedGarbageCollected(new MIDIAccess(accessor, sysexEnabled, ports, executionContext));
        access->suspendIfNeeded();
        return access;
    }
    virtual ~MIDIAccess();

    MIDIInputVector inputs() const { return m_inputs; }
    MIDIOutputVector outputs() const { return m_outputs; }

    DEFINE_ATTRIBUTE_EVENT_LISTENER(connect);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(disconnect);

    bool sysexEnabled() const { return m_sysexEnabled; }

    // EventTarget
    virtual const AtomicString& interfaceName() const OVERRIDE { return EventTargetNames::MIDIAccess; }
    virtual ExecutionContext* executionContext() const OVERRIDE { return ActiveDOMObject::executionContext(); }

    // ActiveDOMObject
    virtual void stop() OVERRIDE;

    // MIDIAccessorClient
    virtual void didAddInputPort(const String& id, const String& manufacturer, const String& name, const String& version) OVERRIDE;
    virtual void didAddOutputPort(const String& id, const String& manufacturer, const String& name, const String& version) OVERRIDE;
    virtual void didStartSession(bool success, const String& error, const String& message) OVERRIDE
    {
        // This method is for MIDIAccess initialization: MIDIAccessInitializer
        // has the implementation.
        ASSERT_NOT_REACHED();
    }
    virtual void didReceiveMIDIData(unsigned portIndex, const unsigned char* data, size_t length, double timeStamp) OVERRIDE;

    // |timeStampInMilliseconds| is in the same time coordinate system as performance.now().
    void sendMIDIData(unsigned portIndex, const unsigned char* data, size_t length, double timeStampInMilliseconds);

    virtual void trace(Visitor*) OVERRIDE;

private:
    MIDIAccess(PassOwnPtr<MIDIAccessor>, bool sysexEnabled, const Vector<MIDIAccessInitializer::PortDescriptor>&, ExecutionContext*);

    OwnPtr<MIDIAccessor> m_accessor;
    bool m_sysexEnabled;
    MIDIInputVector m_inputs;
    MIDIOutputVector m_outputs;
};

} // namespace WebCore

#endif // MIDIAccess_h
