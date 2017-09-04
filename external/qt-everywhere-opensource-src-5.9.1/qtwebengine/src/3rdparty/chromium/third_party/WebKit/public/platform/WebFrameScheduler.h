// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebFrameScheduler_h
#define WebFrameScheduler_h

#include "WebCommon.h"

#include <string>

namespace blink {

class WebSecurityOrigin;
class WebTaskRunner;
class WebViewScheduler;

class BLINK_PLATFORM_EXPORT WebFrameScheduler {
 public:
  virtual ~WebFrameScheduler() {}

  // The scheduler may throttle tasks associated with offscreen frames.
  virtual void setFrameVisible(bool) {}

  // Tells the scheduler that the page this frame belongs to is not visible.
  // The scheduler may throttle tasks associated with pages that are not
  // visible.
  virtual void setPageVisible(bool) {}

  // Set whether this frame is cross origin w.r.t. the top level frame. Cross
  // origin frames may use a different scheduling policy from same origin
  // frames.
  virtual void setCrossOrigin(bool) {}

  // Returns the WebTaskRunner for loading tasks.
  // WebFrameScheduler owns the returned WebTaskRunner.
  virtual WebTaskRunner* loadingTaskRunner() { return nullptr; }

  // Returns the WebTaskRunner for timer tasks.
  // WebFrameScheduler owns the returned WebTaskRunner.
  virtual WebTaskRunner* timerTaskRunner() { return nullptr; }

  // Returns the WebTaskRunner for tasks which should never get throttled.
  // This is generally used for executing internal browser tasks which should
  // never be throttled. Ideally only tasks whose performance characteristics
  // are known should be posted to this task runner; for example user
  // JavaScript is discouraged. WebFrameScheduler owns the returned
  // WebTaskRunner.
  virtual WebTaskRunner* unthrottledTaskRunner() { return nullptr; }

  // Returns the parent WebViewScheduler.
  virtual WebViewScheduler* webViewScheduler() { return nullptr; }

  // Tells the scheduler a resource load has started. The scheduler may make
  // policy decisions based on this.
  virtual void didStartLoading(unsigned long identifier) {}

  // Tells the scheduler a resource load has stopped. The scheduler may make
  // policy decisions based on this.
  virtual void didStopLoading(unsigned long identifier) {}

  // Tells the scheduler if we are parsing a document on another thread. This
  // tells the scheduler not to advance virtual time if it's using the
  // DETERMINISTIC_LOADING policy.
  virtual void setDocumentParsingInBackground(bool) {}
};

}  // namespace blink

#endif  // WebFrameScheduler_h
