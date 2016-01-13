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

#include "config.h"
#include "modules/webmidi/MIDIAccess.h"

#include "core/dom/Document.h"
#include "core/loader/DocumentLoadTiming.h"
#include "core/loader/DocumentLoader.h"
#include "modules/webmidi/MIDIAccessInitializer.h"
#include "modules/webmidi/MIDIConnectionEvent.h"
#include "modules/webmidi/MIDIController.h"
#include "modules/webmidi/MIDIOptions.h"
#include "modules/webmidi/MIDIPort.h"
#include "platform/AsyncMethodRunner.h"
#include <v8.h>

namespace WebCore {

MIDIAccess::MIDIAccess(PassOwnPtr<MIDIAccessor> accessor, bool sysexEnabled, const Vector<MIDIAccessInitializer::PortDescriptor>& ports, ExecutionContext* executionContext)
    : ActiveDOMObject(executionContext)
    , m_accessor(accessor)
    , m_sysexEnabled(sysexEnabled)
{
    ScriptWrappable::init(this);
    m_accessor->setClient(this);
    for (size_t i = 0; i < ports.size(); ++i) {
        const MIDIAccessInitializer::PortDescriptor& port = ports[i];
        if (port.type == MIDIPort::MIDIPortTypeInput) {
            m_inputs.append(MIDIInput::create(this, port.id, port.manufacturer, port.name, port.version));
        } else {
            m_outputs.append(MIDIOutput::create(this, m_outputs.size(), port.id, port.manufacturer, port.name, port.version));
        }
    }
}

MIDIAccess::~MIDIAccess()
{
}

void MIDIAccess::didAddInputPort(const String& id, const String& manufacturer, const String& name, const String& version)
{
    ASSERT(isMainThread());
    m_inputs.append(MIDIInput::create(this, id, manufacturer, name, version));
}

void MIDIAccess::didAddOutputPort(const String& id, const String& manufacturer, const String& name, const String& version)
{
    ASSERT(isMainThread());
    unsigned portIndex = m_outputs.size();
    m_outputs.append(MIDIOutput::create(this, portIndex, id, manufacturer, name, version));
}

void MIDIAccess::didReceiveMIDIData(unsigned portIndex, const unsigned char* data, size_t length, double timeStamp)
{
    ASSERT(isMainThread());
    if (portIndex >= m_inputs.size())
        return;

    // Convert from time in seconds which is based on the time coordinate system of monotonicallyIncreasingTime()
    // into time in milliseconds (a DOMHighResTimeStamp) according to the same time coordinate system as performance.now().
    // This is how timestamps are defined in the Web MIDI spec.
    Document* document = toDocument(executionContext());
    ASSERT(document);

    double timeStampInMilliseconds = 1000 * document->loader()->timing()->monotonicTimeToZeroBasedDocumentTime(timeStamp);

    m_inputs[portIndex]->didReceiveMIDIData(portIndex, data, length, timeStampInMilliseconds);
}

void MIDIAccess::sendMIDIData(unsigned portIndex, const unsigned char* data, size_t length, double timeStampInMilliseconds)
{
    if (!data || !length || portIndex >= m_outputs.size())
        return;
    // Convert from a time in milliseconds (a DOMHighResTimeStamp) according to the same time coordinate system as performance.now()
    // into a time in seconds which is based on the time coordinate system of monotonicallyIncreasingTime().
    double timeStamp;

    if (!timeStampInMilliseconds) {
        // We treat a value of 0 (which is the default value) as special, meaning "now".
        // We need to translate it exactly to 0 seconds.
        timeStamp = 0;
    } else {
        Document* document = toDocument(executionContext());
        ASSERT(document);
        double documentStartTime = document->loader()->timing()->referenceMonotonicTime();
        timeStamp = documentStartTime + 0.001 * timeStampInMilliseconds;
    }

    m_accessor->sendMIDIData(portIndex, data, length, timeStamp);
}

void MIDIAccess::stop()
{
    m_accessor.clear();
}

void MIDIAccess::trace(Visitor* visitor)
{
    visitor->trace(m_inputs);
    visitor->trace(m_outputs);
    EventTargetWithInlineData::trace(visitor);
}

} // namespace WebCore
