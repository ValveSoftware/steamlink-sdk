// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_EXTENSIONS_RENDER_FRAME_OBSERVER_H_
#define EXTENSIONS_RENDERER_EXTENSIONS_RENDER_FRAME_OBSERVER_H_

#include <stdint.h>

#include "base/macros.h"
#include "content/public/renderer/render_frame_observer.h"

namespace extensions {

// This class holds the extensions specific parts of RenderFrame, and has the
// same lifetime.
class ExtensionsRenderFrameObserver
    : public content::RenderFrameObserver {
 public:
  explicit ExtensionsRenderFrameObserver(
      content::RenderFrame* render_frame);
  ~ExtensionsRenderFrameObserver() override;

 private:
  // RenderFrameObserver implementation.
  void DetailedConsoleMessageAdded(const base::string16& message,
                                   const base::string16& source,
                                   const base::string16& stack_trace,
                                   uint32_t line_number,
                                   int32_t severity_level) override;
  void OnDestruct() override;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsRenderFrameObserver);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_EXTENSIONS_RENDER_FRAME_OBSERVER_H_

