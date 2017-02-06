// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FakeGraphicsLayerClient_h
#define FakeGraphicsLayerClient_h

#include "platform/graphics/GraphicsLayerClient.h"

namespace blink {

// A simple GraphicsLayerClient implementation suitable for use in unit tests.
class FakeGraphicsLayerClient : public GraphicsLayerClient {
public:
    // GraphicsLayerClient implementation.
    IntRect computeInterestRect(const GraphicsLayer*, const IntRect&) const override { return IntRect(); }
    String debugName(const GraphicsLayer*) const override { return String(); }
    bool isTrackingPaintInvalidations() const override { return m_isTrackingPaintInvalidations; }
    bool needsRepaint(const GraphicsLayer&) const override { return true; }
    void paintContents(const GraphicsLayer*, GraphicsContext&, GraphicsLayerPaintingPhase, const IntRect&) const override { }

    void setIsTrackingPaintInvalidations(bool isTrackingPaintInvalidations)
    {
        m_isTrackingPaintInvalidations = isTrackingPaintInvalidations;
    }

private:
    bool m_isTrackingPaintInvalidations = false;
};

} // namespace blink

#endif // FakeGraphicsLayerClient_h
