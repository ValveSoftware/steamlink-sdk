// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/peerconnection/RTCSessionDescriptionRequestPromiseImpl.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "modules/peerconnection/RTCPeerConnection.h"
#include "modules/peerconnection/RTCSessionDescription.h"
#include "public/platform/WebRTCSessionDescription.h"

namespace blink {

RTCSessionDescriptionRequestPromiseImpl* RTCSessionDescriptionRequestPromiseImpl::create(RTCPeerConnection* requester, ScriptPromiseResolver* resolver)
{
    return new RTCSessionDescriptionRequestPromiseImpl(requester, resolver);
}

RTCSessionDescriptionRequestPromiseImpl::RTCSessionDescriptionRequestPromiseImpl(RTCPeerConnection* requester, ScriptPromiseResolver* resolver)
    : m_requester(requester)
    , m_resolver(resolver)
{
    DCHECK(m_requester);
    DCHECK(m_resolver);
}

RTCSessionDescriptionRequestPromiseImpl::~RTCSessionDescriptionRequestPromiseImpl()
{
    DCHECK(!m_requester);
}

void RTCSessionDescriptionRequestPromiseImpl::requestSucceeded(const WebRTCSessionDescription& webSessionDescription)
{
    if (m_requester && m_requester->shouldFireDefaultCallbacks()) {
        m_resolver->resolve(RTCSessionDescription::create(webSessionDescription));
    } else {
        // This is needed to have the resolver release its internal resources
        // while leaving the associated promise pending as specified.
        m_resolver->detach();
    }

    clear();
}

void RTCSessionDescriptionRequestPromiseImpl::requestFailed(const String& error)
{
    if (m_requester && m_requester->shouldFireDefaultCallbacks()) {
        // TODO(guidou): The error code should come from the content layer. See crbug.com/589455
        m_resolver->reject(DOMException::create(OperationError, error));
    } else {
        // This is needed to have the resolver release its internal resources
        // while leaving the associated promise pending as specified.
        m_resolver->detach();
    }

    clear();
}

void RTCSessionDescriptionRequestPromiseImpl::clear()
{
    m_requester.clear();
}

DEFINE_TRACE(RTCSessionDescriptionRequestPromiseImpl)
{
    visitor->trace(m_resolver);
    visitor->trace(m_requester);
    RTCSessionDescriptionRequest::trace(visitor);
}

} // namespace blink
