// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_NPAPI_WEBPLUGIN_ACCELERATED_SURFACE_MAC_H_
#define CONTENT_CHILD_NPAPI_WEBPLUGIN_ACCELERATED_SURFACE_MAC_H_

#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"

// Avoid having to include OpenGL headers here.
typedef struct _CGLContextObject* CGLContextObj;

namespace content {

// Interface class for interacting with an accelerated plugin surface, used
// for the Core Animation flavors of plugin drawing on the Mac.
class WebPluginAcceleratedSurface {
 public:
  virtual ~WebPluginAcceleratedSurface() {}

  // Sets the size of the surface.
  virtual void SetSize(const gfx::Size& size) = 0;

  // Returns the context used to draw into this surface.
  // If initializing the surface failed, this will be NULL.
  virtual CGLContextObj context() = 0;

  // Readies the surface for drawing. Must be called before any drawing session.
  virtual void StartDrawing() = 0;

  // Ends a drawing session. Changes to the surface may not be reflected until
  // this is called.
  virtual void EndDrawing() = 0;
};

}  // namespace content

#endif  // CONTENT_CHILD_NPAPI_WEBPLUGIN_ACCELERATED_SURFACE_MAC_H_
