// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/broadcastchannel/BroadcastChannel.h"

#include "bindings/core/v8/SerializedScriptValue.h"
#include "core/dom/ExceptionCode.h"
#include "core/events/EventQueue.h"
#include "core/events/MessageEvent.h"
#include "platform/mojo/MojoHelper.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"
#include "wtf/Functional.h"

namespace blink {

namespace {

// To ensure proper ordering of messages send to/from multiple BroadcastChannel
// instances in the same thread this uses one BroadcastChannelService
// connection as basis for all connections to channels from the same thread. The
// actual connections used to send/receive messages are then created using
// associated interfaces, ensuring proper message ordering.
mojom::blink::BroadcastChannelProviderPtr& getThreadSpecificProvider() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      ThreadSpecific<mojom::blink::BroadcastChannelProviderPtr>, provider,
      new ThreadSpecific<mojom::blink::BroadcastChannelProviderPtr>);
  if (!provider.isSet()) {
    Platform::current()->interfaceProvider()->getInterface(
        mojo::GetProxy(&*provider));
  }
  return *provider;
}

}  // namespace

// static
BroadcastChannel* BroadcastChannel::create(ExecutionContext* executionContext,
                                           const String& name,
                                           ExceptionState& exceptionState) {
  if (executionContext->getSecurityOrigin()->isUnique()) {
    // TODO(mek): Decide what to do here depending on
    // https://github.com/whatwg/html/issues/1319
    exceptionState.throwDOMException(
        NotSupportedError, "Can't create BroadcastChannel in an opaque origin");
    return nullptr;
  }
  return new BroadcastChannel(executionContext, name);
}

BroadcastChannel::~BroadcastChannel() {}

void BroadcastChannel::dispose() {
  close();
}

void BroadcastChannel::postMessage(const ScriptValue& message,
                                   ExceptionState& exceptionState) {
  if (!m_binding.is_bound()) {
    exceptionState.throwDOMException(InvalidStateError, "Channel is closed");
    return;
  }
  RefPtr<SerializedScriptValue> value = SerializedScriptValue::serialize(
      message.isolate(), message.v8Value(), nullptr, nullptr, exceptionState);
  if (exceptionState.hadException())
    return;

  Vector<char> data;
  value->toWireBytes(data);
  Vector<uint8_t> mojoData;
  mojoData.appendVector(data);
  m_remoteClient->OnMessage(std::move(mojoData));
}

void BroadcastChannel::close() {
  if (!Platform::current()) {
    // TODO(rockot): Remove this hack once renderer shutdown sequence is fixed.
    // Note that reaching this code indicates that the MessageLoop has already
    // been torn down, so it's impossible for further incoming messages to be
    // dispatched on |m_binding| or reply callbacks to be invoked from
    // |m_remoteClient|.
    return;
  }
  m_remoteClient.reset();
  if (m_binding.is_bound())
    m_binding.Close();
}

const AtomicString& BroadcastChannel::interfaceName() const {
  return EventTargetNames::BroadcastChannel;
}

bool BroadcastChannel::hasPendingActivity() const {
  return m_binding.is_bound() && hasEventListeners(EventTypeNames::message);
}

void BroadcastChannel::contextDestroyed() {
  close();
}

DEFINE_TRACE(BroadcastChannel) {
  ContextLifecycleObserver::trace(visitor);
  EventTargetWithInlineData::trace(visitor);
}

void BroadcastChannel::OnMessage(mojo::WTFArray<uint8_t> message) {
  // Queue a task to dispatch the event.
  RefPtr<SerializedScriptValue> value = SerializedScriptValue::create(
      reinterpret_cast<const char*>(&message.front()), message.size());
  MessageEvent* event = MessageEvent::create(
      nullptr, value.release(),
      getExecutionContext()->getSecurityOrigin()->toString());
  event->setTarget(this);
  bool success = getExecutionContext()->getEventQueue()->enqueueEvent(event);
  DCHECK(success);
  ALLOW_UNUSED_LOCAL(success);
}

void BroadcastChannel::onError() {
  close();
}

BroadcastChannel::BroadcastChannel(ExecutionContext* executionContext,
                                   const String& name)
    : ActiveScriptWrappable(this),
      ContextLifecycleObserver(executionContext),
      m_origin(executionContext->getSecurityOrigin()),
      m_name(name),
      m_binding(this) {
  ThreadState::current()->registerPreFinalizer(this);

  mojom::blink::BroadcastChannelProviderPtr& provider =
      getThreadSpecificProvider();

  // Local BroadcastChannelClient for messages send from the browser to this
  // channel.
  mojom::blink::BroadcastChannelClientAssociatedPtrInfo localClientInfo;
  m_binding.Bind(&localClientInfo, provider.associated_group());
  m_binding.set_connection_error_handler(convertToBaseCallback(
      WTF::bind(&BroadcastChannel::onError, wrapWeakPersistent(this))));

  // Remote BroadcastChannelClient for messages send from this channel to the
  // browser.
  mojom::blink::BroadcastChannelClientAssociatedPtrInfo remoteClientInfo;
  mojo::AssociatedInterfaceRequest<mojom::blink::BroadcastChannelClient>
      remoteCientRequest;
  provider.associated_group()->CreateAssociatedInterface(
      mojo::AssociatedGroup::WILL_PASS_REQUEST, &remoteClientInfo,
      &remoteCientRequest);
  m_remoteClient.Bind(std::move(remoteClientInfo));
  m_remoteClient.set_connection_error_handler(convertToBaseCallback(
      WTF::bind(&BroadcastChannel::onError, wrapWeakPersistent(this))));

  provider->ConnectToChannel(m_origin, m_name, std::move(localClientInfo),
                             std::move(remoteCientRequest));
}

}  // namespace blink
