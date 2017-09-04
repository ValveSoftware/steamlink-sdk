// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_CONTENT_VIEW_CORE_IMPL_OBSERVER_H
#define CONTENT_BROWSER_ANDROID_CONTENT_VIEW_CORE_IMPL_OBSERVER_H

namespace content {

class ContentViewCoreImplObserver {
 public:
  virtual void OnContentViewCoreDestroyed() = 0;
  virtual void OnAttachedToWindow() = 0;
  virtual void OnDetachedFromWindow() = 0;

 protected:
  virtual ~ContentViewCoreImplObserver() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_CONTENT_VIEW_CORE_IMPL_OBSERVER_H
