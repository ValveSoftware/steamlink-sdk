// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_PAINTED_SCROLLBAR_LAYER_H_
#define CC_LAYERS_PAINTED_SCROLLBAR_LAYER_H_

#include "cc/base/cc_export.h"
#include "cc/input/scrollbar.h"
#include "cc/layers/contents_scaling_layer.h"
#include "cc/layers/scrollbar_layer_interface.h"
#include "cc/layers/scrollbar_theme_painter.h"
#include "cc/resources/layer_updater.h"
#include "cc/resources/scoped_ui_resource.h"

namespace cc {
class ScrollbarThemeComposite;

class CC_EXPORT PaintedScrollbarLayer : public ScrollbarLayerInterface,
                                        public ContentsScalingLayer {
 public:
  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE;

  static scoped_refptr<PaintedScrollbarLayer> Create(
      scoped_ptr<Scrollbar> scrollbar,
      int scroll_layer_id);

  virtual bool OpacityCanAnimateOnImplThread() const OVERRIDE;
  virtual ScrollbarLayerInterface* ToScrollbarLayer() OVERRIDE;

  // ScrollbarLayerInterface
  virtual int ScrollLayerId() const OVERRIDE;
  virtual void SetScrollLayer(int layer_id) OVERRIDE;
  virtual void SetClipLayer(int layer_id) OVERRIDE;

  virtual ScrollbarOrientation orientation() const OVERRIDE;

  // Layer interface
  virtual bool Update(ResourceUpdateQueue* queue,
                      const OcclusionTracker<Layer>* occlusion) OVERRIDE;
  virtual void SetLayerTreeHost(LayerTreeHost* host) OVERRIDE;
  virtual void PushPropertiesTo(LayerImpl* layer) OVERRIDE;
  virtual void PushScrollClipPropertiesTo(LayerImpl* layer) OVERRIDE;
  virtual void CalculateContentsScale(float ideal_contents_scale,
                                      float device_scale_factor,
                                      float page_scale_factor,
                                      float maximum_animation_contents_scale,
                                      bool animating_transform_to_screen,
                                      float* contents_scale_x,
                                      float* contents_scale_y,
                                      gfx::Size* content_bounds) OVERRIDE;

 protected:
  PaintedScrollbarLayer(scoped_ptr<Scrollbar> scrollbar, int scroll_layer_id);
  virtual ~PaintedScrollbarLayer();

  // For unit tests
  UIResourceId track_resource_id() {
    return track_resource_.get() ? track_resource_->id() : 0;
  }
  UIResourceId thumb_resource_id() {
    return thumb_resource_.get() ? thumb_resource_->id() : 0;
  }
  void UpdateThumbAndTrackGeometry();

 private:
  gfx::Rect ScrollbarLayerRectToContentRect(const gfx::Rect& layer_rect) const;
  gfx::Rect OriginThumbRect() const;

  template<typename T> void UpdateProperty(T value, T* prop) {
    if (*prop == value)
      return;
    *prop = value;
    SetNeedsPushProperties();
  }

  int MaxTextureSize();
  float ClampScaleToMaxTextureSize(float scale);

  UIResourceBitmap RasterizeScrollbarPart(const gfx::Rect& layer_rect,
                                          const gfx::Rect& content_rect,
                                          ScrollbarPart part);

  scoped_ptr<Scrollbar> scrollbar_;
  int scroll_layer_id_;
  int clip_layer_id_;

  // Snapshot of properties taken in UpdateThumbAndTrackGeometry and used in
  // PushPropertiesTo.
  int thumb_thickness_;
  int thumb_length_;
  gfx::Point location_;
  gfx::Rect track_rect_;
  bool is_overlay_;
  bool has_thumb_;

  scoped_ptr<ScopedUIResource> track_resource_;
  scoped_ptr<ScopedUIResource> thumb_resource_;

  DISALLOW_COPY_AND_ASSIGN(PaintedScrollbarLayer);
};

}  // namespace cc

#endif  // CC_LAYERS_PAINTED_SCROLLBAR_LAYER_H_
