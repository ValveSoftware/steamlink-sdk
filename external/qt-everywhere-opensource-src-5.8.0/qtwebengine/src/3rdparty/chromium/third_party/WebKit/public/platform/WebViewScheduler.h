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
    virtual ~WebViewScheduler() { }

    // The scheduler may throttle tasks associated with background pages.
    virtual void setPageVisible(bool) = 0;

    // Creates a new WebFrameScheduler. The caller is responsible for deleting
    // it. All tasks executed by the frame scheduler will be attributed to
    // |BlameContext|.
    virtual std::unique_ptr<WebFrameScheduler> createFrameScheduler(BlameContext*) = 0;

    // Instructs this WebViewScheduler to use virtual time. When virtual time is enabled
    // the system doesn't actually sleep for the delays between tasks before executing
    // them. E.g: A-E are delayed tasks
    //
    // |    A   B C  D           E  (normal)
    // |-----------------------------> time
    //
    // |ABCDE                       (virtual time)
    // |-----------------------------> time
    virtual void enableVirtualTime() = 0;

    // Controls whether or not virtual time is allowed to advance. If virtual time
    // is not allowed to advance then delayed tasks posted to the WebTaskRunners owned
    // by any child WebFrameSchedulers will be paused. If virtual time is allowed to
    // advance then tasks will be run in time order (as usual) but virtual time will
    // fast forward so that the system doesn't actually sleep for the delays between
    // tasks before executing them.
    virtual void setAllowVirtualTimeToAdvance(bool) = 0;
};

} // namespace blink

#endif // WebViewScheduler
