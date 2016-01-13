// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "web/RemoteFrameClient.h"

#include "web/WebRemoteFrameImpl.h"

namespace blink {

RemoteFrameClient::RemoteFrameClient(WebRemoteFrameImpl* webFrame)
    : m_webFrame(webFrame)
{
}

WebCore::Frame* RemoteFrameClient::opener() const
{
    return toWebCoreFrame(m_webFrame->opener());
}

void RemoteFrameClient::setOpener(WebCore::Frame*)
{
    // FIXME: Implement.
}

WebCore::Frame* RemoteFrameClient::parent() const
{
    return toWebCoreFrame(m_webFrame->parent());
}

WebCore::Frame* RemoteFrameClient::top() const
{
    return toWebCoreFrame(m_webFrame->top());
}

WebCore::Frame* RemoteFrameClient::previousSibling() const
{
    return toWebCoreFrame(m_webFrame->previousSibling());
}

WebCore::Frame* RemoteFrameClient::nextSibling() const
{
    return toWebCoreFrame(m_webFrame->nextSibling());
}

WebCore::Frame* RemoteFrameClient::firstChild() const
{
    return toWebCoreFrame(m_webFrame->firstChild());
}

WebCore::Frame* RemoteFrameClient::lastChild() const
{
    return toWebCoreFrame(m_webFrame->lastChild());
}

} // namespace blink
