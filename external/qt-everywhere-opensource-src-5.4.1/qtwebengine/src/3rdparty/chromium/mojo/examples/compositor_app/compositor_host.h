// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_COMPOSITOR_APP_COMPOSITOR_HOST_H_
#define MOJO_EXAMPLES_COMPOSITOR_APP_COMPOSITOR_HOST_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "cc/trees/layer_tree_host_client.h"
#include "mojo/public/cpp/system/core.h"
#include "ui/gfx/size.h"

namespace cc {
class Layer;
class LayerTreeHost;
}  // namespace cc

namespace mojo {
namespace examples {
class GLES2ClientImpl;

class CompositorHost : public cc::LayerTreeHostClient {
 public:
  explicit CompositorHost(ScopedMessagePipeHandle command_buffer_handle);
  virtual ~CompositorHost();

  void SetSize(const gfx::Size& viewport_size);

  // cc::LayerTreeHostClient implementation.
  virtual void WillBeginMainFrame(int frame_id) OVERRIDE;
  virtual void DidBeginMainFrame() OVERRIDE;
  virtual void Animate(base::TimeTicks frame_begin_time) OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void ApplyScrollAndScale(const gfx::Vector2d& scroll_delta,
                                   float page_scale) OVERRIDE;
  virtual scoped_ptr<cc::OutputSurface> CreateOutputSurface(bool fallback)
      OVERRIDE;
  virtual void DidInitializeOutputSurface() OVERRIDE;
  virtual void WillCommit() OVERRIDE;
  virtual void DidCommit() OVERRIDE;
  virtual void DidCommitAndDrawFrame() OVERRIDE;
  virtual void DidCompleteSwapBuffers() OVERRIDE;

 private:
  void SetupScene();

  ScopedMessagePipeHandle command_buffer_handle_;
  scoped_ptr<cc::LayerTreeHost> tree_;
  scoped_refptr<cc::Layer> child_layer_;
  base::Thread compositor_thread_;
};

}  // namespace examples
}  // namespace mojo

#endif  // MOJO_EXAMPLES_COMPOSITOR_APP_COMPOSITOR_HOST_H_
