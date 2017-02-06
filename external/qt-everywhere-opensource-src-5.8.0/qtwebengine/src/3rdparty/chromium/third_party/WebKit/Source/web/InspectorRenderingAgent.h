// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorRenderingAgent_h
#define InspectorRenderingAgent_h

#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/protocol/Rendering.h"

namespace blink {

class InspectorOverlay;
class WebLocalFrameImpl;
class WebViewImpl;

class InspectorRenderingAgent final : public InspectorBaseAgent<protocol::Rendering::Metainfo> {
    WTF_MAKE_NONCOPYABLE(InspectorRenderingAgent);
public:
    static InspectorRenderingAgent* create(WebLocalFrameImpl*, InspectorOverlay*);

    // protocol::Dispatcher::PageCommandHandler implementation.
    void setShowPaintRects(ErrorString*, bool show) override;
    void setShowDebugBorders(ErrorString*, bool show) override;
    void setShowFPSCounter(ErrorString*, bool show) override;
    void setShowScrollBottleneckRects(ErrorString*, bool show) override;
    void setShowViewportSizeOnResize(ErrorString*, bool show) override;

    // InspectorBaseAgent overrides.
    void disable(ErrorString*) override;
    void restore() override;

    DECLARE_VIRTUAL_TRACE();

private:
    InspectorRenderingAgent(WebLocalFrameImpl*, InspectorOverlay*);
    bool compositingEnabled(ErrorString*);
    WebViewImpl* webViewImpl();

    Member<WebLocalFrameImpl> m_webLocalFrameImpl;
    Member<InspectorOverlay> m_overlay;
};


} // namespace blink

#endif // !defined(InspectorRenderingAgent_h)
