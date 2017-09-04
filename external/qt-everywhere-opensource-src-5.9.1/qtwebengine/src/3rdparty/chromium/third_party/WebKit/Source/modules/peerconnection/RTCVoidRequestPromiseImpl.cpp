// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/peerconnection/RTCVoidRequestPromiseImpl.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "modules/peerconnection/RTCPeerConnection.h"

namespace blink {

RTCVoidRequestPromiseImpl* RTCVoidRequestPromiseImpl::create(
    RTCPeerConnection* requester,
    ScriptPromiseResolver* resolver) {
  return new RTCVoidRequestPromiseImpl(requester, resolver);
}

RTCVoidRequestPromiseImpl::RTCVoidRequestPromiseImpl(
    RTCPeerConnection* requester,
    ScriptPromiseResolver* resolver)
    : m_requester(requester), m_resolver(resolver) {
  DCHECK(m_requester);
  DCHECK(m_resolver);
}

RTCVoidRequestPromiseImpl::~RTCVoidRequestPromiseImpl() {}

void RTCVoidRequestPromiseImpl::requestSucceeded() {
  if (m_requester && m_requester->shouldFireDefaultCallbacks()) {
    m_resolver->resolve();
  } else {
    // This is needed to have the resolver release its internal resources
    // while leaving the associated promise pending as specified.
    m_resolver->detach();
  }

  clear();
}

void RTCVoidRequestPromiseImpl::requestFailed(const String& error) {
  if (m_requester && m_requester->shouldFireDefaultCallbacks()) {
    // TODO(guidou): The error code should come from the content layer. See
    // crbug.com/589455
    m_resolver->reject(DOMException::create(OperationError, error));
  } else {
    // This is needed to have the resolver release its internal resources
    // while leaving the associated promise pending as specified.
    m_resolver->detach();
  }

  clear();
}

void RTCVoidRequestPromiseImpl::clear() {
  m_requester.clear();
}

DEFINE_TRACE(RTCVoidRequestPromiseImpl) {
  visitor->trace(m_resolver);
  visitor->trace(m_requester);
  RTCVoidRequest::trace(visitor);
}

}  // namespace blink
