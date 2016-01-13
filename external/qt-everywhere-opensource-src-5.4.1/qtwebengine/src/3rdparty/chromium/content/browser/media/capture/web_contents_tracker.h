// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Given a starting render_process_id and render_view_id, the WebContentsTracker
// tracks RenderViewHost instance swapping during the lifetime of a WebContents
// instance.  This is used when mirroring tab video and audio so that user
// navigations, crashes, etc., during a tab's lifetime allow the capturing code
// to remain active on the current/latest RenderView.
//
// Threading issues: Start(), Stop() and the ChangeCallback are invoked on the
// same thread.  This can be any thread, and the decision is locked-in by
// WebContentsTracker when Start() is called.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_WEB_CONTENTS_TRACKER_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_WEB_CONTENTS_TRACKER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"

namespace base {
class MessageLoopProxy;
}

namespace content {

class CONTENT_EXPORT WebContentsTracker
    : public base::RefCountedThreadSafe<WebContentsTracker>,
      public WebContentsObserver {
 public:
  WebContentsTracker();

  // Callback for whenever the target is swapped.  The callback is also invoked
  // with both arguments set to MSG_ROUTING_NONE to indicate tracking will not
  // continue (i.e., the WebContents instance was not found or has been
  // destroyed).
  typedef base::Callback<void(int render_process_id, int render_view_id)>
      ChangeCallback;

  // Start tracking.  The last-known |render_process_id| and |render_view_id|
  // are provided, and the given callback is invoked asynchronously one or more
  // times.  The callback will be invoked on the same thread calling Start().
  virtual void Start(int render_process_id, int render_view_id,
                     const ChangeCallback& callback);

  // Stop tracking.  Once this method returns, the callback is guaranteed not to
  // be invoked again.
  virtual void Stop();

 protected:
  friend class base::RefCountedThreadSafe<WebContentsTracker>;
  virtual ~WebContentsTracker();

 private:
  // Reads the render_process_id/render_view_id from the current WebContents
  // instance and then invokes the callback.
  void OnWebContentsChangeEvent();

  // Called on the thread that Start()/Stop() are called on, check whether the
  // callback is still valid and, if so, invoke it.
  void MaybeDoCallback(int render_process_id, int render_view_id);

  // Look-up the current WebContents instance associated with the given
  // |render_process_id| and |render_view_id| and begin observing it.
  void LookUpAndObserveWebContents(int render_process_id,
                                   int render_view_id);

  // WebContentsObserver overrides to react to events of interest.
  virtual void RenderViewReady() OVERRIDE;
  virtual void AboutToNavigateRenderView(RenderViewHost* render_view_host)
      OVERRIDE;
  virtual void DidNavigateMainFrame(const LoadCommittedDetails& details,
                                    const FrameNavigateParams& params) OVERRIDE;
  virtual void WebContentsDestroyed() OVERRIDE;

  scoped_refptr<base::MessageLoopProxy> message_loop_;
  ChangeCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsTracker);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_WEB_CONTENTS_TRACKER_H_
