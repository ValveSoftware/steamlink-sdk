// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_SCROLLBAR_ANIMATION_CONTROLLER_LINEAR_FADE_H_
#define CC_INPUT_SCROLLBAR_ANIMATION_CONTROLLER_LINEAR_FADE_H_

#include <memory>

#include "base/macros.h"
#include "cc/base/cc_export.h"
#include "cc/input/scrollbar_animation_controller.h"

namespace cc {
class LayerImpl;

class CC_EXPORT ScrollbarAnimationControllerLinearFade
    : public ScrollbarAnimationController {
 public:
  static std::unique_ptr<ScrollbarAnimationControllerLinearFade> Create(
      int scroll_layer_id,
      ScrollbarAnimationControllerClient* client,
      base::TimeDelta delay_before_starting,
      base::TimeDelta resize_delay_before_starting,
      base::TimeDelta duration);

  ~ScrollbarAnimationControllerLinearFade() override;

  void DidScrollUpdate(bool on_resize) override;

 protected:
  ScrollbarAnimationControllerLinearFade(
      int scroll_layer_id,
      ScrollbarAnimationControllerClient* client,
      base::TimeDelta delay_before_starting,
      base::TimeDelta resize_delay_before_starting,
      base::TimeDelta duration);

  void RunAnimationFrame(float progress) override;

 private:
  float OpacityAtTime(base::TimeTicks now) const;
  void ApplyOpacityToScrollbars(float opacity);

  DISALLOW_COPY_AND_ASSIGN(ScrollbarAnimationControllerLinearFade);
};

}  // namespace cc

#endif  // CC_INPUT_SCROLLBAR_ANIMATION_CONTROLLER_LINEAR_FADE_H_
