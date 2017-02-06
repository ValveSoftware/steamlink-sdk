/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef InspectorBaseAgent_h
#define InspectorBaseAgent_h

#include "core/CoreExport.h"
#include "core/InstrumentingAgents.h"
#include "platform/heap/Handle.h"
#include "platform/inspector_protocol/DispatcherBase.h"
#include "platform/inspector_protocol/Maybe.h"
#include "platform/inspector_protocol/Values.h"
#include "wtf/Forward.h"
#include "wtf/text/WTFString.h"

namespace blink {

class LocalFrame;

using protocol::Maybe;

class CORE_EXPORT InspectorAgent : public GarbageCollectedFinalized<InspectorAgent> {
public:
    InspectorAgent() { }
    virtual ~InspectorAgent() { }
    DEFINE_INLINE_VIRTUAL_TRACE() { }

    virtual void restore() { }
    virtual void didCommitLoadForLocalFrame(LocalFrame*) { }
    virtual void flushPendingProtocolNotifications() { }

    virtual void init(InstrumentingAgents*, protocol::UberDispatcher*, protocol::DictionaryValue*) = 0;
    virtual void dispose() = 0;
};

template<typename DomainMetainfo>
class InspectorBaseAgent : public InspectorAgent, public DomainMetainfo::BackendClass {
public:
    ~InspectorBaseAgent() override { }

    void init(InstrumentingAgents* instrumentingAgents, protocol::UberDispatcher* dispatcher, protocol::DictionaryValue* state) override
    {
        m_instrumentingAgents = instrumentingAgents;
        m_frontend.reset(new typename DomainMetainfo::FrontendClass(dispatcher->channel()));
        DomainMetainfo::DispatcherClass::wire(dispatcher, this);

        m_state = state->getObject(DomainMetainfo::domainName);
        if (!m_state) {
            std::unique_ptr<protocol::DictionaryValue> newState = protocol::DictionaryValue::create();
            m_state = newState.get();
            state->setObject(DomainMetainfo::domainName, std::move(newState));
        }
    }

    void disable(ErrorString*) override { }

    void dispose() override
    {
        ErrorString error;
        disable(&error);
        m_frontend.reset();
        m_state = nullptr;
        m_instrumentingAgents = nullptr;
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_instrumentingAgents);
        InspectorAgent::trace(visitor);
    }

protected:
    InspectorBaseAgent() { }

    typename DomainMetainfo::FrontendClass* frontend() const { return m_frontend.get(); }
    Member<InstrumentingAgents> m_instrumentingAgents;
    protocol::DictionaryValue* m_state;

private:
    std::unique_ptr<typename DomainMetainfo::FrontendClass> m_frontend;
};

} // namespace blink

#endif // !defined(InspectorBaseAgent_h)
