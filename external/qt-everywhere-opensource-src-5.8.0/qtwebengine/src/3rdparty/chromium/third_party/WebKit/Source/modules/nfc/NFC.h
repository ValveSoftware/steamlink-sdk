// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NFC_h
#define NFC_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/modules/v8/StringOrArrayBufferOrNFCMessage.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/page/PageLifecycleObserver.h"
#include "device/nfc/nfc.mojom-blink.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace blink {

class MessageCallback;
class NFCError;
class NFCPushOptions;
using NFCPushMessage = StringOrArrayBufferOrNFCMessage;
class NFCWatchOptions;
class ServiceRegistry;

class NFC final
    : public GarbageCollectedFinalized<NFC>
    , public ScriptWrappable
    , public PageLifecycleObserver
    , public ContextLifecycleObserver
    , public device::nfc::blink::NFCClient {
    DEFINE_WRAPPERTYPEINFO();
    USING_GARBAGE_COLLECTED_MIXIN(NFC);
    USING_PRE_FINALIZER(NFC, dispose);

public:
    static NFC* create(LocalFrame*);

    virtual ~NFC();

    void dispose();

    // ContextLifecycleObserver overrides.
    void contextDestroyed() override;

    // Pushes NFCPushMessage asynchronously to NFC tag / peer.
    ScriptPromise push(ScriptState*, const NFCPushMessage&, const NFCPushOptions&);

    // Cancels ongoing push operation.
    ScriptPromise cancelPush(ScriptState*, const String&);

    // Starts watching for NFC messages that match NFCWatchOptions criteria.
    ScriptPromise watch(ScriptState*, MessageCallback*, const NFCWatchOptions&);

    // Cancels watch operation with id.
    ScriptPromise cancelWatch(ScriptState*, long id);

    // Cancels all watch operations.
    ScriptPromise cancelWatch(ScriptState*);

    // Implementation of PageLifecycleObserver.
    void pageVisibilityChanged() override;

    // Interface required by garbage collection.
    DECLARE_VIRTUAL_TRACE();

private:
    void OnRequestCompleted(ScriptPromiseResolver*, device::nfc::blink::NFCErrorPtr);
    void OnConnectionError();

    // device::nfc::blink::NFCClient implementation.
    void OnWatch(mojo::WTFArray<uint32_t> ids, device::nfc::blink::NFCMessagePtr) override;

private:
    explicit NFC(LocalFrame*);
    device::nfc::blink::NFCPtr m_nfc;
    mojo::Binding<device::nfc::blink::NFCClient> m_client;
    HeapHashSet<Member<ScriptPromiseResolver>> m_requests;
};

} // namespace blink

#endif // NFC_h
