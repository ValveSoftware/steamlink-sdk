// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_COMPOSITOR_H_
#define UI_COMPOSITOR_COMPOSITOR_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "cc/output/begin_frame_args.h"
#include "cc/surfaces/surface_sequence.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_single_thread_client.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/compositor_animation_observer.h"
#include "ui/compositor/compositor_export.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/layer_animator_collection.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/native_widget_types.h"

namespace base {
class RunLoop;
class SingleThreadTaskRunner;
}

namespace cc {
class AnimationTimeline;
class ContextProvider;
class Layer;
class LayerTreeDebugState;
class LayerTreeHost;
class RendererSettings;
class SharedBitmapManager;
class SurfaceIdAllocator;
class SurfaceManager;
class TaskGraphRunner;
}

namespace gfx {
class Rect;
class Size;
}

namespace gpu {
class GpuMemoryBufferManager;
struct Mailbox;
}

namespace ui {

class Compositor;
class CompositorVSyncManager;
class LatencyInfo;
class Layer;
class Reflector;
class Texture;

const int kCompositorLockTimeoutMs = 67;

class COMPOSITOR_EXPORT ContextFactoryObserver {
 public:
  virtual ~ContextFactoryObserver() {}

  // Notifies that the ContextProvider returned from
  // ui::ContextFactory::SharedMainThreadContextProvider was lost.  When this
  // is called, the old resources (e.g. shared context, GL helper) still
  // exist, but are about to be destroyed. Getting a reference to those
  // resources from the ContextFactory (e.g. through
  // SharedMainThreadContextProvider()) will return newly recreated, valid
  // resources.
  virtual void OnLostResources() = 0;
};

// This class abstracts the creation of the 3D context for the compositor. It is
// a global object.
class COMPOSITOR_EXPORT ContextFactory {
 public:
  virtual ~ContextFactory() {}

  // Creates an output surface for the given compositor. The factory may keep
  // per-compositor data (e.g. a shared context), that needs to be cleaned up
  // by calling RemoveCompositor when the compositor gets destroyed.
  virtual void CreateOutputSurface(base::WeakPtr<Compositor> compositor) = 0;

  // Creates a reflector that copies the content of the |mirrored_compositor|
  // onto |mirroring_layer|.
  virtual std::unique_ptr<Reflector> CreateReflector(
      Compositor* mirrored_compositor,
      Layer* mirroring_layer) = 0;
  // Removes the reflector, which stops the mirroring.
  virtual void RemoveReflector(Reflector* reflector) = 0;

  // Return a reference to a shared offscreen context provider usable from the
  // main thread.
  virtual scoped_refptr<cc::ContextProvider>
      SharedMainThreadContextProvider() = 0;

  // Destroys per-compositor data.
  virtual void RemoveCompositor(Compositor* compositor) = 0;

  // When true, the factory uses test contexts that do not do real GL
  // operations.
  virtual bool DoesCreateTestContexts() = 0;

  // Returns the OpenGL target to use for image textures.
  virtual uint32_t GetImageTextureTarget(gfx::BufferFormat format,
                                         gfx::BufferUsage usage) = 0;

  // Gets the shared bitmap manager for software mode.
  virtual cc::SharedBitmapManager* GetSharedBitmapManager() = 0;

  // Gets the GPU memory buffer manager.
  virtual gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() = 0;

  // Gets the task graph runner.
  virtual cc::TaskGraphRunner* GetTaskGraphRunner() = 0;

  // Creates a Surface ID allocator with a new namespace.
  virtual std::unique_ptr<cc::SurfaceIdAllocator>
  CreateSurfaceIdAllocator() = 0;

  // Gets the surface manager.
  virtual cc::SurfaceManager* GetSurfaceManager() = 0;

  // Resize the display corresponding to this compositor to a particular size.
  virtual void ResizeDisplay(ui::Compositor* compositor,
                             const gfx::Size& size) = 0;

  // Set the output color profile into which this compositor should render.
  virtual void SetDisplayColorSpace(ui::Compositor* compositor,
                                    const gfx::ColorSpace& color_space) = 0;

  virtual void SetAuthoritativeVSyncInterval(ui::Compositor* compositor,
                                             base::TimeDelta interval) = 0;

  virtual void SetOutputIsSecure(Compositor* compositor, bool secure) = 0;

  virtual void AddObserver(ContextFactoryObserver* observer) = 0;

