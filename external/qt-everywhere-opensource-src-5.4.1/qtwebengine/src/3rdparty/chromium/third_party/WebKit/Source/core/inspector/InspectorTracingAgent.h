/*
 * Copyright 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef InspectorTracingAgent_h
#define InspectorTracingAgent_h

#include "core/InspectorFrontend.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class InspectorClient;

class InspectorTracingAgent FINAL
    : public InspectorBaseAgent<InspectorTracingAgent>
    , public InspectorBackendDispatcher::TracingCommandHandler {
    WTF_MAKE_NONCOPYABLE(InspectorTracingAgent);
public:
    static PassOwnPtr<InspectorTracingAgent> create(InspectorClient* client)
    {
        return adoptPtr(new InspectorTracingAgent(client));
    }

    // Base agent methods.
    virtual void restore() OVERRIDE;

    // Protocol method implementations.
    virtual void start(ErrorString*, const String& categoryFilter, const String&, const double*, String* sessionId) OVERRIDE;

    // Methods for other agents to use.
    void setLayerTreeId(int);

private:
    explicit InspectorTracingAgent(InspectorClient*);

    void emitMetadataEvents();
    String sessionId();

    int m_layerTreeId;
    InspectorClient* m_client;
};

}

#endif // InspectorTracingAgent_h
