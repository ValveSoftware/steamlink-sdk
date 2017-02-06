// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BroadcastChannel_h
#define BroadcastChannel_h

#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "components/webmessaging/public/interfaces/broadcast_channel.mojom-blink.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/events/EventTarget.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "platform/weborigin/SecurityOrigin.h"

namespace blink {

class BroadcastChannel final
    : public EventTargetWithInlineData
    , public ActiveScriptWrappable
    , public ContextLifecycleObserver
    , public webmessaging::mojom::blink::BroadcastChannelClient {
    DEFINE_WRAPPERTYPEINFO();
    USING_GARBAGE_COLLECTED_MIXIN(BroadcastChannel);
    USING_PRE_FINALIZER(BroadcastChannel, dispose);
    WTF_MAKE_NONCOPYABLE(BroadcastChannel);
public:
    static BroadcastChannel* create(ExecutionContext*, const String& name, ExceptionState&);
    ~BroadcastChannel() override;
    void dispose();

    // IDL
    String name() const { return m_name; }
    void postMessage(const ScriptValue&, ExceptionState&);
    void close();
    DEFINE_ATTRIBUTE_EVENT_LISTENER(message);

    // EventTarget:
    const AtomicString& interfaceName() const override;
    ExecutionContext* getExecutionContext() const override { return ContextLifecycleObserver::getExecutionContext(); }

    // ActiveScriptWrappable:
    bool hasPendingActivity() const override;

    // ContextLifecycleObserver:
    void contextDestroyed() override;

    DECLARE_VIRTUAL_TRACE();

private:
    BroadcastChannel(ExecutionContext*, const String& name);

    // webmessaging::mojom::blink::BroadcastChannelClient:
    void OnMessage(const String& message) override;

    // Called when the mojo binding disconnects.
    void onError();

    RefPtr<SecurityOrigin> m_origin;
    String m_name;

    mojo::AssociatedBinding<webmessaging::mojom::blink::BroadcastChannelClient> m_binding;
    webmessaging::mojom::blink::BroadcastChannelClientAssociatedPtr m_remoteClient;
};

} // namespace blink

#endif // BroadcastChannel_h
