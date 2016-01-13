// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/geolocation_dispatcher_host.h"

#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_message_filter.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/geoposition.h"
#include "content/common/geolocation_messages.h"

namespace content {
namespace {

// Geoposition error codes for reporting in UMA.
enum GeopositionErrorCode {
  // NOTE: Do not renumber these as that would confuse interpretation of
  // previously logged data. When making changes, also update the enum list
  // in tools/metrics/histograms/histograms.xml to keep it in sync.

  // There was no error.
  GEOPOSITION_ERROR_CODE_NONE = 0,

  // User denied use of geolocation.
  GEOPOSITION_ERROR_CODE_PERMISSION_DENIED = 1,

  // Geoposition could not be determined.
  GEOPOSITION_ERROR_CODE_POSITION_UNAVAILABLE = 2,

  // Timeout.
  GEOPOSITION_ERROR_CODE_TIMEOUT = 3,

  // NOTE: Add entries only immediately above this line.
  GEOPOSITION_ERROR_CODE_COUNT = 4
};

void RecordGeopositionErrorCode(Geoposition::ErrorCode error_code) {
  GeopositionErrorCode code = GEOPOSITION_ERROR_CODE_NONE;
  switch (error_code) {
    case Geoposition::ERROR_CODE_NONE:
      code = GEOPOSITION_ERROR_CODE_NONE;
      break;
    case Geoposition::ERROR_CODE_PERMISSION_DENIED:
      code = GEOPOSITION_ERROR_CODE_PERMISSION_DENIED;
      break;
    case Geoposition::ERROR_CODE_POSITION_UNAVAILABLE:
      code = GEOPOSITION_ERROR_CODE_POSITION_UNAVAILABLE;
      break;
    case Geoposition::ERROR_CODE_TIMEOUT:
      code = GEOPOSITION_ERROR_CODE_TIMEOUT;
      break;
  }
  UMA_HISTOGRAM_ENUMERATION("Geolocation.LocationUpdate.ErrorCode",
                            code,
                            GEOPOSITION_ERROR_CODE_COUNT);
}

}  // namespace

GeolocationDispatcherHost::PendingPermission::PendingPermission(
    int render_frame_id,
    int render_process_id,
    int bridge_id)
    : render_frame_id(render_frame_id),
      render_process_id(render_process_id),
      bridge_id(bridge_id) {
}

GeolocationDispatcherHost::PendingPermission::~PendingPermission() {
}

GeolocationDispatcherHost::GeolocationDispatcherHost(
    WebContents* web_contents)
    : WebContentsObserver(web_contents),
      paused_(false),
      weak_factory_(this) {
  // This is initialized by WebContentsImpl. Do not add any non-trivial
  // initialization here, defer to OnStartUpdating which is triggered whenever
  // a javascript geolocation object is actually initialized.
}

GeolocationDispatcherHost::~GeolocationDispatcherHost() {
}

void GeolocationDispatcherHost::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  OnStopUpdating(render_frame_host);
}

void GeolocationDispatcherHost::RenderViewHostChanged(
    RenderViewHost* old_host,
    RenderViewHost* new_host) {
  updating_frames_.clear();
  paused_ = false;
  geolocation_subscription_.reset();
}

bool GeolocationDispatcherHost::OnMessageReceived(
    const IPC::Message& msg, RenderFrameHost* render_frame_host) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(GeolocationDispatcherHost, msg,
                                   render_frame_host)
    IPC_MESSAGE_HANDLER(GeolocationHostMsg_RequestPermission,
                        OnRequestPermission)
    IPC_MESSAGE_HANDLER(GeolocationHostMsg_CancelPermissionRequest,
                        OnCancelPermissionRequest)
    IPC_MESSAGE_HANDLER(GeolocationHostMsg_StartUpdating, OnStartUpdating)
    IPC_MESSAGE_HANDLER(GeolocationHostMsg_StopUpdating, OnStopUpdating)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void GeolocationDispatcherHost::OnLocationUpdate(
    const Geoposition& geoposition) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  RecordGeopositionErrorCode(geoposition.error_code);
  if (paused_)
    return;

  for (std::map<RenderFrameHost*, bool>::iterator i = updating_frames_.begin();
       i != updating_frames_.end(); ++i) {
    i->first->Send(new GeolocationMsg_PositionUpdated(
        i->first->GetRoutingID(), geoposition));
  }
}

