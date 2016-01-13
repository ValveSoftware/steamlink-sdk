// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_RENDERING_HELPER_H_
#define CONTENT_COMMON_GPU_MEDIA_RENDERING_HELPER_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_bindings.h"

namespace base {
class MessageLoop;
class WaitableEvent;
}

#if !defined(OS_WIN) && defined(ARCH_CPU_X86_FAMILY)
#define GL_VARIANT_GLX 1
typedef GLXContext NativeContextType;
#else
#define GL_VARIANT_EGL 1
typedef EGLContext NativeContextType;
#endif

namespace content {

struct RenderingHelperParams;

// Creates and draws textures used by the video decoder.
// This class is not thread safe and thus all the methods of this class
// (except for ctor/dtor) ensure they're being run on a single thread.
class RenderingHelper {
 public:
  // Interface for the content provider of the RenderingHelper.
  class Client {
   public:
    // Callback to tell client to render the content.
    virtual void RenderContent(RenderingHelper* helper) = 0;

    // Callback to get the desired window size of the client.
    virtual const gfx::Size& GetWindowSize() = 0;

   protected:
    virtual ~Client() {}
  };

  RenderingHelper();
  ~RenderingHelper();

  // Create the render context and windows by the specified dimensions.
  void Initialize(const RenderingHelperParams& params,
                  base::WaitableEvent* done);

  // Undo the effects of Initialize() and signal |*done|.
  void UnInitialize(base::WaitableEvent* done);

  // Return a newly-created GLES2 texture id of the specified size, and
  // signal |*done|.
  void CreateTexture(uint32 texture_target,
                     uint32* texture_id,
                     const gfx::Size& size,
                     base::WaitableEvent* done);

  // Render thumbnail in the |texture_id| to the FBO buffer using target
  // |texture_target|.
  void RenderThumbnail(uint32 texture_target, uint32 texture_id);

  // Render |texture_id| to the current view port of the screen using target
  // |texture_target|.
  void RenderTexture(uint32 texture_target, uint32 texture_id);

  // Delete |texture_id|.
  void DeleteTexture(uint32 texture_id);

  // Get the platform specific handle to the OpenGL display.
  void* GetGLDisplay();

  // Get the platform specific handle to the OpenGL context.
  NativeContextType GetGLContext();

  // Get rendered thumbnails as RGB.
  // Sets alpha_solid to true if the alpha channel is entirely 0xff.
  void GetThumbnailsAsRGB(std::vector<unsigned char>* rgb,
                          bool* alpha_solid,
                          base::WaitableEvent* done);

 private:
  void Clear();

  void RenderContent();

  void LayoutRenderingAreas();

  // Timer to trigger the RenderContent() repeatly.
  base::RepeatingTimer<RenderingHelper> render_timer_;
  base::MessageLoop* message_loop_;

  NativeContextType gl_context_;

#if defined(GL_VARIANT_EGL)
  EGLDisplay gl_display_;
  EGLSurface gl_surface_;
#else
  XVisualInfo* x_visual_;
#endif

#if defined(OS_WIN)
  HWND window_;
#else
  Display* x_display_;
  Window x_window_;
#endif

  gfx::Size screen_size_;

  // The rendering area of each window on the screen.
  std::vector<gfx::Rect> render_areas_;

  std::vector<base::WeakPtr<Client> > clients_;

  bool render_as_thumbnails_;
  int frame_count_;
  GLuint thumbnails_fbo_id_;
  GLuint thumbnails_texture_id_;
  gfx::Size thumbnails_fbo_size_;
  gfx::Size thumbnail_size_;
  GLuint program_;
  base::TimeDelta frame_duration_;

  DISALLOW_COPY_AND_ASSIGN(RenderingHelper);
};

struct RenderingHelperParams {
  RenderingHelperParams();
  ~RenderingHelperParams();

  // The rendering FPS.
  int rendering_fps;

  // The clients who provide the content for rendering.
  std::vector<base::WeakPtr<RenderingHelper::Client> > clients;

  // Whether the frames are rendered as scaled thumbnails within a
  // larger FBO that is in turn rendered to the window.
  bool render_as_thumbnails;
  // The size of the FBO containing all visible thumbnails.
  gfx::Size thumbnails_page_size;
  // The size of each thumbnail within the FBO.
  gfx::Size thumbnail_size;
};
}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_RENDERING_HELPER_H_