  virtual void RemoveObserver(ContextFactoryObserver* observer) = 0;
};

// This class represents a lock on the compositor, that can be used to prevent
// commits to the compositor tree while we're waiting for an asynchronous
// event. The typical use case is when waiting for a renderer to produce a frame
// at the right size. The caller keeps a reference on this object, and drops the
// reference once it desires to release the lock.
// By default, the lock will be cancelled after a short timeout to ensure
// responsiveness of the UI, so the compositor tree should be kept in a
// "reasonable" state while the lock is held. If the compositor sets
// locks to not time out, then the lock will remain in effect until destroyed.
// Don't instantiate this class directly, use Compositor::GetCompositorLock.
class COMPOSITOR_EXPORT CompositorLock
    : public base::RefCounted<CompositorLock>,
      public base::SupportsWeakPtr<CompositorLock> {
 private:
  friend class base::RefCounted<CompositorLock>;
  friend class Compositor;

  explicit CompositorLock(Compositor* compositor);
  ~CompositorLock();

  void CancelLock();

  Compositor* compositor_;
  DISALLOW_COPY_AND_ASSIGN(CompositorLock);
};

// Compositor object to take care of GPU painting.
// A Browser compositor object is responsible for generating the final
// displayable form of pixels comprising a single widget's contents. It draws an
// appropriately transformed texture for each transformed view in the widget's
// view hierarchy.
class COMPOSITOR_EXPORT Compositor
    : NON_EXPORTED_BASE(public cc::LayerTreeHostClient),
      NON_EXPORTED_BASE(public cc::LayerTreeHostSingleThreadClient) {
 public:
  Compositor(ui::ContextFactory* context_factory,
             scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~Compositor() override;

  ui::ContextFactory* context_factory() { return context_factory_; }

  void SetOutputSurface(std::unique_ptr<cc::OutputSurface> surface);

  // Schedules a redraw of the layer tree associated with this compositor.
  void ScheduleDraw();

  // Sets the root of the layer tree drawn by this Compositor. The root layer
  // must have no parent. The compositor's root layer is reset if the root layer
  // is destroyed. NULL can be passed to reset the root layer, in which case the
  // compositor will stop drawing anything.
  // The Compositor does not own the root layer.
  const Layer* root_layer() const { return root_layer_; }
  Layer* root_layer() { return root_layer_; }
  void SetRootLayer(Layer* root_layer);

  cc::AnimationTimeline* GetAnimationTimeline() const;

  // Called when we need the compositor to preserve the alpha channel in the
  // output for situations when we want to render transparently atop something
  // else, e.g. Aero glass.
  void SetHostHasTransparentBackground(bool host_has_transparent_background);

  // The scale factor of the device that this compositor is
  // compositing layers on.
  float device_scale_factor() const { return device_scale_factor_; }

  // Where possible, draws are scissored to a damage region calculated from
  // changes to layer properties.  This bypasses that and indicates that
  // the whole frame needs to be drawn.
  void ScheduleFullRedraw();

  // Schedule redraw and append damage_rect to the damage region calculated
  // from changes to layer properties.
  void ScheduleRedrawRect(const gfx::Rect& damage_rect);

  // Finishes all outstanding rendering and disables swapping on this surface.
  void FinishAllRendering();

  // Finishes all outstanding rendering and disables swapping on this surface
  // until it is resized.
  void DisableSwapUntilResize();

  void SetLatencyInfo(const LatencyInfo& latency_info);

  // Sets the compositor's device scale factor and size.
  void SetScaleAndSize(float scale, const gfx::Size& size_in_pixel);

  // Set the output color profile into which this compositor should render.
  void SetDisplayColorSpace(const gfx::ColorSpace& color_space);

  // Returns the size of the widget that is being drawn to in pixel coordinates.
  const gfx::Size& size() const { return size_; }

  // Sets the background color used for areas that aren't covered by
  // the |root_layer|.
  void SetBackgroundColor(SkColor color);

  // Sets the visibility of the underlying compositor.
  void SetVisible(bool visible);

  // Gets the visibility of the underlying compositor.
  bool IsVisible();

  // The "authoritative" vsync interval, if provided, will override interval
  // reported from 3D context. This is typically the value reported by a more
  // reliable source, e.g, the platform display configuration.
  // In the particular case of ChromeOS -- this is the value queried through
  // XRandR, which is more reliable than the value queried through the 3D
  // context.
  void SetAuthoritativeVSyncInterval(const base::TimeDelta& interval);

  // Sets the widget for the compositor to render into.
  void SetAcceleratedWidget(gfx::AcceleratedWidget widget);
  // Releases the widget previously set through SetAcceleratedWidget().
  // After returning it will not be used for rendering anymore.
  // The compositor must be set to invisible when taking away a widget.
  gfx::AcceleratedWidget ReleaseAcceleratedWidget();
  gfx::AcceleratedWidget widget() const;

  // Returns the vsync manager for this compositor.
  scoped_refptr<CompositorVSyncManager> vsync_manager() const;

  // Returns the main thread task runner this compositor uses. Users of the
  // compositor generally shouldn't use this.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const {
    return task_runner_;
  }

  // Compositor does not own observers. It is the responsibility of the
  // observer to remove itself when it is done observing.
  void AddObserver(CompositorObserver* observer);
  void RemoveObserver(CompositorObserver* observer);
  bool HasObserver(const CompositorObserver* observer) const;

  void AddAnimationObserver(CompositorAnimationObserver* observer);
  void RemoveAnimationObserver(CompositorAnimationObserver* observer);
  bool HasAnimationObserver(const CompositorAnimationObserver* observer) const;

  // Change the timeout behavior for all future locks that are created. Locks
  // should time out if there is an expectation that the compositor will be
  // responsive.
  void SetLocksWillTimeOut(bool locks_will_time_out) {
    locks_will_time_out_ = locks_will_time_out;
  }

  // Creates a compositor lock. Returns NULL if it is not possible to lock at
  // this time (i.e. we're waiting to complete a previous unlock).
  scoped_refptr<CompositorLock> GetCompositorLock();

  // Internal functions, called back by command-buffer contexts on swap buffer
  // events.

  // Signals swap has been posted.
  void OnSwapBuffersPosted();

  // Signals swap has completed.
  void OnSwapBuffersComplete();

  // Signals swap has aborted (e.g. lost context).
  void OnSwapBuffersAborted();

  // LayerTreeHostClient implementation.
  void WillBeginMainFrame() override {}
  void DidBeginMainFrame() override {}
  void BeginMainFrame(const cc::BeginFrameArgs& args) override;
  void BeginMainFrameNotExpectedSoon() override;
  void UpdateLayerTreeHost() override;
  void ApplyViewportDeltas(const gfx::Vector2dF& inner_delta,
                           const gfx::Vector2dF& outer_delta,
                           const gfx::Vector2dF& elastic_overscroll_delta,
                           float page_scale,
                           float top_controls_delta) override {}
  void RequestNewOutputSurface() override;
  void DidInitializeOutputSurface() override;
  void DidFailToInitializeOutputSurface() override;
  void WillCommit() override {}
  void DidCommit() override;
  void DidCommitAndDrawFrame() override;
  void DidCompleteSwapBuffers() override;
  void DidCompletePageScaleAnimation() override {}

  // cc::LayerTreeHostSingleThreadClient implementation.
  void DidPostSwapBuffers() override;
  void DidAbortSwapBuffers() override;

  bool IsLocked() { return compositor_lock_ != NULL; }

  void SetOutputIsSecure(bool output_is_secure);

  const cc::LayerTreeDebugState& GetLayerTreeDebugState() const;
  void SetLayerTreeDebugState(const cc::LayerTreeDebugState& debug_state);
  const cc::RendererSettings& GetRendererSettings() const;

  LayerAnimatorCollection* layer_animator_collection() {
    return &layer_animator_collection_;
  }

  cc::SurfaceIdAllocator* surface_id_allocator() {
    return surface_id_allocator_.get();
  }

 private:
  friend class base::RefCounted<Compositor>;
  friend class CompositorLock;

  // Called by CompositorLock.
  void UnlockCompositor();

  // Called to release any pending CompositorLock
  void CancelCompositorLock();

  gfx::Size size_;

  ui::ContextFactory* context_factory_;

  // The root of the Layer tree drawn by this compositor.
  Layer* root_layer_;

  base::ObserverList<CompositorObserver, true> observer_list_;
  base::ObserverList<CompositorAnimationObserver> animation_observer_list_;

  gfx::AcceleratedWidget widget_;
  bool widget_valid_;
  bool output_surface_requested_;
  std::unique_ptr<cc::SurfaceIdAllocator> surface_id_allocator_;
  scoped_refptr<cc::Layer> root_web_layer_;
  std::unique_ptr<cc::LayerTreeHost> host_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // The manager of vsync parameters for this compositor.
  scoped_refptr<CompositorVSyncManager> vsync_manager_;

  // The device scale factor of the monitor that this compositor is compositing
  // layers on.
  float device_scale_factor_;

  bool locks_will_time_out_;
  CompositorLock* compositor_lock_;

  LayerAnimatorCollection layer_animator_collection_;
  scoped_refptr<cc::AnimationTimeline> animation_timeline_;

  base::WeakPtrFactory<Compositor> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Compositor);
};

}  // namespace ui

#endif  // UI_COMPOSITOR_COMPOSITOR_H_
