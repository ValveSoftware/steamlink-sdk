// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/frame/RemoteFrameView.h"

#include "core/frame/RemoteFrame.h"
#include "core/rendering/RenderPart.h"

namespace WebCore {

RemoteFrameView::RemoteFrameView(RemoteFrame* remoteFrame)
    : m_remoteFrame(remoteFrame)
{
    ASSERT(remoteFrame);
}

RemoteFrameView::~RemoteFrameView()
{
}

PassRefPtr<RemoteFrameView> RemoteFrameView::create(RemoteFrame* remoteFrame)
{
    RefPtr<RemoteFrameView> view = adoptRef(new RemoteFrameView(remoteFrame));
    view->show();
    return view.release();
}

void RemoteFrameView::invalidateRect(const IntRect& rect)
{
    RenderPart* renderer = m_remoteFrame->ownerRenderer();
    if (!renderer)
        return;

    IntRect repaintRect = rect;
    repaintRect.move(renderer->borderLeft() + renderer->paddingLeft(),
        renderer->borderTop() + renderer->paddingTop());
    renderer->invalidatePaintRectangle(repaintRect);
}

void RemoteFrameView::setFrameRect(const IntRect& newRect)
{
    IntRect oldRect = frameRect();

    if (newRect == oldRect)
        return;

    Widget::setFrameRect(newRect);

    frameRectsChanged();
}

void RemoteFrameView::frameRectsChanged()
{
    // FIXME: Notify embedder via WebLocalFrameClient when that is possible.
}

} // namespace WebCore
