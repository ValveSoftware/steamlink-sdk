// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_COMPOSITING_IOSURFACE_MAC_H_
#define CONTENT_BROWSER_RENDERER_HOST_COMPOSITING_IOSURFACE_MAC_H_

#include <deque>
#include <list>
#include <vector>

#import <Cocoa/Cocoa.h>
#include <IOSurface/IOSurfaceAPI.h>
#include <QuartzCore/QuartzCore.h>

#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "media/base/video_frame.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/size.h"

class SkBitmap;

namespace gfx {
class Rect;
}

namespace content {

class CompositingIOSurfaceContext;
class CompositingIOSurfaceShaderPrograms;
class CompositingIOSurfaceTransformer;
class RenderWidgetHostViewFrameSubscriber;
class RenderWidgetHostViewMac;

// This class manages an OpenGL context and IOSurface for the accelerated
// compositing code path. The GL context is attached to
// RenderWidgetHostViewCocoa for blitting the IOSurface.
class CompositingIOSurfaceMac
    : public base::RefCounted<CompositingIOSurfaceMac> {
 public:
  // Returns NULL if IOSurface or GL API calls fail.
  static scoped_refptr<CompositingIOSurfaceMac> Create();

  // Set IOSurface that will be drawn on the next NSView drawRect.
  bool SetIOSurfaceWithContextCurrent(
      scoped_refptr<CompositingIOSurfaceContext> current_context,
      IOSurfaceID io_surface_handle,
      const gfx::Size& size,
      float scale_factor) WARN_UNUSED_RESULT;

  // Get the CGL renderer ID currently associated with this context.
  int GetRendererID();

  // Blit the IOSurface to the rectangle specified by |window_rect| in DIPs,
  // with the origin in the lower left corner. If the window rect's size is
  // larger than the IOSurface, the remaining right and bottom edges will be
  // white. |window_scale_factor| is 1 in normal views, 2 in HiDPI views.
  bool DrawIOSurface(
      scoped_refptr<CompositingIOSurfaceContext> drawing_context,
      const gfx::Rect& window_rect,
      float window_scale_factor) WARN_UNUSED_RESULT;

  // Copy the data of the "live" OpenGL texture referring to this IOSurfaceRef
  // into |out|. The copied region is specified with |src_pixel_subrect| and
  // the data is transformed so that it fits in |dst_pixel_size|.
  // |src_pixel_subrect| and |dst_pixel_size| are not in DIP but in pixel.
  // Caller must ensure that |out| is allocated to dimensions that match
  // dst_pixel_size, with no additional padding.
  // |callback| is invoked when the operation is completed or failed.
  // Do no call this method again before |callback| is invoked.
  void CopyTo(const gfx::Rect& src_pixel_subrect,
              const gfx::Size& dst_pixel_size,
              const base::Callback<void(bool, const SkBitmap&)>& callback);

  // Transfer the contents of the surface to an already-allocated YV12
  // VideoFrame, and invoke a callback to indicate success or failure.
  void CopyToVideoFrame(
      const gfx::Rect& src_subrect,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(bool)>& callback);

  // Unref the IOSurface and delete the associated GL texture. If the GPU
  // process is no longer referencing it, this will delete the IOSurface.
  void UnrefIOSurface();

  bool HasIOSurface() { return !!io_surface_.get(); }

  const gfx::Size& pixel_io_surface_size() const {
    return pixel_io_surface_size_;
  }
  // In cocoa view units / DIPs.
  const gfx::Size& dip_io_surface_size() const { return dip_io_surface_size_; }
  float scale_factor() const { return scale_factor_; }

  // Returns true if asynchronous readback is supported on this system.
  bool IsAsynchronousReadbackSupported();

  // Scan the list of started asynchronous copies and test if each one has
  // completed. If |block_until_finished| is true, then block until all
  // pending copies are finished.
  void CheckIfAllCopiesAreFinished(bool block_until_finished);

  // Returns true if the offscreen context used by this surface has been
  // poisoned.
  bool HasBeenPoisoned() const;

 private:
  friend class base::RefCounted<CompositingIOSurfaceMac>;

  // Vertex structure for use in glDraw calls.
  struct SurfaceVertex {
    SurfaceVertex() : x_(0.0f), y_(0.0f), tx_(0.0f), ty_(0.0f) { }
    void set(float x, float y, float tx, float ty) {
      x_ = x;
      y_ = y;
      tx_ = tx;
      ty_ = ty;
    }
    void set_position(float x, float y) {
      x_ = x;
      y_ = y;
    }
    void set_texcoord(float tx, float ty) {
      tx_ = tx;
      ty_ = ty;
    }
    float x_;
    float y_;
    float tx_;
    float ty_;
  };

  // Counter-clockwise verts starting from upper-left corner (0, 0).
  struct SurfaceQuad {
    void set_size(gfx::Size vertex_size, gfx::Size texcoord_size) {
      // Texture coordinates are flipped vertically so they can be drawn on
      // a projection with a flipped y-axis (origin is top left).
      float vw = static_cast<float>(vertex_size.width());
      float vh = static_cast<float>(vertex_size.height());
      float tw = static_cast<float>(texcoord_size.width());
      float th = static_cast<float>(texcoord_size.height());
      verts_[0].set(0.0f, 0.0f, 0.0f, th);
      verts_[1].set(0.0f, vh, 0.0f, 0.0f);
      verts_[2].set(vw, vh, tw, 0.0f);
      verts_[3].set(vw, 0.0f, tw, th);
    }
    void set_rect(float x1, float y1, float x2, float y2) {
      verts_[0].set_position(x1, y1);
      verts_[1].set_position(x1, y2);
      verts_[2].set_position(x2, y2);
      verts_[3].set_position(x2, y1);
    }
    void set_texcoord_rect(float tx1, float ty1, float tx2, float ty2) {
      // Texture coordinates are flipped vertically so they can be drawn on
      // a projection with a flipped y-axis (origin is top left).
      verts_[0].set_texcoord(tx1, ty2);
      verts_[1].set_texcoord(tx1, ty1);
      verts_[2].set_texcoord(tx2, ty1);
      verts_[3].set_texcoord(tx2, ty2);
    }
    SurfaceVertex verts_[4];
  };

  // Keeps track of states and buffers for readback of IOSurface.
  //
  // TODO(miu): Major code refactoring is badly needed!  To be done in a
  // soon-upcoming change.  For now, we blatantly violate the style guide with
  // respect to struct vs. class usage:
  struct CopyContext {
    explicit CopyContext(const scoped_refptr<CompositingIOSurfaceContext>& ctx);
    ~CopyContext();

    // Delete any references to owned OpenGL objects.  This must be called
    // within the OpenGL context just before destruction.
    void ReleaseCachedGLObjects();

    // The following two methods assume |num_outputs| has been set, and are
    // being called within the OpenGL context.
    void PrepareReadbackFramebuffers();
    void PrepareForAsynchronousReadback();

    const scoped_ptr<CompositingIOSurfaceTransformer> transformer;
    GLenum output_readback_format;
    int num_outputs;
    GLuint output_textures[3];  // Not owned.
    // Note: For YUV, the |output_texture_sizes| widths are in terms of 4-byte
    // quads, not pixels.
    gfx::Size output_texture_sizes[3];
    GLuint frame_buffers[3];
    GLuint pixel_buffers[3];
    GLuint fence;  // When non-zero, doing an asynchronous copy.
    int cycles_elapsed;
    base::Callback<bool(const void*, int)> map_buffer_callback;
    base::Callback<void(bool)> done_callback;
  };

  CompositingIOSurfaceMac(
      const scoped_refptr<CompositingIOSurfaceContext>& context);
  ~CompositingIOSurfaceMac();

  // If this IOSurface has moved to a different window, use that window's
  // GL context (if multiple visible windows are using the same GL context
  // then call to setView call can stall and prevent reaching 60fps).
  void SwitchToContextOnNewWindow(NSView* view,
                                  int window_number);

  // Returns true if IOSurface is ready to render. False otherwise.
  bool MapIOSurfaceToTextureWithContextCurrent(
      const scoped_refptr<CompositingIOSurfaceContext>& current_context,
      const gfx::Size pixel_size,
      float scale_factor,
      IOSurfaceID io_surface_handle) WARN_UNUSED_RESULT;

  void UnrefIOSurfaceWithContextCurrent();

  void DrawQuad(const SurfaceQuad& quad);

  // Copy current frame to |target| video frame. This method must be called
  // within a CGL context. Returns a callback that should be called outside
  // of the CGL context.
  // If |called_within_draw| is true this method is called within a drawing
  // operations. This allow certain optimizations.
  base::Closure CopyToVideoFrameWithinContext(
      const gfx::Rect& src_subrect,
      bool called_within_draw,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(bool)>& callback);

  // Common GPU-readback copy path.  Only one of |bitmap_output| or
  // |video_frame_output| may be specified: Either ARGB is written to
  // |bitmap_output| or letter-boxed YV12 is written to |video_frame_output|.
  base::Closure CopyToSelectedOutputWithinContext(
      const gfx::Rect& src_pixel_subrect,
      const gfx::Rect& dst_pixel_rect,
      bool called_within_draw,
      const SkBitmap* bitmap_output,
      const scoped_refptr<media::VideoFrame>& video_frame_output,
      const base::Callback<void(bool)>& done_callback);

  // TODO(hclam): These two methods should be static.
  void AsynchronousReadbackForCopy(
      const gfx::Rect& dst_pixel_rect,
      bool called_within_draw,
      CopyContext* copy_context,
      const SkBitmap* bitmap_output,
      const scoped_refptr<media::VideoFrame>& video_frame_output);
  bool SynchronousReadbackForCopy(
      const gfx::Rect& dst_pixel_rect,
      CopyContext* copy_context,
      const SkBitmap* bitmap_output,
      const scoped_refptr<media::VideoFrame>& video_frame_output);

  void CheckIfAllCopiesAreFinishedWithinContext(
      bool block_until_finished,
      std::vector<base::Closure>* done_callbacks);

  void FailAllCopies();
  void DestroyAllCopyContextsWithinContext();

  // Check for GL errors and store the result in error_. Only return new
  // errors
  GLenum GetAndSaveGLError();

  gfx::Rect IntersectWithIOSurface(const gfx::Rect& rect) const;

  // Offscreen context used for all operations other than drawing to the
  // screen. This is in the same share group as the contexts used for
  // drawing, and is the same for all IOSurfaces in all windows.
  scoped_refptr<CompositingIOSurfaceContext> offscreen_context_;

  // IOSurface data.
  IOSurfaceID io_surface_handle_;
  base::ScopedCFTypeRef<IOSurfaceRef> io_surface_;

  // The width and height of the io surface.
  gfx::Size pixel_io_surface_size_;  // In pixels.
  gfx::Size dip_io_surface_size_;  // In view / density independent pixels.
  float scale_factor_;

  // The "live" OpenGL texture referring to this IOSurfaceRef. Note
  // that per the CGLTexImageIOSurface2D API we do not need to
  // explicitly update this texture's contents once created. All we
  // need to do is ensure it is re-bound before attempting to draw
  // with it.
  GLuint texture_;

  // A pool of CopyContexts with OpenGL objects ready for re-use.  Prefer to
  // pull one from the pool before creating a new CopyContext.
  std::vector<CopyContext*> copy_context_pool_;

  // CopyContexts being used for in-flight copy operations.
  std::deque<CopyContext*> copy_requests_;

  // Timer for finishing a copy operation.
  base::Timer finish_copy_timer_;

  // Error saved by GetAndSaveGLError
  GLint gl_error_;

  // Aggressive IOSurface eviction logic. When using CoreAnimation, IOSurfaces
  // are used only transiently to transfer from the GPU process to the browser
  // process. Once the IOSurface has been drawn to its CALayer, the CALayer
  // will not need updating again until its view is hidden and re-shown.
  // Aggressively evict surfaces when more than 8 (the number allowed by the
  // memory manager for fast tab switching) are allocated.
  enum {
    kMaximumUnevictedSurfaces = 8,
  };
  typedef std::list<CompositingIOSurfaceMac*> EvictionQueue;
  void EvictionMarkUpdated();
  void EvictionMarkEvicted();
  EvictionQueue::iterator eviction_queue_iterator_;
  bool eviction_has_been_drawn_since_updated_;

  static void EvictionScheduleDoEvict();
  static void EvictionDoEvict();
  static base::LazyInstance<EvictionQueue> eviction_queue_;
  static bool eviction_scheduled_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_COMPOSITING_IOSURFACE_MAC_H_
