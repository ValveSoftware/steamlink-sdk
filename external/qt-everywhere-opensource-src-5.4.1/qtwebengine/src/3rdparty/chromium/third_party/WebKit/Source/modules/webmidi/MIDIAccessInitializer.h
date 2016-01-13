// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIDIAccessInitializer_h
#define MIDIAccessInitializer_h

#include "bindings/v8/ScriptPromise.h"
#include "bindings/v8/ScriptPromiseResolverWithContext.h"
#include "modules/webmidi/MIDIAccessor.h"
#include "modules/webmidi/MIDIAccessorClient.h"
#include "modules/webmidi/MIDIOptions.h"
#include "modules/webmidi/MIDIPort.h"
#include "wtf/OwnPtr.h"
#include "wtf/RawPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

class MIDIAccess;
class Navigator;
class ScriptState;

class MIDIAccessInitializer : public ScriptPromiseResolverWithContext, public MIDIAccessorClient {
public:
    struct PortDescriptor {
        String id;
        String manufacturer;
        String name;
        MIDIPort::MIDIPortTypeCode type;
        String version;

        PortDescriptor(const String& id, const String& manufacturer, const String& name, MIDIPort::MIDIPortTypeCode type, const String& version)
            : id(id)
            , manufacturer(manufacturer)
            , name(name)
            , type(type)
            , version(version) { }
    };

    static ScriptPromise start(ScriptState* scriptState, const MIDIOptions& options)
    {
        RefPtr<MIDIAccessInitializer> p = adoptRef(new MIDIAccessInitializer(scriptState, options));
        p->keepAliveWhilePending();
        p->suspendIfNeeded();
        return p->start();
    }

    virtual ~MIDIAccessInitializer();

    // MIDIAccessorClient
    virtual void didAddInputPort(const String& id, const String& manufacturer, const String& name, const String& version) OVERRIDE;
    virtual void didAddOutputPort(const String& id, const String& manufacturer, const String& name, const String& version) OVERRIDE;
    virtual void didStartSession(bool success, const String& error, const String& message) OVERRIDE;
    virtual void didReceiveMIDIData(unsigned portIndex, const unsigned char* data, size_t length, double timeStamp) OVERRIDE { }

    void setSysexEnabled(bool value);
    SecurityOrigin* securityOrigin() const;

private:
    ScriptPromise start();

    MIDIAccessInitializer(ScriptState*, const MIDIOptions&);

    ExecutionContext* executionContext() const;

    OwnPtr<MIDIAccessor> m_accessor;
    MIDIOptions m_options;
    bool m_sysexEnabled;
    Vector<PortDescriptor> m_portDescriptors;
};

} // namespace WebCore


#endif // MIDIAccessInitializer_h
