// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Given a starting render_process_id and main_render_frame_id, the
// WebContentsTracker tracks changes to the active RenderFrameHost tree during
// the lifetime of a WebContents instance.  This is used when mirroring tab
// video and audio so that user navigations, crashes, iframes, etc., during a
// tab's lifetime allow the capturing code to remain active on the
// current/latest render frame tree.
//
// Threading issues: Start(), Stop() and the ChangeCallback are invoked on the
// same thread.  This can be any thread, and the decision is locked-in by
// WebContentsTracker when Start() is called.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_WEB_CONTENTS_TRACKER_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_WEB_CONTENTS_TRACKER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace content {

class RenderWidgetHost;

class CONTENT_EXPORT WebContentsTracker
    : public base::RefCountedThreadSafe<WebContentsTracker>,
      public WebContentsObserver {
 public:
  // If |track_fullscreen_rwh| is true, the ChangeCallback will be run when a
  // WebContents shows/destroys a fullscreen RenderWidgetHost view.  If false,
  // fullscreen events are ignored.  Specify true for video tab capture and
  // false for audio tab capture.
  explicit WebContentsTracker(bool track_fullscreen_rwh);

  // Callback to indicate a new RenderWidgetHost should be targeted for capture.
  // This is also invoked with false to indicate tracking will not continue
  // (i.e., the WebContents instance was not found or has been destroyed).
  typedef base::Callback<void(bool was_still_tracking)> ChangeCallback;

  // Start tracking.  The last-known |render_process_id| and
  // |main_render_frame_id| are provided, and |callback| will be run once to
  // indicate whether tracking successfully started (this may occur during the
  // invocation of Start(), or in the future).  The callback will be invoked on
  // the same thread calling Start().
  virtual void Start(int render_process_id, int main_render_frame_id,
                     const ChangeCallback& callback);

  // Stop tracking.  Once this method returns, the callback is guaranteed not to
  // be invoked again.
  virtual void Stop();

  // Current target.  This must only be called on the UI BrowserThread.
  RenderWidgetHost* GetTargetRenderWidgetHost() const;

  // Set a callback that is run whenever the main frame of the WebContents is
  // resized.  This method must be called on the same thread that calls
  // Start()/Stop(), and |callback| will be run on that same thread.  Calling
  // the Stop() method guarantees the callback will never be invoked again.
  void SetResizeChangeCallback(const base::Closure& callback);

 protected:
  friend class base::RefCountedThreadSafe<WebContentsTracker>;
  ~WebContentsTracker() override;

 private:
  // Determine the target RenderWidgetHost and, if different from that last
  // reported, runs the ChangeCallback on the appropriate thread.  If
  // |force_callback_run| is true, the ChangeCallback is run even if the
  // RenderWidgetHost has not changed.
  void OnPossibleTargetChange(bool force_callback_run);

  // Called on the thread that Start()/Stop() are called on.  Checks whether the
  // callback is still valid and, if so, runs it.
  void MaybeDoCallback(bool was_still_tracking);

  // Called on the thread that Start()/Stop() are called on.  Checks whether the
  // callback is still valid and, if so, runs it to indicate the main frame has
  // changed in size.
  void MaybeDoResizeCallback();

  // Look-up the current WebContents instance associated with the given
  // |render_process_id| and |main_render_frame_id| and begin observing it.
  void StartObservingWebContents(int render_process_id,
                                 int main_render_frame_id);

  // WebContentsObserver overrides: According to web_contents_observer.h, these
  // two method overrides are all that is necessary to track the set of active
  // RenderFrameHosts.
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;
  void RenderFrameHostChanged(RenderFrameHost* old_host,
                              RenderFrameHost* new_host) override;

  // WebContentsObserver override to notify the client that the source size has
  // changed.
  void MainFrameWasResized(bool width_changed) override;

  // WebContentsObserver override to notify the client that the capture target
  // has been permanently lost.
  void WebContentsDestroyed() override;

  // WebContentsObserver overrides to notify the client that the capture target
  // may have changed due to a separate fullscreen widget shown/destroyed.
  void DidShowFullscreenWidget() override;
  void DidDestroyFullscreenWidget() override;

  // If true, the client is interested in the showing/destruction of fullscreen
  // RenderWidgetHosts.
  const bool track_fullscreen_rwh_;

  // TaskRunner corresponding to the thread that called Start().
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Callback to run when the target RenderWidgetHost has changed.
  ChangeCallback callback_;

  // Pointer to the RenderWidgetHost provided in the last run of |callback_|.
  // This is used to eliminate duplicate callback runs.
  RenderWidgetHost* last_target_;

  // Callback to run when the target RenderWidgetHost has resized.
  base::Closure resize_callback_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsTracker);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_WEB_CONTENTS_TRACKER_H_
