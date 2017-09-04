// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_COMPOSITOR_CLIENT_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_COMPOSITOR_CLIENT_H_

#include "base/macros.h"
#include "content/common/content_export.h"

namespace content {

class CONTENT_EXPORT CompositorClient {
 public:
  // Gives the client a chance to update the layer tree host before compositing.
  virtual void UpdateLayerTreeHost() {}

  // The compositor has completed swapping a frame.
  virtual void OnSwapBuffersCompleted(int pending_swap_buffers) {}

 protected:
  CompositorClient() {}
  virtual ~CompositorClient() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CompositorClient);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_COMPOSITOR_CLIENT_H_
