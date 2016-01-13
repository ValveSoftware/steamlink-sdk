// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_DELEGATED_FRAME_HOST_H_
#define CONTENT_BROWSER_COMPOSITOR_DELEGATED_FRAME_HOST_H_

#include "cc/layers/delegated_frame_provider.h"
#include "cc/layers/delegated_frame_resource_collection.h"
#include "cc/output/copy_output_result.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "content/browser/compositor/owned_mailbox.h"
#include "content/browser/renderer_host/delegated_frame_evictor.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/render_process_host.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/compositor_vsync_manager.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_owner_delegate.h"
#include "ui/gfx/rect_conversions.h"

namespace media {
class VideoFrame;
}

namespace content {

class DelegatedFrameHost;
class ReadbackYUVInterface;
class RenderWidgetHostViewFrameSubscriber;
class RenderWidgetHostImpl;
class ResizeLock;

// The DelegatedFrameHostClient is the interface from the DelegatedFrameHost,
// which manages delegated frames, and the ui::Compositor being used to
// display them.
class CONTENT_EXPORT DelegatedFrameHostClient {
 public:
  virtual ui::Compositor* GetCompositor() const = 0;
  virtual ui::Layer* GetLayer() = 0;
  virtual RenderWidgetHostImpl* GetHost() = 0;
  virtual void SchedulePaintInRect(const gfx::Rect& damage_rect_in_dip) = 0;
  virtual bool IsVisible() = 0;
  virtual scoped_ptr<ResizeLock> CreateResizeLock(
      bool defer_compositor_lock) = 0;
  virtual gfx::Size DesiredFrameSize() = 0;

  // TODO(ccameron): It is likely that at least one of these two functions is
  // redundant. Find which one, and delete it.
  virtual float CurrentDeviceScaleFactor() = 0;
  virtual gfx::Size ConvertViewSizeToPixel(const gfx::Size& size) = 0;

