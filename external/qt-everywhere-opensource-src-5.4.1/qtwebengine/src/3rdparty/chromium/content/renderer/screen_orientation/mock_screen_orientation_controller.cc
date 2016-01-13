// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/screen_orientation/mock_screen_orientation_controller.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "content/renderer/render_view_impl.h"

namespace content {

MockScreenOrientationController::MockScreenOrientationController()
    : RenderViewObserver(NULL),
      current_lock_(blink::WebScreenOrientationLockDefault),
      device_orientation_(blink::WebScreenOrientationPortraitPrimary),
      current_orientation_(blink::WebScreenOrientationPortraitPrimary) {
  // Since MockScreenOrientationController is held by LazyInstance reference,
  // add this ref for it.
  AddRef();
}

MockScreenOrientationController::~MockScreenOrientationController() {
}

void MockScreenOrientationController::ResetData() {
  if (render_view_impl())
    render_view_impl()->RemoveObserver(this);

  current_lock_ = blink::WebScreenOrientationLockDefault;
  device_orientation_ = blink::WebScreenOrientationPortraitPrimary;
  current_orientation_ = blink::WebScreenOrientationPortraitPrimary;
}

void MockScreenOrientationController::UpdateDeviceOrientation(
    RenderView* render_view,
    blink::WebScreenOrientationType orientation) {
  if (this->render_view()) {
    // Make sure that render_view_ did not change during test.
    DCHECK_EQ(this->render_view(), render_view);
  } else {
    Observe(render_view);
  }

  if (device_orientation_ == orientation)
    return;
  device_orientation_ = orientation;
  if (!IsOrientationAllowedByCurrentLock(orientation))
    return;
  UpdateScreenOrientation(orientation);
}

RenderViewImpl* MockScreenOrientationController::render_view_impl() const {
  return static_cast<RenderViewImpl*>(render_view());
}

void MockScreenOrientationController::UpdateScreenOrientation(
    blink::WebScreenOrientationType orientation) {
  if (current_orientation_ == orientation)
    return;
  current_orientation_ = orientation;
  if (render_view_impl())
    render_view_impl()->SetScreenOrientationForTesting(orientation);
}

bool MockScreenOrientationController::IsOrientationAllowedByCurrentLock(
    blink::WebScreenOrientationType orientation) {
  if (current_lock_ == blink::WebScreenOrientationLockDefault ||
      current_lock_ == blink::WebScreenOrientationLockAny) {
    return true;
  }

  switch (orientation) {
    case blink::WebScreenOrientationPortraitPrimary:
      return current_lock_ == blink::WebScreenOrientationLockPortraitPrimary ||
             current_lock_ == blink::WebScreenOrientationLockPortrait;
    case blink::WebScreenOrientationPortraitSecondary:
      return current_lock_ ==
                 blink::WebScreenOrientationLockPortraitSecondary ||
             current_lock_ == blink::WebScreenOrientationLockPortrait;
    case blink::WebScreenOrientationLandscapePrimary:
      return current_lock_ == blink::WebScreenOrientationLockLandscapePrimary ||
             current_lock_ == blink::WebScreenOrientationLockLandscape;
    case blink::WebScreenOrientationLandscapeSecondary:
      return current_lock_ ==
                 blink::WebScreenOrientationLockLandscapeSecondary ||
             current_lock_ == blink::WebScreenOrientationLockLandscape;
    default:
      return false;
  }
}

void MockScreenOrientationController::OnDestruct() {
}

} // namespace content
