// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/presentation/PresentationReceiver.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/LocalFrame.h"
#include "modules/presentation/PresentationConnection.h"
#include "modules/presentation/PresentationConnectionList.h"
#include "public/platform/modules/presentation/WebPresentationClient.h"

namespace blink {

PresentationReceiver::PresentationReceiver(LocalFrame* frame,
                                           WebPresentationClient* client)
    : DOMWindowProperty(frame) {
  m_connectionList = new PresentationConnectionList(frame->document());

  if (client)
    client->setReceiver(this);
}

ScriptPromise PresentationReceiver::connectionList(ScriptState* scriptState) {
  if (!m_connectionListProperty)
    m_connectionListProperty =
        new ConnectionListProperty(scriptState->getExecutionContext(), this,
                                   ConnectionListProperty::Ready);

  if (!m_connectionList->isEmpty() &&
      m_connectionListProperty->getState() ==
          ScriptPromisePropertyBase::Pending)
    m_connectionListProperty->resolve(m_connectionList);

  return m_connectionListProperty->promise(scriptState->world());
}

void PresentationReceiver::onReceiverConnectionAvailable(
    WebPresentationConnectionClient* connectionClient) {
  DCHECK(connectionClient);
  // take() will call PresentationReceiver::registerConnection()
  // and register the connection.
  auto connection =
      PresentationConnection::take(this, wrapUnique(connectionClient));

  // receiver.connectionList property not accessed
  if (!m_connectionListProperty)
    return;

  if (m_connectionListProperty->getState() ==
      ScriptPromisePropertyBase::Pending)
    m_connectionListProperty->resolve(m_connectionList);
  else if (m_connectionListProperty->getState() ==
           ScriptPromisePropertyBase::Resolved)
    m_connectionList->dispatchConnectionAvailableEvent(connection);
}

void PresentationReceiver::registerConnection(
    PresentationConnection* connection) {
  DCHECK(m_connectionList);
  m_connectionList->addConnection(connection);
}

DEFINE_TRACE(PresentationReceiver) {
  visitor->trace(m_connectionList);
  visitor->trace(m_connectionListProperty);
  DOMWindowProperty::trace(visitor);
}
}  // namespace blink