  // These are to be overridden for testing only.
  // TODO(ccameron): This is convoluted. Make the tests that need to override
  // these functions test DelegatedFrameHost directly (rather than do it
  // through RenderWidgetHostViewAura).
  virtual DelegatedFrameHost* GetDelegatedFrameHost() const = 0;
  virtual bool ShouldCreateResizeLock();
  virtual void RequestCopyOfOutput(scoped_ptr<cc::CopyOutputRequest> request);
};

// The DelegatedFrameHost is used to host all of the RenderWidgetHostView state
// and functionality that is associated with delegated frames being sent from
// the RenderWidget. The DelegatedFrameHost will push these changes through to
// the ui::Compositor associated with its DelegatedFrameHostClient.
class CONTENT_EXPORT DelegatedFrameHost
    : public ui::CompositorObserver,
      public ui::CompositorVSyncManager::Observer,
      public ui::LayerOwnerDelegate,
      public ImageTransportFactoryObserver,
      public DelegatedFrameEvictorClient,
      public cc::DelegatedFrameResourceCollectionClient,
      public base::SupportsWeakPtr<DelegatedFrameHost> {
 public:
  DelegatedFrameHost(DelegatedFrameHostClient* client);
  virtual ~DelegatedFrameHost();

  bool CanCopyToBitmap() const;

  // Public interface exposed to RenderWidgetHostView.
  void SwapDelegatedFrame(
      uint32 output_surface_id,
      scoped_ptr<cc::DelegatedFrameData> frame_data,
      float frame_device_scale_factor,
      const std::vector<ui::LatencyInfo>& latency_info);
  void WasHidden();
  void WasShown();
  void WasResized();
  gfx::Size GetRequestedRendererSize() const;
  void AddedToWindow();
  void RemovingFromWindow();
  void CopyFromCompositingSurface(
      const gfx::Rect& src_subrect,
      const gfx::Size& dst_size,
      const base::Callback<void(bool, const SkBitmap&)>& callback,
      const SkBitmap::Config config);
  void CopyFromCompositingSurfaceToVideoFrame(
      const gfx::Rect& src_subrect,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(bool)>& callback);
  bool CanCopyToVideoFrame() const;
  bool CanSubscribeFrame() const;
  void BeginFrameSubscription(
      scoped_ptr<RenderWidgetHostViewFrameSubscriber> subscriber);
  void EndFrameSubscription();

  // Exposed for tests.
  cc::DelegatedFrameProvider* FrameProviderForTesting() const {
    return frame_provider_.get();
  }
  void OnCompositingDidCommitForTesting(ui::Compositor* compositor) {
    OnCompositingDidCommit(compositor);
  }
  bool ShouldCreateResizeLockForTesting() { return ShouldCreateResizeLock(); }

 private:
  friend class DelegatedFrameHostClient;
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           SkippedDelegatedFrames);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           DiscardDelegatedFramesWithLocking);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraCopyRequestTest,
                           DestroyedAfterCopyRequest);

  RenderWidgetHostViewFrameSubscriber* frame_subscriber() const {
    return frame_subscriber_.get();
  }
  bool ShouldCreateResizeLock();
  void RequestCopyOfOutput(scoped_ptr<cc::CopyOutputRequest> request);

  void LockResources();
  void UnlockResources();

  // Overridden from ui::CompositorObserver:
  virtual void OnCompositingDidCommit(ui::Compositor* compositor) OVERRIDE;
  virtual void OnCompositingStarted(ui::Compositor* compositor,
                                    base::TimeTicks start_time) OVERRIDE;
  virtual void OnCompositingEnded(ui::Compositor* compositor) OVERRIDE;
  virtual void OnCompositingAborted(ui::Compositor* compositor) OVERRIDE;
  virtual void OnCompositingLockStateChanged(
      ui::Compositor* compositor) OVERRIDE;

  // Overridden from ui::CompositorVSyncManager::Observer:
  virtual void OnUpdateVSyncParameters(base::TimeTicks timebase,
                                       base::TimeDelta interval) OVERRIDE;

  // Overridden from ui::LayerOwnerObserver:
  virtual void OnLayerRecreated(ui::Layer* old_layer,
                                ui::Layer* new_layer) OVERRIDE;

  // Overridden from ImageTransportFactoryObserver:
  virtual void OnLostResources() OVERRIDE;

  bool ShouldSkipFrame(gfx::Size size_in_dip) const;

  // Lazily grab a resize lock if the aura window size doesn't match the current
  // frame size, to give time to the renderer.
  void MaybeCreateResizeLock();

  // Checks if the resize lock can be released because we received an new frame.
  void CheckResizeLock();

  // Run all on compositing commit callbacks.
  void RunOnCommitCallbacks();

  // Add on compositing commit callback.
  void AddOnCommitCallbackAndDisableLocks(const base::Closure& callback);

  // Called after async thumbnailer task completes.  Scales and crops the result
  // of the copy.
  static void CopyFromCompositingSurfaceHasResult(
      const gfx::Size& dst_size_in_pixel,
      const SkBitmap::Config config,
      const base::Callback<void(bool, const SkBitmap&)>& callback,
      scoped_ptr<cc::CopyOutputResult> result);
  static void PrepareTextureCopyOutputResult(
      const gfx::Size& dst_size_in_pixel,
      const SkBitmap::Config config,
      const base::Callback<void(bool, const SkBitmap&)>& callback,
      scoped_ptr<cc::CopyOutputResult> result);
  static void PrepareBitmapCopyOutputResult(
      const gfx::Size& dst_size_in_pixel,
      const SkBitmap::Config config,
      const base::Callback<void(bool, const SkBitmap&)>& callback,
      scoped_ptr<cc::CopyOutputResult> result);
  static void CopyFromCompositingSurfaceHasResultForVideo(
      base::WeakPtr<DelegatedFrameHost> rwhva,
      scoped_refptr<OwnedMailbox> subscriber_texture,
      scoped_refptr<media::VideoFrame> video_frame,
      const base::Callback<void(bool)>& callback,
      scoped_ptr<cc::CopyOutputResult> result);
  static void CopyFromCompositingSurfaceFinishedForVideo(
      base::WeakPtr<DelegatedFrameHost> rwhva,
      const base::Callback<void(bool)>& callback,
      scoped_refptr<OwnedMailbox> subscriber_texture,
      scoped_ptr<cc::SingleReleaseCallback> release_callback,
      bool result);
  static void ReturnSubscriberTexture(
      base::WeakPtr<DelegatedFrameHost> rwhva,
      scoped_refptr<OwnedMailbox> subscriber_texture,
      uint32 sync_point);

  void SendDelegatedFrameAck(uint32 output_surface_id);
  void SendReturnedDelegatedResources(uint32 output_surface_id);

  // DelegatedFrameEvictorClient implementation.
  virtual void EvictDelegatedFrame() OVERRIDE;

  // cc::DelegatedFrameProviderClient implementation.
  virtual void UnusedResourcesAreAvailable() OVERRIDE;

  void DidReceiveFrameFromRenderer();

  DelegatedFrameHostClient* client_;

  std::vector<base::Closure> on_compositing_did_commit_callbacks_;

  // The vsync manager we are observing for changes, if any.
  scoped_refptr<ui::CompositorVSyncManager> vsync_manager_;

  // With delegated renderer, this is the last output surface, used to
  // disambiguate resources with the same id coming from different output
  // surfaces.
  uint32 last_output_surface_id_;

  // The number of delegated frame acks that are pending, to delay resource
  // returns until the acks are sent.
  int pending_delegated_ack_count_;

  // True after a delegated frame has been skipped, until a frame is not
  // skipped.
  bool skipped_frames_;
  std::vector<ui::LatencyInfo> skipped_latency_info_list_;

  // Holds delegated resources that have been given to a DelegatedFrameProvider,
  // and gives back resources when they are no longer in use for return to the
  // renderer.
  scoped_refptr<cc::DelegatedFrameResourceCollection> resource_collection_;

  // Provides delegated frame updates to the cc::DelegatedRendererLayer.
  scoped_refptr<cc::DelegatedFrameProvider> frame_provider_;

  // This lock is the one waiting for a frame of the right size to come back
  // from the renderer/GPU process. It is set from the moment the aura window
  // got resized, to the moment we committed the renderer frame of the same
  // size. It keeps track of the size we expect from the renderer, and locks the
  // compositor, as well as the UI for a short time to give a chance to the
  // renderer of producing a frame of the right size.
  scoped_ptr<ResizeLock> resize_lock_;

  // Keeps track of the current frame size.
  gfx::Size current_frame_size_in_dip_;

  // This lock is for waiting for a front surface to become available to draw.
  scoped_refptr<ui::CompositorLock> released_front_lock_;

  enum CanLockCompositorState {
    YES_CAN_LOCK,
    // We locked, so at some point we'll need to kick a frame.
    YES_DID_LOCK,
    // No. A lock timed out, we need to kick a new frame before locking again.
    NO_PENDING_RENDERER_FRAME,
    // No. We've got a frame, but it hasn't been committed.
    NO_PENDING_COMMIT,
  };
  CanLockCompositorState can_lock_compositor_;

  base::TimeTicks last_draw_ended_;

  // Subscriber that listens to frame presentation events.
  scoped_ptr<RenderWidgetHostViewFrameSubscriber> frame_subscriber_;
  std::vector<scoped_refptr<OwnedMailbox> > idle_frame_subscriber_textures_;

  // YUV readback pipeline.
  scoped_ptr<content::ReadbackYUVInterface>
      yuv_readback_pipeline_;

  scoped_ptr<DelegatedFrameEvictor> delegated_frame_evictor_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_DELEGATED_FRAME_HOST_H_
