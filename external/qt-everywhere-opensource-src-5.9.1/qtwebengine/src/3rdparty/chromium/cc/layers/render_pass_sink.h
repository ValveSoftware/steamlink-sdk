// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_RENDER_PASS_SINK_H_
#define CC_LAYERS_RENDER_PASS_SINK_H_

#include <memory>

#include "cc/base/cc_export.h"

namespace cc {
class RenderPass;

class CC_EXPORT RenderPassSink {
 public:
  virtual void AppendRenderPass(std::unique_ptr<RenderPass> render_pass) = 0;

 protected:
  virtual ~RenderPassSink() {}
};

}  // namespace cc

#endif  // CC_LAYERS_RENDER_PASS_SINK_H_
