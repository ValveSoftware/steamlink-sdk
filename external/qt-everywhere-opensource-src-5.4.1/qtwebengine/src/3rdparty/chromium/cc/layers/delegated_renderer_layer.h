// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_DELEGATED_RENDERER_LAYER_H_
#define CC_LAYERS_DELEGATED_RENDERER_LAYER_H_

#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "cc/base/cc_export.h"
#include "cc/layers/delegated_frame_provider.h"
#include "cc/layers/layer.h"
#include "cc/resources/returned_resource.h"

namespace cc {
class BlockingTaskRunner;

class CC_EXPORT DelegatedRendererLayer : public Layer {
 public:
  static scoped_refptr<DelegatedRendererLayer> Create(
      const scoped_refptr<DelegatedFrameProvider>& frame_provider);

  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE;
  virtual void SetLayerTreeHost(LayerTreeHost* host) OVERRIDE;
  virtual bool Update(ResourceUpdateQueue* queue,
                      const OcclusionTracker<Layer>* occlusion) OVERRIDE;
  virtual void PushPropertiesTo(LayerImpl* impl) OVERRIDE;

  // Called by the DelegatedFrameProvider when a new frame is available to be
  // picked up.
  void ProviderHasNewFrame();

 protected:
  DelegatedRendererLayer(
      const scoped_refptr<DelegatedFrameProvider>& frame_provider);
  virtual ~DelegatedRendererLayer();

 private:
  scoped_refptr<DelegatedFrameProvider> frame_provider_;

  bool should_collect_new_frame_;

  DelegatedFrameData* frame_data_;
  gfx::RectF frame_damage_;

  scoped_refptr<BlockingTaskRunner> main_thread_runner_;
  base::WeakPtrFactory<DelegatedRendererLayer> weak_ptrs_;

  DISALLOW_COPY_AND_ASSIGN(DelegatedRendererLayer);
};

}  // namespace cc

#endif  // CC_LAYERS_DELEGATED_RENDERER_LAYER_H_
