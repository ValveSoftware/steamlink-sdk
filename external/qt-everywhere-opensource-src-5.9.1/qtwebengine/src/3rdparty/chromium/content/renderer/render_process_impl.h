// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_PROCESS_IMPL_H_
#define CONTENT_RENDERER_RENDER_PROCESS_IMPL_H_

#include "base/macros.h"
#include "content/renderer/render_process.h"

namespace content {

// Implementation of the RenderProcess interface for the regular browser.
// See also MockRenderProcess which implements the active "RenderProcess" when
// running under certain unit tests.
class RenderProcessImpl : public RenderProcess {
 public:
  RenderProcessImpl();
  ~RenderProcessImpl() override;

  // RenderProcess implementation.
  void AddBindings(int bindings) override;
  int GetEnabledBindings() const override;

 private:
  // Bitwise-ORed set of extra bindings that have been enabled anywhere in this
  // process.  See BindingsPolicy for details.
  int enabled_bindings_;

  DISALLOW_COPY_AND_ASSIGN(RenderProcessImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_PROCESS_IMPL_H_
