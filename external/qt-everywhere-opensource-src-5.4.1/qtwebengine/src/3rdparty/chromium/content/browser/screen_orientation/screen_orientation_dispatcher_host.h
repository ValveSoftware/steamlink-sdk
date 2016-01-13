// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCREEN_ORIENTATION_SCREEN_ORIENTATION_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_SCREEN_ORIENTATION_SCREEN_ORIENTATION_DISPATCHER_HOST_H_

#include "content/public/browser/web_contents_observer.h"
#include "third_party/WebKit/public/platform/WebScreenOrientationLockType.h"
#include "third_party/WebKit/public/platform/WebScreenOrientationType.h"

namespace content {

class RenderFrameHost;
class ScreenOrientationProvider;
class WebContents;

// ScreenOrientationDispatcherHost receives lock and unlock requests from the
// RenderFrames and dispatch them to the ScreenOrientationProvider. It also
// make sure that the right RenderFrame get replied for each lock request.
class CONTENT_EXPORT ScreenOrientationDispatcherHost
    : public WebContentsObserver {
 public:
  explicit ScreenOrientationDispatcherHost(WebContents* web_contents);
  virtual ~ScreenOrientationDispatcherHost();

  // WebContentsObserver
  virtual bool OnMessageReceived(const IPC::Message&,
                                 RenderFrameHost* render_frame_host) OVERRIDE;

  void OnOrientationChange(blink::WebScreenOrientationType orientation);

  void SetProviderForTests(ScreenOrientationProvider* provider);

 private:
  void OnLockRequest(RenderFrameHost* render_frame_host,
                     blink::WebScreenOrientationLockType orientation,
                     int request_id);
  void OnUnlockRequest(RenderFrameHost* render_frame_host);

  static ScreenOrientationProvider* CreateProvider();

  scoped_ptr<ScreenOrientationProvider> provider_;

  DISALLOW_COPY_AND_ASSIGN(ScreenOrientationDispatcherHost);
};

}  // namespace content

#endif // CONTENT_BROWSER_SCREEN_ORIENTATION_SCREEN_ORIENTATION_DISPATCHER_HOST_H_
