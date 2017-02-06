// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediasession/MediaSession.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/loader/FrameLoaderClient.h"
#include "modules/mediasession/MediaMetadata.h"
#include "modules/mediasession/MediaSessionError.h"
#include <memory>

namespace blink {

MediaSession::MediaSession(std::unique_ptr<WebMediaSession> webMediaSession)
    : m_webMediaSession(std::move(webMediaSession))
{
    DCHECK(m_webMediaSession);
}

MediaSession* MediaSession::create(ExecutionContext* context, ExceptionState& exceptionState)
{
    Document* document = toDocument(context);
    LocalFrame* frame = document->frame();
    FrameLoaderClient* client = frame->loader().client();
    std::unique_ptr<WebMediaSession> webMediaSession = client->createWebMediaSession();
    if (!webMediaSession) {
        exceptionState.throwDOMException(NotSupportedError, "Missing platform implementation.");
        return nullptr;
    }
    return new MediaSession(std::move(webMediaSession));
}

ScriptPromise MediaSession::activate(ScriptState* scriptState)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    m_webMediaSession->activate(new CallbackPromiseAdapter<void, MediaSessionError>(resolver));
    return promise;
}

ScriptPromise MediaSession::deactivate(ScriptState* scriptState)
{
    ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    m_webMediaSession->deactivate(new CallbackPromiseAdapter<void, void>(resolver));
    return promise;
}

void MediaSession::setMetadata(MediaMetadata* metadata)
{
    m_metadata = metadata;
    if (metadata) {
        WebMediaMetadata webMetadata = (WebMediaMetadata) *metadata;
        m_webMediaSession->setMetadata(&webMetadata);
    } else {
        m_webMediaSession->setMetadata(nullptr);
    }
}

MediaMetadata* MediaSession::metadata() const
{
    return m_metadata;
}

DEFINE_TRACE(MediaSession)
{
    visitor->trace(m_metadata);
}

} // namespace blink
