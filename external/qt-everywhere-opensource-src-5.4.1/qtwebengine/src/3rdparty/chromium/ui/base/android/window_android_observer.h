// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_ANDROID_WINDOW_ANDROID_OBSERVER_H_
#define UI_BASE_ANDROID_WINDOW_ANDROID_OBSERVER_H_

#include "ui/base/ui_base_export.h"

namespace ui {

class UI_BASE_EXPORT WindowAndroidObserver {
 public:
  virtual void OnCompositingDidCommit() = 0;
  virtual void OnAttachCompositor() = 0;
  virtual void OnDetachCompositor() = 0;
  virtual void OnVSync(base::TimeTicks frame_time,
                       base::TimeDelta vsync_period) = 0;
  virtual void OnAnimate(base::TimeTicks frame_begin_time) {}

 protected:
  virtual ~WindowAndroidObserver() {}
};

}  // namespace ui

#endif  // UI_BASE_ANDROID_WINDOW_ANDROID_OBSERVER_H_
