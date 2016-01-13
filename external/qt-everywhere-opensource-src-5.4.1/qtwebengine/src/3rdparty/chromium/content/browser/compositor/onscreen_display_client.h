// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_ONSCREEN_DISPLAY_CLIENT_H_
#define CONTENT_BROWSER_COMPOSITOR_ONSCREEN_DISPLAY_CLIENT_H_

#include "cc/surfaces/display_client.h"

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cc/surfaces/display.h"

namespace cc {
class ContextProvider;
class SurfaceManager;
}

namespace content {

// This class provides a DisplayClient implementation for drawing directly to an
// onscreen context.
class OnscreenDisplayClient : cc::DisplayClient {
 public:
  OnscreenDisplayClient(
      const scoped_refptr<cc::ContextProvider>& onscreen_context_provider,
      scoped_ptr<cc::OutputSurface> software_surface,
      cc::SurfaceManager* manager);
  virtual ~OnscreenDisplayClient();

  cc::Display* display() { return &display_; }

  // cc::DisplayClient implementation.
  virtual scoped_ptr<cc::OutputSurface> CreateOutputSurface() OVERRIDE;

 private:
  scoped_refptr<cc::ContextProvider> onscreen_context_provider_;
  scoped_ptr<cc::OutputSurface> software_surface_;
  cc::Display display_;

  DISALLOW_COPY_AND_ASSIGN(OnscreenDisplayClient);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_ONSCREEN_DISPLAY_CLIENT_H_
