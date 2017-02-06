// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/audio_output_devices/SetSinkIdCallbacks.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "modules/audio_output_devices/HTMLMediaElementAudioOutputDevice.h"

namespace blink {

namespace {

DOMException* ToException(WebSetSinkIdError error)
{
    switch (error) {
    case WebSetSinkIdError::NotFound:
        return DOMException::create(NotFoundError, "Requested device not found");
    case WebSetSinkIdError::NotAuthorized:
        return DOMException::create(SecurityError, "No permission to use requested device");
    case WebSetSinkIdError::Aborted:
        return DOMException::create(AbortError, "The operation could not be performed and was aborted");
    case WebSetSinkIdError::NotSupported:
        return DOMException::create(NotSupportedError, "Operation not supported");
    default:
        ASSERT_NOT_REACHED();
        return DOMException::create(AbortError, "Invalid error code");
    }
}

} // namespace

SetSinkIdCallbacks::SetSinkIdCallbacks(ScriptPromiseResolver* resolver, HTMLMediaElement& element, const String& sinkId)
    : m_resolver(resolver)
    , m_element(element)
    , m_sinkId(sinkId)
{
    ASSERT(m_resolver);
}

SetSinkIdCallbacks::~SetSinkIdCallbacks()
{
}

void SetSinkIdCallbacks::onSuccess()
{
    if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
        return;

    HTMLMediaElementAudioOutputDevice& aodElement = HTMLMediaElementAudioOutputDevice::from(*m_element);
    aodElement.setSinkId(m_sinkId);
    m_resolver->resolve();
}

void SetSinkIdCallbacks::onError(WebSetSinkIdError error)
{
    if (!m_resolver->getExecutionContext() || m_resolver->getExecutionContext()->activeDOMObjectsAreStopped())
        return;

    m_resolver->reject(ToException(error));
}

} // namespace blink
