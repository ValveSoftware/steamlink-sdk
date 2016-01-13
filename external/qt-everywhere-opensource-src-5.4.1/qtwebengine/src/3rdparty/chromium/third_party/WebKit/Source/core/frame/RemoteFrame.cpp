// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/frame/RemoteFrame.h"

#include "core/frame/RemoteFrameView.h"
#include "core/html/HTMLFrameOwnerElement.h"

namespace WebCore {

inline RemoteFrame::RemoteFrame(FrameClient* client, FrameHost* host, FrameOwner* owner)
    : Frame(client, host, owner)
{
}

PassRefPtr<RemoteFrame> RemoteFrame::create(FrameClient* client, FrameHost* host, FrameOwner* owner)
{
    RefPtr<RemoteFrame> frame = adoptRef(new RemoteFrame(client, host, owner));
    return frame.release();
}

RemoteFrame::~RemoteFrame()
{
    setView(nullptr);
}

void RemoteFrame::setView(PassRefPtr<RemoteFrameView> view)
{
    m_view = view;
}

void RemoteFrame::createView()
{
    RefPtr<RemoteFrameView> view = RemoteFrameView::create(this);
    setView(view);

    if (ownerRenderer()) {
        HTMLFrameOwnerElement* owner = deprecatedLocalOwner();
        ASSERT(owner);
        owner->setWidget(view);
    }
}

} // namespace WebCore
