// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_DISPATCHER_HOST_H_

#include <map>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/geolocation/geolocation_provider_impl.h"
#include "content/public/browser/web_contents_observer.h"

class GURL;

namespace content {

// GeolocationDispatcherHost is an observer for Geolocation messages.
// It's the complement of GeolocationDispatcher (owned by RenderView).
class GeolocationDispatcherHost : public WebContentsObserver {
 public:
  explicit GeolocationDispatcherHost(WebContents* web_contents);
  virtual ~GeolocationDispatcherHost();

  // Pause or resumes geolocation. Resuming when nothing is paused is a no-op.
  // If the web contents is paused while not currently using geolocation but
  // then goes on to do so before being resumed, then it will not get
  // geolocation updates until it is resumed.
  void PauseOrResume(bool should_pause);

 private:
  // WebContentsObserver
  virtual void RenderFrameDeleted(RenderFrameHost* render_frame_host) OVERRIDE;
  virtual void RenderViewHostChanged(RenderViewHost* old_host,
                                     RenderViewHost* new_host) OVERRIDE;
  virtual bool OnMessageReceived(
      const IPC::Message& msg, RenderFrameHost* render_frame_host) OVERRIDE;

  // Message handlers:
  void OnRequestPermission(RenderFrameHost* render_frame_host,
                           int bridge_id,
                           const GURL& requesting_frame,
                           bool user_gesture);
  void OnCancelPermissionRequest(RenderFrameHost* render_frame_host,
                                 int bridge_id,
                                 const GURL& requesting_frame);
  void OnStartUpdating(RenderFrameHost* render_frame_host,
                       const GURL& requesting_frame,
                       bool enable_high_accuracy);
  void OnStopUpdating(RenderFrameHost* render_frame_host);

  // Updates the geolocation provider with the currently required update
  // options.
  void RefreshGeolocationOptions();

  void OnLocationUpdate(const Geoposition& position);

  void SendGeolocationPermissionResponse(int render_process_id,
                                         int render_frame_id,
                                         int bridge_id,
                                         bool allowed);

  // A map from the RenderFrameHosts that have requested geolocation updates to
  // the type of accuracy they requested (true = high accuracy).
  std::map<RenderFrameHost*, bool> updating_frames_;
  bool paused_;

  struct PendingPermission {
    PendingPermission(int render_frame_id,
                      int render_process_id,
                      int bridge_id);
    ~PendingPermission();
    int render_frame_id;
    int render_process_id;
    int bridge_id;
    base::Closure cancel;
  };
  std::vector<PendingPermission> pending_permissions_;

  scoped_ptr<GeolocationProvider::Subscription> geolocation_subscription_;

  base::WeakPtrFactory<GeolocationDispatcherHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_DISPATCHER_HOST_H_