void GeolocationDispatcherHost::OnRequestPermission(
    RenderFrameHost* render_frame_host,
    int bridge_id,
    const GURL& requesting_frame,
    bool user_gesture) {
  int render_process_id = render_frame_host->GetProcess()->GetID();
  int render_frame_id = render_frame_host->GetRoutingID();

  PendingPermission pending_permission(
      render_frame_id, render_process_id, bridge_id);
  pending_permissions_.push_back(pending_permission);

  GetContentClient()->browser()->RequestGeolocationPermission(
      web_contents(),
      bridge_id,
      requesting_frame,
      user_gesture,
      base::Bind(&GeolocationDispatcherHost::SendGeolocationPermissionResponse,
                 weak_factory_.GetWeakPtr(),
                 render_process_id, render_frame_id, bridge_id),
      &pending_permissions_.back().cancel);
}

void GeolocationDispatcherHost::OnCancelPermissionRequest(
    RenderFrameHost* render_frame_host,
    int bridge_id,
    const GURL& requesting_frame) {
  int render_process_id = render_frame_host->GetProcess()->GetID();
  int render_frame_id = render_frame_host->GetRoutingID();
  for (size_t i = 0; i < pending_permissions_.size(); ++i) {
    if (pending_permissions_[i].render_process_id == render_process_id &&
        pending_permissions_[i].render_frame_id == render_frame_id &&
        pending_permissions_[i].bridge_id == bridge_id) {
      if (!pending_permissions_[i].cancel.is_null())
        pending_permissions_[i].cancel.Run();
      pending_permissions_.erase(pending_permissions_.begin() + i);
      return;
    }
  }
}

void GeolocationDispatcherHost::OnStartUpdating(
    RenderFrameHost* render_frame_host,
    const GURL& requesting_frame,
    bool enable_high_accuracy) {
  // StartUpdating() can be invoked as a result of high-accuracy mode
  // being enabled / disabled. No need to record the dispatcher again.
  UMA_HISTOGRAM_BOOLEAN(
      "Geolocation.GeolocationDispatcherHostImpl.EnableHighAccuracy",
      enable_high_accuracy);

  updating_frames_[render_frame_host] = enable_high_accuracy;
  RefreshGeolocationOptions();
}

void GeolocationDispatcherHost::OnStopUpdating(
    RenderFrameHost* render_frame_host) {
  updating_frames_.erase(render_frame_host);
  RefreshGeolocationOptions();
}

void GeolocationDispatcherHost::PauseOrResume(bool should_pause) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  paused_ = should_pause;
  RefreshGeolocationOptions();
}

void GeolocationDispatcherHost::RefreshGeolocationOptions() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (updating_frames_.empty() || paused_) {
    geolocation_subscription_.reset();
    return;
  }

  bool high_accuracy = false;
  for (std::map<RenderFrameHost*, bool>::iterator i =
            updating_frames_.begin(); i != updating_frames_.end(); ++i) {
    if (i->second) {
      high_accuracy = true;
      break;
    }
  }
  geolocation_subscription_ = GeolocationProvider::GetInstance()->
      AddLocationUpdateCallback(
          base::Bind(&GeolocationDispatcherHost::OnLocationUpdate,
                      base::Unretained(this)),
          high_accuracy);
}

void GeolocationDispatcherHost::SendGeolocationPermissionResponse(
    int render_process_id,
    int render_frame_id,
    int bridge_id,
    bool allowed) {
  for (size_t i = 0; i < pending_permissions_.size(); ++i) {
    if (pending_permissions_[i].render_process_id == render_process_id &&
        pending_permissions_[i].render_frame_id == render_frame_id &&
        pending_permissions_[i].bridge_id == bridge_id) {
      RenderFrameHost* render_frame_host =
          RenderFrameHost::FromID(render_process_id, render_frame_id);
      if (render_frame_host) {
        render_frame_host->Send(new GeolocationMsg_PermissionSet(
            render_frame_id, bridge_id, allowed));
      }

      if (allowed) {
        GeolocationProviderImpl::GetInstance()->
            UserDidOptIntoLocationServices();
      }

      pending_permissions_.erase(pending_permissions_.begin() + i);
      return;
    }
  }

  NOTREACHED();
}

}  // namespace content
