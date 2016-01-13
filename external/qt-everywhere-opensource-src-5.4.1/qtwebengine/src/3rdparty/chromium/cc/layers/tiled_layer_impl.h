// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_TILED_LAYER_IMPL_H_
#define CC_LAYERS_TILED_LAYER_IMPL_H_

#include <string>

#include "cc/base/cc_export.h"
#include "cc/layers/layer_impl.h"

namespace cc {

class LayerTilingData;
class DrawableTile;

class CC_EXPORT TiledLayerImpl : public LayerImpl {
 public:
  static scoped_ptr<TiledLayerImpl> Create(LayerTreeImpl* tree_impl, int id) {
    return make_scoped_ptr(new TiledLayerImpl(tree_impl, id));
  }
  virtual ~TiledLayerImpl();

  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE;
  virtual void PushPropertiesTo(LayerImpl* layer) OVERRIDE;

  virtual bool WillDraw(DrawMode draw_mode,
                        ResourceProvider* resource_provider) OVERRIDE;
  virtual void AppendQuads(QuadSink* quad_sink,
                           AppendQuadsData* append_quads_data) OVERRIDE;

  virtual ResourceProvider::ResourceId ContentsResourceId() const OVERRIDE;

  void set_skips_draw(bool skips_draw) { skips_draw_ = skips_draw; }
  void SetTilingData(const LayerTilingData& tiler);
  void PushTileProperties(int i,
                          int j,
                          ResourceProvider::ResourceId resource,
                          const gfx::Rect& opaque_rect,
                          bool contents_swizzled);
  void PushInvalidTile(int i, int j);

  virtual Region VisibleContentOpaqueRegion() const OVERRIDE;
  virtual void ReleaseResources() OVERRIDE;

  const LayerTilingData* TilingForTesting() const { return tiler_.get(); }

  virtual size_t GPUMemoryUsageInBytes() const OVERRIDE;

 protected:
  TiledLayerImpl(LayerTreeImpl* tree_impl, int id);
  // Exposed for testing.
  bool HasTileAt(int i, int j) const;
  bool HasResourceIdForTileAt(int i, int j) const;

  virtual void GetDebugBorderProperties(SkColor* color, float* width) const
      OVERRIDE;
  virtual void AsValueInto(base::DictionaryValue* state) const OVERRIDE;

 private:
  virtual const char* LayerTypeAsString() const OVERRIDE;

  DrawableTile* TileAt(int i, int j) const;
  DrawableTile* CreateTile(int i, int j);

  bool skips_draw_;

  scoped_ptr<LayerTilingData> tiler_;

  DISALLOW_COPY_AND_ASSIGN(TiledLayerImpl);
};

}  // namespace cc

#endif  // CC_LAYERS_TILED_LAYER_IMPL_H_
