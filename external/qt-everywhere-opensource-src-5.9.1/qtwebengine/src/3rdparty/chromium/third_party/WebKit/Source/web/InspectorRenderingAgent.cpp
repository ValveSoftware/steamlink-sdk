// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/InspectorRenderingAgent.h"

#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/page/Page.h"
#include "web/InspectorOverlay.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebViewImpl.h"

namespace blink {

namespace RenderingAgentState {
static const char showDebugBorders[] = "showDebugBorders";
static const char showFPSCounter[] = "showFPSCounter";
static const char showPaintRects[] = "showPaintRects";
static const char showScrollBottleneckRects[] = "showScrollBottleneckRects";
static const char showSizeOnResize[] = "showSizeOnResize";
}

InspectorRenderingAgent* InspectorRenderingAgent::create(
    WebLocalFrameImpl* webLocalFrameImpl,
    InspectorOverlay* overlay) {
  return new InspectorRenderingAgent(webLocalFrameImpl, overlay);
}

InspectorRenderingAgent::InspectorRenderingAgent(
    WebLocalFrameImpl* webLocalFrameImpl,
    InspectorOverlay* overlay)
    : m_webLocalFrameImpl(webLocalFrameImpl), m_overlay(overlay) {}

WebViewImpl* InspectorRenderingAgent::webViewImpl() {
  return m_webLocalFrameImpl->viewImpl();
}

void InspectorRenderingAgent::restore() {
  setShowDebugBorders(
      m_state->booleanProperty(RenderingAgentState::showDebugBorders, false));
  setShowFPSCounter(
      m_state->booleanProperty(RenderingAgentState::showFPSCounter, false));
  setShowPaintRects(
      m_state->booleanProperty(RenderingAgentState::showPaintRects, false));
  setShowScrollBottleneckRects(m_state->booleanProperty(
      RenderingAgentState::showScrollBottleneckRects, false));
  setShowViewportSizeOnResize(
      m_state->booleanProperty(RenderingAgentState::showSizeOnResize, false));
}

Response InspectorRenderingAgent::disable() {
  setShowDebugBorders(false);
  setShowFPSCounter(false);
  setShowPaintRects(false);
  setShowScrollBottleneckRects(false);
  setShowViewportSizeOnResize(false);
  return Response::OK();
}

Response InspectorRenderingAgent::setShowDebugBorders(bool show) {
  m_state->setBoolean(RenderingAgentState::showDebugBorders, show);
  if (show) {
    Response response = compositingEnabled();
    if (!response.isSuccess())
      return response;
  }
  webViewImpl()->setShowDebugBorders(show);
  return Response::OK();
}

Response InspectorRenderingAgent::setShowFPSCounter(bool show) {
  m_state->setBoolean(RenderingAgentState::showFPSCounter, show);
  if (show) {
    Response response = compositingEnabled();
    if (!response.isSuccess())
      return response;
  }
  webViewImpl()->setShowFPSCounter(show);
  return Response::OK();
}

Response InspectorRenderingAgent::setShowPaintRects(bool show) {
  m_state->setBoolean(RenderingAgentState::showPaintRects, show);
  webViewImpl()->setShowPaintRects(show);
  if (!show && m_webLocalFrameImpl->frameView())
    m_webLocalFrameImpl->frameView()->invalidate();
  return Response::OK();
}

Response InspectorRenderingAgent::setShowScrollBottleneckRects(bool show) {
  m_state->setBoolean(RenderingAgentState::showScrollBottleneckRects, show);
  if (show) {
    Response response = compositingEnabled();
    if (!response.isSuccess())
      return response;
  }
  webViewImpl()->setShowScrollBottleneckRects(show);
  return Response::OK();
}

Response InspectorRenderingAgent::setShowViewportSizeOnResize(bool show) {
  m_state->setBoolean(RenderingAgentState::showSizeOnResize, show);
  if (m_overlay)
    m_overlay->setShowViewportSizeOnResize(show);
  return Response::OK();
}

Response InspectorRenderingAgent::compositingEnabled() {
  if (!webViewImpl()->page()->settings().acceleratedCompositingEnabled())
    return Response::Error("Compositing mode is not supported");
  return Response::OK();
}

DEFINE_TRACE(InspectorRenderingAgent) {
  visitor->trace(m_webLocalFrameImpl);
  visitor->trace(m_overlay);
  InspectorBaseAgent::trace(visitor);
}

}  // namespace blink
