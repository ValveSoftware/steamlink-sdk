// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_GLES2_GL_SURFACE_ADAPTER_H_
#define COMPONENTS_MUS_GLES2_GL_SURFACE_ADAPTER_H_

#include <stdint.h>

#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/swap_result.h"
#include "ui/gl/gl_surface.h"

namespace mus {

// Wraps a GLSurface such that there is a real GLSurface instance acting as
// delegate. Implements the GLSurface interface. The |gl::GLSurfaceAdapter|
// superclass provides a default implementation that forwards all GLSurface
// methods to the provided delegate. |GLSurfaceAdapterMus| overrides only the
// necessary methods to augment the completion callback argument to
// |SwapBuffersAsync|, |PostSubBufferAsync| and |CommitOverlayPlanesAsync| with
// a callback provided with the |SetGpuCompletedSwapBuffersCallback| method.
class GLSurfaceAdapterMus : public gl::GLSurfaceAdapter {
 public:
  // Creates a new |GLSurfaceAdapterMus| that delegates to |surface|.
  explicit GLSurfaceAdapterMus(scoped_refptr<gl::GLSurface> surface);

  // gl::GLSurfaceAdapter.
  void SwapBuffersAsync(
      const gl::GLSurface::SwapCompletionCallback& callback) override;
  void PostSubBufferAsync(
      int x,
      int y,
      int width,
      int height,
      const gl::GLSurface::SwapCompletionCallback& callback) override;
  void CommitOverlayPlanesAsync(
      const gl::GLSurface::SwapCompletionCallback& callback) override;

  // Set an additional callback to run on SwapBuffers completion.
  void SetGpuCompletedSwapBuffersCallback(
      gl::GLSurface::SwapCompletionCallback callback);

 private:
  ~GLSurfaceAdapterMus() override;

  // We wrap the callback given to the |gl::GLSurfaceAdapter| overrides above
  // with an invocation of this function. It invokes |adapter_callback_| and the
  // original |original_callback| provided by the original invoker of
  // |SwapBuffersAsync| etc.
  void WrappedCallbackForSwapBuffersAsync(
      const gl::GLSurface::SwapCompletionCallback& original_callback,
      gfx::SwapResult result);

  gl::GLSurface::SwapCompletionCallback adapter_callback_;

  // gl::GLSurfaceAdapter doesn't own the delegate surface so keep a reference
  // here.
  scoped_refptr<gl::GLSurface> surface_;

  base::WeakPtrFactory<GLSurfaceAdapterMus> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GLSurfaceAdapterMus);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_GLES2_GL_SURFACE_ADAPTER_H_
