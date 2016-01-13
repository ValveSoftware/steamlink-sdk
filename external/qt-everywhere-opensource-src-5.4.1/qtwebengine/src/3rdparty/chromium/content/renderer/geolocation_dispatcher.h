// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GEOLOCATION_DISPATCHER_H_
#define CONTENT_RENDERER_GEOLOCATION_DISPATCHER_H_

#include "base/memory/scoped_ptr.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/WebKit/public/web/WebGeolocationClient.h"
#include "third_party/WebKit/public/web/WebGeolocationController.h"

namespace blink {
class WebGeolocationController;
class WebGeolocationPermissionRequest;
class WebGeolocationPermissionRequestManager;
class WebGeolocationPosition;
}

namespace content {
struct Geoposition;

// GeolocationDispatcher is a delegate for Geolocation messages used by
// WebKit.
// It's the complement of GeolocationDispatcherHost.
class GeolocationDispatcher : public RenderFrameObserver,
                              public blink::WebGeolocationClient {
 public:
  explicit GeolocationDispatcher(RenderFrame* render_frame);
  virtual ~GeolocationDispatcher();

 private:
  // RenderFrame::Observer implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // WebGeolocationClient
  virtual void geolocationDestroyed();
  virtual void startUpdating();
  virtual void stopUpdating();
  virtual void setEnableHighAccuracy(bool enable_high_accuracy);
  virtual void setController(blink::WebGeolocationController* controller);
  virtual bool lastPosition(blink::WebGeolocationPosition& position);
  virtual void requestPermission(
      const blink::WebGeolocationPermissionRequest& permissionRequest);
  virtual void cancelPermissionRequest(
      const blink::WebGeolocationPermissionRequest& permissionRequest);

  // Permission for using geolocation has been set.
  void OnPermissionSet(int bridge_id, bool is_allowed);

  // We have an updated geolocation position or error code.
  void OnPositionUpdated(const content::Geoposition& geoposition);

  // The controller_ is valid for the lifetime of the underlying
  // WebCore::GeolocationController. geolocationDestroyed() is
  // invoked when the underlying object is destroyed.
  scoped_ptr<blink::WebGeolocationController> controller_;

  scoped_ptr<blink::WebGeolocationPermissionRequestManager>
      pending_permissions_;
  bool enable_high_accuracy_;
  bool updating_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_GEOLOCATION_DISPATCHER_H_
