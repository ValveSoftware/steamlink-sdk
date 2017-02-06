// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/AudioOutputDeviceClientImpl.h"

#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "public/web/WebFrameClient.h"
#include "web/WebLocalFrameImpl.h"
#include <memory>

namespace blink {

AudioOutputDeviceClientImpl* AudioOutputDeviceClientImpl::create()
{
    return new AudioOutputDeviceClientImpl();
}

AudioOutputDeviceClientImpl::AudioOutputDeviceClientImpl()
{
}

AudioOutputDeviceClientImpl::~AudioOutputDeviceClientImpl()
{
}

void AudioOutputDeviceClientImpl::checkIfAudioSinkExistsAndIsAuthorized(ExecutionContext* context, const WebString& sinkId, std::unique_ptr<WebSetSinkIdCallbacks> callbacks)
{
    DCHECK(context);
    DCHECK(context->isDocument());
    Document* document = toDocument(context);
    WebLocalFrameImpl* webFrame = WebLocalFrameImpl::fromFrame(document->frame());
    webFrame->client()->checkIfAudioSinkExistsAndIsAuthorized(sinkId, WebSecurityOrigin(context->getSecurityOrigin()), callbacks.release());
}

} // namespace blink
