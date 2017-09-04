// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintArtifactCompositor_h
#define PaintArtifactCompositor_h

#include "base/memory/ref_counted.h"
#include "platform/PlatformExport.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsLayerClient.h"
#include "platform/graphics/paint/PaintController.h"
#include "wtf/Noncopyable.h"
#include "wtf/PtrUtil.h"
#include "wtf/Vector.h"
#include <memory>

namespace cc {
class Layer;
}

namespace gfx {
class Vector2dF;
}

namespace blink {

class JSONObject;
class PaintArtifact;
class WebLayer;
struct PaintChunk;

// Responsible for managing compositing in terms of a PaintArtifact.
//
// Owns a subtree of the compositor layer tree, and updates it in response to
// changes in the paint artifact.
//
// PaintArtifactCompositor is the successor to PaintLayerCompositor, reflecting
// the new home of compositing decisions after paint in Slimming Paint v2.
class PLATFORM_EXPORT PaintArtifactCompositor {
  WTF_MAKE_NONCOPYABLE(PaintArtifactCompositor);

 public:
  ~PaintArtifactCompositor();

  static std::unique_ptr<PaintArtifactCompositor> create() {
    return wrapUnique(new PaintArtifactCompositor());
  }

  // Updates the layer tree to match the provided paint artifact.
  void update(
      const PaintArtifact&,
      RasterInvalidationTrackingMap<const PaintChunk>* paintChunkInvalidations);

  // The root layer of the tree managed by this object.
  cc::Layer* rootLayer() const { return m_rootLayer.get(); }

  // Wraps rootLayer(), so that it can be attached as a child of another
  // WebLayer.
  WebLayer* getWebLayer() const { return m_webLayer.get(); }

  // Returns extra information recorded during unit tests.
  // While not part of the normal output of this class, this provides a simple
  // way of locating the layers of interest, since there are still a slew of
  // placeholder layers required.
  struct ExtraDataForTesting {
    Vector<scoped_refptr<cc::Layer>> contentLayers;
  };
  void enableExtraDataForTesting() { m_extraDataForTestingEnabled = true; }
  ExtraDataForTesting* getExtraDataForTesting() const {
    return m_extraDataForTesting.get();
  }

  void setTracksRasterInvalidations(bool);
  void resetTrackedRasterInvalidations();
  bool hasTrackedRasterInvalidations() const;

  std::unique_ptr<JSONObject> layersAsJSON(LayerTreeFlags) const;

 private:
  PaintArtifactCompositor();

  class ContentLayerClientImpl;

  // Builds a leaf layer that represents a single paint chunk.
  // Note: cc::Layer API assumes the layer bounds start at (0, 0), but the
  // bounding box of a paint chunk does not necessarily start at (0, 0) (and
  // could even be negative). Internally the generated layer translates the
  // paint chunk to align the bounding box to (0, 0) and return the actual
  // origin of the paint chunk in the |layerOffset| outparam.
  scoped_refptr<cc::Layer> layerForPaintChunk(
      const PaintArtifact&,
      const PaintChunk&,
      gfx::Vector2dF& layerOffset,
      Vector<std::unique_ptr<ContentLayerClientImpl>>& newContentLayerClients,
      RasterInvalidationTracking*);

  // Finds a client among the current vector of clients that matches the paint
  // chunk's id, or otherwise allocates a new one.
  std::unique_ptr<ContentLayerClientImpl> clientForPaintChunk(
      const PaintChunk&,
      const PaintArtifact&);

  scoped_refptr<cc::Layer> m_rootLayer;
  std::unique_ptr<WebLayer> m_webLayer;
  Vector<std::unique_ptr<ContentLayerClientImpl>> m_contentLayerClients;

  bool m_extraDataForTestingEnabled = false;
  std::unique_ptr<ExtraDataForTesting> m_extraDataForTesting;
  friend class StubChromeClientForSPv2;

  bool m_isTrackingRasterInvalidations;
};

}  // namespace blink

#endif  // PaintArtifactCompositor_h
