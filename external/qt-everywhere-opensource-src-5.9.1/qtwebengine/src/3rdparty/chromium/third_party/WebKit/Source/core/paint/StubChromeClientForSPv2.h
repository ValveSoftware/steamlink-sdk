// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StubChromeClientForSPv2_h
#define StubChromeClientForSPv2_h

#include "core/loader/EmptyClients.h"
#include "platform/testing/WebLayerTreeViewImplForTesting.h"

namespace blink {

// A simple ChromeClient implementation which forwards painted artifacts to a
// PaintArtifactCompositor attached to a testing WebLayerTreeView, and permits
// simple analysis of the results.
class StubChromeClientForSPv2 : public EmptyChromeClient {
 public:
  StubChromeClientForSPv2() : m_layerTreeView() {}

  bool hasLayer(const WebLayer& layer) {
    return m_layerTreeView.hasLayer(layer);
  }

  void attachRootLayer(WebLayer* layer, LocalFrame* localRoot) override {
    m_layerTreeView.setRootLayer(*layer);
  }

 private:
  WebLayerTreeViewImplForTesting m_layerTreeView;
};

}  // namespace blink

#endif  // StubChromeClientForSPv2_h
