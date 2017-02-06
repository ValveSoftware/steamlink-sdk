// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_ANIMATION_RUNNER_OBSERVER_H_
#define COMPONENTS_MUS_WS_ANIMATION_RUNNER_OBSERVER_H_

namespace mus {
namespace ws {

class AnimationRunnerObserver {
 public:
  virtual void OnAnimationScheduled(uint32_t id) = 0;
  virtual void OnAnimationDone(uint32_t id) = 0;
  virtual void OnAnimationInterrupted(uint32_t id) = 0;
  virtual void OnAnimationCanceled(uint32_t id) = 0;

 protected:
  virtual ~AnimationRunnerObserver() {}
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_ANIMATION_RUNNER_OBSERVER_H_
