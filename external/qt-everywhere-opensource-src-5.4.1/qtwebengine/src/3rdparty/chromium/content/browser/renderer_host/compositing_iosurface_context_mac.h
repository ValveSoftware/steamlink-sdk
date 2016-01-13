// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_COMPOSITING_IOSURFACE_CONTEXT_MAC_H_
#define CONTENT_BROWSER_RENDERER_HOST_COMPOSITING_IOSURFACE_CONTEXT_MAC_H_

#import <AppKit/NSOpenGL.h>
#include <OpenGL/OpenGL.h>
#include <map>

#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "ui/gl/scoped_cgl.h"

namespace content {

class CompositingIOSurfaceShaderPrograms;

class CompositingIOSurfaceContext
    : public base::RefCounted<CompositingIOSurfaceContext>,
      public content::GpuDataManagerObserver {
 public:
  enum {
    // The number used to look up the context used for async readback and for
    // initializing the IOSurface.
    kOffscreenContextWindowNumber = -2,
    // The number used to look up the context used by CAOpenGLLayers.
    kCALayerContextWindowNumber = -3,
  };

  // Get or create a GL context for the specified window with the specified
  // surface ordering. Share these GL contexts as much as possible because
  // creating and destroying them can be expensive
  // http://crbug.com/180463
  static scoped_refptr<CompositingIOSurfaceContext> Get(int window_number);

  // Mark that all the GL contexts in the same sharegroup as this context as
  // invalid, so they shouldn't be returned anymore by Get, but rather, new
  // contexts should be created. This is called as a precaution when unexpected
  // GL errors occur.
  void PoisonContextAndSharegroup();
  bool HasBeenPoisoned() const { return poisoned_; }

  CompositingIOSurfaceShaderPrograms* shader_program_cache() const {
    return shader_program_cache_.get();
  }
  CGLContextObj cgl_context() const { return cgl_context_; }
  bool is_vsync_disabled() const { return is_vsync_disabled_; }
  int window_number() const { return window_number_; }

  // content::GpuDataManagerObserver implementation.
  virtual void OnGpuSwitching() OVERRIDE;

 private:
  friend class base::RefCounted<CompositingIOSurfaceContext>;

  CompositingIOSurfaceContext(
      int window_number,
      base::ScopedTypeRef<CGLContextObj> clg_context_strong,
      CGLContextObj clg_context,
      bool is_vsync_disabled_,
      scoped_ptr<CompositingIOSurfaceShaderPrograms> shader_program_cache);
  virtual ~CompositingIOSurfaceContext();

  int window_number_;
  base::ScopedTypeRef<CGLContextObj> cgl_context_strong_;
  // Weak, backed by |cgl_context_strong_|.
  CGLContextObj cgl_context_;

  bool is_vsync_disabled_;
  scoped_ptr<CompositingIOSurfaceShaderPrograms> shader_program_cache_;
  bool poisoned_;

  // The global map from window number and window ordering to
  // context data.
  typedef std::map<int, CompositingIOSurfaceContext*> WindowMap;
  static base::LazyInstance<WindowMap> window_map_;
  static WindowMap* window_map();
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_COMPOSITING_IOSURFACE_CONTEXT_MAC_H_
