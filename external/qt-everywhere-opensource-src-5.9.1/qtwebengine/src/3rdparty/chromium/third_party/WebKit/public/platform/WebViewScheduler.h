// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebViewScheduler_h
#define WebViewScheduler_h

#include "WebCommon.h"
#include "public/platform/BlameContext.h"

#include <memory>

namespace blink {

class WebFrameScheduler;

class BLINK_PLATFORM_EXPORT WebViewScheduler {
 public:
  // Helper interface to plumb settings from WebSettings to scheduler.
  class BLINK_PLATFORM_EXPORT WebViewSchedulerSettings {
   public:
    virtual ~WebViewSchedulerSettings() {}

    // Background throttling aggressiveness settings.
    virtual float expensiveBackgroundThrottlingCPUBudget() = 0;
    virtual float expensiveBackgroundThrottlingInitialBudget() = 0;
    virtual float expensiveBackgroundThrottlingMaxBudget() = 0;
    virtual float expensiveBackgroundThrottlingMaxDelay() = 0;
  };

  virtual ~WebViewScheduler() {}

  // The scheduler may throttle tasks associated with background pages.
  virtual void setPageVisible(bool) = 0;

  // Creates a new WebFrameScheduler. The caller is responsible for deleting
  // it. All tasks executed by the frame scheduler will be attributed to
  // |BlameContext|.
  virtual std::unique_ptr<WebFrameScheduler> createFrameScheduler(
      BlameContext*) = 0;

  // Instructs this WebViewScheduler to use virtual time. When virtual time is
  // enabled the system doesn't actually sleep for the delays between tasks
  // before executing them. E.g: A-E are delayed tasks
  //
  // |    A   B C  D           E  (normal)
  // |-----------------------------> time
  //
  // |ABCDE                       (virtual time)
  // |-----------------------------> time
  virtual void enableVirtualTime() = 0;

  // Returns true if virtual time is currently allowed to advance.
  virtual bool virtualTimeAllowedToAdvance() const = 0;

  enum class VirtualTimePolicy {
    // In this policy virtual time is allowed to advance. If the blink scheduler
    // runs out of immediate work, the virtual timebase will be incremented so
    // that the next sceduled timer may fire.  NOTE Tasks will be run in time
    // order (as usual).
    ADVANCE,

    // In this policy virtual time is not allowed to advance. Delayed tasks
    // posted to WebTaskRunners owned by any child WebFrameSchedulers will be
    // paused, unless their scheduled run time is less than or equal to the
    // current virtual time.  Note non-delayed tasks will run as normal.
    PAUSE,

    // In this policy virtual time is allowed to advance unless there are
    // pending network fetches associated any child WebFrameScheduler, or a
    // document is being parsed on a background thread. Initially virtual time
    // is not allowed to advance until we have seen at least one load. The aim
    // being to try and make loading (more) deterministic.
    DETERMINISTIC_LOADING
  };

  // Sets the virtual time policy, which is applied imemdiatly to all child
  // WebFrameSchedulers.
  virtual void setVirtualTimePolicy(VirtualTimePolicy) = 0;

  virtual void audioStateChanged(bool isAudioPlaying) = 0;
};

}  // namespace blink

#endif  // WebViewScheduler
