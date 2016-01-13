// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RemoteFrameView_h
#define RemoteFrameView_h

#include "platform/Widget.h"
#include "platform/geometry/IntRect.h"

namespace WebCore {

class RemoteFrame;

class RemoteFrameView : public Widget {
public:
    static PassRefPtr<RemoteFrameView> create(RemoteFrame*);

    virtual ~RemoteFrameView();

    virtual bool isRemoteFrameView() const OVERRIDE { return true; }

    RemoteFrame& frame() const { return *m_remoteFrame; }

    // Override to notify remote frame that its viewport size has changed.
    virtual void frameRectsChanged() OVERRIDE;

    virtual void invalidateRect(const IntRect&) OVERRIDE;

    virtual void setFrameRect(const IntRect&) OVERRIDE;

private:
    explicit RemoteFrameView(RemoteFrame*);

    RefPtr<RemoteFrame> m_remoteFrame;
};

DEFINE_TYPE_CASTS(RemoteFrameView, Widget, widget, widget->isRemoteFrameView(), widget.isRemoteFrameView());

} // namespace WebCore

#endif // RemoteFrameView_h
