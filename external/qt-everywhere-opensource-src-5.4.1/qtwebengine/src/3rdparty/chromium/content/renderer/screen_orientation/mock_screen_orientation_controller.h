// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SCREEN_ORIENTATION_MOCK_SCREEN_ORIENTATION_CONTROLLER_H_
#define CONTENT_RENDERER_SCREEN_ORIENTATION_MOCK_SCREEN_ORIENTATION_CONTROLLER_H_

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/public/renderer/render_view_observer.h"
#include "third_party/WebKit/public/platform/WebScreenOrientationLockType.h"
#include "third_party/WebKit/public/platform/WebScreenOrientationType.h"

namespace content {
class RenderView;
class RenderViewImpl;

class MockScreenOrientationController
    : public base::RefCountedThreadSafe<MockScreenOrientationController>,
      public RenderViewObserver {
 public:
  MockScreenOrientationController();

  void ResetData();
  void UpdateDeviceOrientation(RenderView* render_view,
                               blink::WebScreenOrientationType);

 private:
  virtual ~MockScreenOrientationController();

  void UpdateScreenOrientation(blink::WebScreenOrientationType);
  bool IsOrientationAllowedByCurrentLock(blink::WebScreenOrientationType);
  RenderViewImpl* render_view_impl() const;

  // RenderViewObserver
  virtual void OnDestruct() OVERRIDE;

  blink::WebScreenOrientationLockType current_lock_;
  blink::WebScreenOrientationType device_orientation_;
  blink::WebScreenOrientationType current_orientation_;

  DISALLOW_COPY_AND_ASSIGN(MockScreenOrientationController);
  friend class base::LazyInstance<MockScreenOrientationController>;
  friend class base::RefCountedThreadSafe<MockScreenOrientationController>;
};

} // namespace content

#endif // CONTENT_RENDERER_SCREEN_ORIENTATION_MOCK_SCREEN_ORIENTATION_CONTROLLER_H_
