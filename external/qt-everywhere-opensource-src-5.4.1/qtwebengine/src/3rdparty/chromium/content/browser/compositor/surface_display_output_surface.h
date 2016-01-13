// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_SURFACE_DISPLAY_OUTPUT_SURFACE_H_
#define CONTENT_BROWSER_COMPOSITOR_SURFACE_DISPLAY_OUTPUT_SURFACE_H_

#include "cc/output/output_surface.h"

namespace cc {
class Display;
class SurfaceManager;
}

namespace content {

// This class is maps a compositor OutputSurface to the surface system's Display
// concept, allowing a compositor client to submit frames for a native root
// window or physical display.
class SurfaceDisplayOutputSurface : public cc::OutputSurface {
 public:
  // The underlying cc::Display and cc::SurfaceManager must outlive this class.
  SurfaceDisplayOutputSurface(
      cc::Display* display,
      cc::SurfaceManager* surface_manager,
      const scoped_refptr<cc::ContextProvider>& context_provider);
  virtual ~SurfaceDisplayOutputSurface();

  // cc::OutputSurface implementation.
  virtual void SwapBuffers(cc::CompositorFrame* frame) OVERRIDE;

 private:
  cc::Display* display_;
  cc::SurfaceManager* surface_manager_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceDisplayOutputSurface);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_SURFACE_DISPLAY_OUTPUT_SURFACE_H_
