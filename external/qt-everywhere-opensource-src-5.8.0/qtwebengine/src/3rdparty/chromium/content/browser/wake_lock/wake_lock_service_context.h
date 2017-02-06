// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WAKE_LOCK_WAKE_LOCK_SERVICE_CONTEXT_H_
#define CONTENT_BROWSER_WAKE_LOCK_WAKE_LOCK_SERVICE_CONTEXT_H_

#include <memory>
#include <set>
#include <utility>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/wake_lock/wake_lock_service_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/interface_request.h"

#if defined(OS_ANDROID)
#include "ui/android/view_android.h"
#endif  // OS_ANDROID

namespace device {
class PowerSaveBlocker;
}  // namespace device

namespace content {

class RenderFrameHost;
class WebContents;

class CONTENT_EXPORT WakeLockServiceContext : public WebContentsObserver {
 public:
  explicit WakeLockServiceContext(WebContents* web_contents);
  ~WakeLockServiceContext() override;

  // Creates a WakeLockServiceImpl that is strongly bound to |request|.
  void CreateService(
      int render_process_id,
      int render_frame_id,
      mojo::InterfaceRequest<blink::mojom::WakeLockService> request);

  // WebContentsObserver implementation.
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;
  void WebContentsDestroyed() override;

  // Requests wake lock for RenderFrame identified by |render_process_id| and
  // |render_frame_id|.
  void RequestWakeLock(int render_process_id, int render_frame_id);

  // Cancels wake lock request for RenderFrame identified by
  // |render_process_id| and |render_frame_id|.
  void CancelWakeLock(int render_process_id, int render_frame_id);

  // Used by tests.
  bool HasWakeLockForTests() const;

 private:
  void CreateWakeLock();
  void RemoveWakeLock();
  void UpdateWakeLock();

  // Set of (render_process_id, render_frame_id) pairs identifying all
  // RenderFrames requesting wake lock.
  std::set<std::pair<int, int>> frames_requesting_lock_;

  // The actual power save blocker for screen.
  std::unique_ptr<device::PowerSaveBlocker> wake_lock_;
#if defined(OS_ANDROID)
  std::unique_ptr<base::WeakPtrFactory<ui::ViewAndroid>> view_weak_factory_;
#endif

  base::WeakPtrFactory<WakeLockServiceContext> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WakeLockServiceContext);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WAKE_LOCK_WAKE_LOCK_SERVICE_CONTEXT_H_
