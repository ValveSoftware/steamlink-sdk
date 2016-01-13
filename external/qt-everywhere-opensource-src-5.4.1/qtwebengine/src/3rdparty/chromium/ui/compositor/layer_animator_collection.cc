// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer_animator_collection.h"

#include <set>

#include "base/time/time.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/frame_time.h"

namespace ui {

LayerAnimatorCollection::LayerAnimatorCollection(
    LayerAnimatorCollectionDelegate* delegate)
    : delegate_(delegate),
      last_tick_time_(gfx::FrameTime::Now()) {
}

LayerAnimatorCollection::~LayerAnimatorCollection() {
}

void LayerAnimatorCollection::StartAnimator(
    scoped_refptr<LayerAnimator> animator) {
  DCHECK_EQ(0U, animators_.count(animator));
  if (!animators_.size())
    last_tick_time_ = gfx::FrameTime::Now();
  animators_.insert(animator);
  if (delegate_)
    delegate_->ScheduleAnimationForLayerCollection();
}

void LayerAnimatorCollection::StopAnimator(
    scoped_refptr<LayerAnimator> animator) {
  DCHECK_GT(animators_.count(animator), 0U);
  animators_.erase(animator);
}

bool LayerAnimatorCollection::HasActiveAnimators() const {
  return !animators_.empty();
}

void LayerAnimatorCollection::Progress(base::TimeTicks now) {
  last_tick_time_ = now;
  std::set<scoped_refptr<LayerAnimator> > list = animators_;
  for (std::set<scoped_refptr<LayerAnimator> >::iterator iter = list.begin();
       iter != list.end();
       ++iter) {
    // Make sure the animator is still valid.
    if (animators_.count(*iter) > 0)
      (*iter)->Step(now);
  }
}

}  // namespace ui
