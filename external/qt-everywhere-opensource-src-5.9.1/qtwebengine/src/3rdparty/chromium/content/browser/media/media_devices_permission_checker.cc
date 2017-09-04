// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_devices_permission_checker.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "content/browser/frame_host/render_frame_host_delegate.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/common/media/media_devices.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

MediaDevicesManager::BoolDeviceTypes DoCheckPermissionsOnUIThread(
    MediaDevicesManager::BoolDeviceTypes requested_device_types,
    int render_process_id,
    int render_frame_id,
    const url::Origin& security_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHostImpl* frame_host =
      RenderFrameHostImpl::FromID(render_process_id, render_frame_id);

  // If there is no |frame_host|, return false for all permissions.
  if (!frame_host)
    return MediaDevicesManager::BoolDeviceTypes();

  RenderFrameHostDelegate* delegate = frame_host->delegate();
  GURL origin = security_origin.GetURL();

  // Currently, the MEDIA_DEVICE_AUDIO_CAPTURE permission is used for
  // both audio input and output.
  // TODO(guidou): use specific permission for audio output when it becomes
  // available. See http://crbug.com/556542.
  bool has_audio_permission =
      (requested_device_types[MEDIA_DEVICE_TYPE_AUDIO_INPUT] ||
       requested_device_types[MEDIA_DEVICE_TYPE_AUDIO_OUTPUT]) &&
      delegate->CheckMediaAccessPermission(origin, MEDIA_DEVICE_AUDIO_CAPTURE);

  MediaDevicesManager::BoolDeviceTypes result;
  result[MEDIA_DEVICE_TYPE_AUDIO_INPUT] = has_audio_permission;
  result[MEDIA_DEVICE_TYPE_AUDIO_OUTPUT] = has_audio_permission;
  result[MEDIA_DEVICE_TYPE_VIDEO_INPUT] =
      requested_device_types[MEDIA_DEVICE_TYPE_VIDEO_INPUT] &&
      delegate->CheckMediaAccessPermission(origin, MEDIA_DEVICE_VIDEO_CAPTURE);

  return result;
}

bool CheckSinglePermissionOnUIThread(MediaDeviceType device_type,
                                     int render_process_id,
                                     int render_frame_id,
                                     const url::Origin& security_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  MediaDevicesManager::BoolDeviceTypes requested;
  requested[device_type] = true;
  MediaDevicesManager::BoolDeviceTypes result = DoCheckPermissionsOnUIThread(
      requested, render_process_id, render_frame_id, security_origin);
  return result[device_type];
}

}  // namespace

MediaDevicesPermissionChecker::MediaDevicesPermissionChecker()
    : use_override_(base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeUIForMediaStream)),
      override_value_(
          base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              switches::kUseFakeUIForMediaStream) != "deny") {}

MediaDevicesPermissionChecker::MediaDevicesPermissionChecker(
    bool override_value)
    : use_override_(true), override_value_(override_value) {}

bool MediaDevicesPermissionChecker::CheckPermissionOnUIThread(
    MediaDeviceType device_type,
    int render_process_id,
    int render_frame_id,
    const url::Origin& security_origin) const {
  if (use_override_)
    return override_value_;

  return CheckSinglePermissionOnUIThread(device_type, render_process_id,
                                         render_frame_id, security_origin);
}

void MediaDevicesPermissionChecker::CheckPermission(
    MediaDeviceType device_type,
    int render_process_id,
    int render_frame_id,
    const url::Origin& security_origin,
    const base::Callback<void(bool)>& callback) const {
  if (use_override_) {
    callback.Run(override_value_);
    return;
  }

  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&CheckSinglePermissionOnUIThread, device_type,
                 render_process_id, render_frame_id, security_origin),
      callback);
}

MediaDevicesManager::BoolDeviceTypes
MediaDevicesPermissionChecker::CheckPermissionsOnUIThread(
    MediaDevicesManager::BoolDeviceTypes requested_device_types,
    int render_process_id,
    int render_frame_id,
    const url::Origin& security_origin) const {
  if (use_override_) {
    MediaDevicesManager::BoolDeviceTypes result;
    result.fill(override_value_);
    return result;
  }

  return DoCheckPermissionsOnUIThread(requested_device_types, render_process_id,
                                      render_frame_id, security_origin);
}

void MediaDevicesPermissionChecker::CheckPermissions(
    MediaDevicesManager::BoolDeviceTypes requested,
    int render_process_id,
    int render_frame_id,
    const url::Origin& security_origin,
    const base::Callback<void(const MediaDevicesManager::BoolDeviceTypes&)>&
        callback) const {
  if (use_override_) {
    MediaDevicesManager::BoolDeviceTypes result;
    result.fill(override_value_);
    callback.Run(result);
    return;
  }

  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&DoCheckPermissionsOnUIThread, requested, render_process_id,
                 render_frame_id, security_origin),
      callback);
}

}  // namespace content
