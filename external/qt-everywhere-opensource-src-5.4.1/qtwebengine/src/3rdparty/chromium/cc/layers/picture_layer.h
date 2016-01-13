// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_PICTURE_LAYER_H_
#define CC_LAYERS_PICTURE_LAYER_H_

#include "cc/base/invalidation_region.h"
#include "cc/debug/devtools_instrumentation.h"
#include "cc/debug/micro_benchmark_controller.h"
#include "cc/layers/layer.h"
#include "cc/resources/picture_pile.h"
#include "cc/trees/occlusion_tracker.h"

namespace cc {

class ContentLayerClient;
class ResourceUpdateQueue;

class CC_EXPORT PictureLayer : public Layer {
 public:
  static scoped_refptr<PictureLayer> Create(ContentLayerClient* client);

  void ClearClient() { client_ = NULL; }

  // Layer interface.
  virtual bool DrawsContent() const OVERRIDE;
  virtual scoped_ptr<LayerImpl> CreateLayerImpl(
      LayerTreeImpl* tree_impl) OVERRIDE;
  virtual void SetLayerTreeHost(LayerTreeHost* host) OVERRIDE;
  virtual void PushPropertiesTo(LayerImpl* layer) OVERRIDE;
  virtual void SetNeedsDisplayRect(const gfx::RectF& layer_rect) OVERRIDE;
  virtual bool Update(ResourceUpdateQueue* queue,
                      const OcclusionTracker<Layer>* occlusion) OVERRIDE;
  virtual void SetIsMask(bool is_mask) OVERRIDE;
  virtual bool SupportsLCDText() const OVERRIDE;
  virtual skia::RefPtr<SkPicture> GetPicture() const OVERRIDE;
  virtual bool IsSuitableForGpuRasterization() const OVERRIDE;

  virtual void RunMicroBenchmark(MicroBenchmark* benchmark) OVERRIDE;

  ContentLayerClient* client() { return client_; }

  Picture::RecordingMode RecordingMode() const;

  PicturePile* GetPicturePileForTesting() const { return pile_.get(); }

 protected:
  explicit PictureLayer(ContentLayerClient* client);
  virtual ~PictureLayer();

  void UpdateCanUseLCDText();

 private:
  ContentLayerClient* client_;
  scoped_refptr<PicturePile> pile_;
  devtools_instrumentation::
      ScopedLayerObjectTracker instrumentation_object_tracker_;
  // Invalidation to use the next time update is called.
  InvalidationRegion pending_invalidation_;
  // Invalidation from the last time update was called.
  Region pile_invalidation_;
  gfx::Rect last_updated_visible_content_rect_;
  bool is_mask_;

  int update_source_frame_number_;
  bool can_use_lcd_text_last_frame_;

  DISALLOW_COPY_AND_ASSIGN(PictureLayer);
};

}  // namespace cc

#endif  // CC_LAYERS_PICTURE_LAYER_H_
