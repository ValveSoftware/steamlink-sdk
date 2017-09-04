// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ANDROID_SYNCHRONOUS_COMPOSITOR_REGISTRY_H_
#define CONTENT_RENDERER_ANDROID_SYNCHRONOUS_COMPOSITOR_REGISTRY_H_


namespace content {
class SynchronousCompositorFrameSink;

class SynchronousCompositorRegistry {
 public:
  virtual void RegisterCompositorFrameSink(
      int routing_id,
      SynchronousCompositorFrameSink* compositor_frame_sink) = 0;
  virtual void UnregisterCompositorFrameSink(
      int routing_id,
      SynchronousCompositorFrameSink* compositor_frame_sink) = 0;

 protected:
  virtual ~SynchronousCompositorRegistry() {}
};

}  // namespace content

#endif  // CONTENT_RENDERER_ANDROID_SYNCHRONOUS_COMPOSITOR_REGISTRY_H_
