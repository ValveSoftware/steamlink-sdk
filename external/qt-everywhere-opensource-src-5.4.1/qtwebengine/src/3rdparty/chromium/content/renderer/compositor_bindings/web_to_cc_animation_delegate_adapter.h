// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_COMPOSITOR_BINDINGS_WEB_TO_CC_ANIMATION_DELEGATE_ADAPTER_H_
#define CONTENT_RENDERER_COMPOSITOR_BINDINGS_WEB_TO_CC_ANIMATION_DELEGATE_ADAPTER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "cc/animation/animation_delegate.h"

namespace blink {
class WebAnimationDelegate;
}

namespace content {

class WebToCCAnimationDelegateAdapter : public cc::AnimationDelegate {
 public:
  explicit WebToCCAnimationDelegateAdapter(
      blink::WebAnimationDelegate* delegate);

 private:
  virtual void NotifyAnimationStarted(
      base::TimeTicks monotonic_time,
      cc::Animation::TargetProperty target_property) OVERRIDE;
  virtual void NotifyAnimationFinished(
      base::TimeTicks monotonic_time,
      cc::Animation::TargetProperty target_property) OVERRIDE;

  blink::WebAnimationDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(WebToCCAnimationDelegateAdapter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_COMPOSITOR_BINDINGS_WEB_TO_CC_ANIMATION_DELEGATE_ADAPTER_H_

