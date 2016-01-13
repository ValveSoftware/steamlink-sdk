// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_LAYER_ANIMATOR_COLLECTION_H_
#define UI_COMPOSITOR_LAYER_ANIMATOR_COLLECTION_H_

#include <set>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "ui/compositor/compositor_export.h"

namespace base {
class TimeTicks;
}

namespace ui {

class LayerAnimator;

class COMPOSITOR_EXPORT LayerAnimatorCollectionDelegate {
 public:
  virtual ~LayerAnimatorCollectionDelegate() {}

  virtual void ScheduleAnimationForLayerCollection() = 0;
};

// A collection of LayerAnimators that should be updated at each animation step
// in the compositor.
class COMPOSITOR_EXPORT LayerAnimatorCollection {
 public:
  explicit LayerAnimatorCollection(LayerAnimatorCollectionDelegate* delegate);
  ~LayerAnimatorCollection();

  void StartAnimator(scoped_refptr<LayerAnimator> animator);
  void StopAnimator(scoped_refptr<LayerAnimator> animator);

  bool HasActiveAnimators() const;

  void Progress(base::TimeTicks now);

  base::TimeTicks last_tick_time() const { return last_tick_time_; }

 private:
  LayerAnimatorCollectionDelegate* delegate_;
  base::TimeTicks last_tick_time_;
  std::set<scoped_refptr<LayerAnimator> > animators_;

  DISALLOW_COPY_AND_ASSIGN(LayerAnimatorCollection);
};

}  // namespace ui

#endif  // UI_COMPOSITOR_LAYER_ANIMATOR_COLLECTION_H_
