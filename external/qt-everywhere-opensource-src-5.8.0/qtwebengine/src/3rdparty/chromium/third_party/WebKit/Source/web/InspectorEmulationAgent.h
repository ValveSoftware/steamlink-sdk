// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorEmulationAgent_h
#define InspectorEmulationAgent_h

#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/protocol/Emulation.h"

namespace blink {

class WebLocalFrameImpl;
class WebViewImpl;

class InspectorEmulationAgent final : public InspectorBaseAgent<protocol::Emulation::Metainfo> {
    WTF_MAKE_NONCOPYABLE(InspectorEmulationAgent);
public:
    class Client {
    public:
        virtual ~Client() {}

        virtual void setCPUThrottlingRate(double rate) {}
    };

    static InspectorEmulationAgent* create(WebLocalFrameImpl*, Client*);
    ~InspectorEmulationAgent() override;

    // protocol::Dispatcher::EmulationCommandHandler implementation.
    void resetPageScaleFactor(ErrorString*) override;
    void setPageScaleFactor(ErrorString*, double in_pageScaleFactor) override;
    void setScriptExecutionDisabled(ErrorString*, bool in_value) override;
    void setTouchEmulationEnabled(ErrorString*, bool in_enabled, const protocol::Maybe<String>& in_configuration) override;
    void setEmulatedMedia(ErrorString*, const String& in_media) override;
    void setCPUThrottlingRate(ErrorString*, double in_rate) override;

    // InspectorBaseAgent overrides.
    void disable(ErrorString*) override;
    void restore() override;

    DECLARE_VIRTUAL_TRACE();

private:
    InspectorEmulationAgent(WebLocalFrameImpl*, Client*);
    WebViewImpl* webViewImpl();

    Member<WebLocalFrameImpl> m_webLocalFrameImpl;
    Client* m_client;
};

} // namespace blink

#endif // !defined(InspectorEmulationAgent_h)
