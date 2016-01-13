// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_MEDIA_DEVICE_MANAGER_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_MEDIA_DEVICE_MANAGER_H_

#include <map>

#include "base/memory/weak_ptr.h"
#include "content/renderer/media/media_stream_dispatcher_eventhandler.h"
#include "content/renderer/pepper/pepper_device_enumeration_host_helper.h"
#include "content/public/renderer/render_view_observer_tracker.h"
#include "content/public/renderer/render_view_observer.h"

namespace content {
class RenderViewImpl;

class PepperMediaDeviceManager
    : public MediaStreamDispatcherEventHandler,
      public PepperDeviceEnumerationHostHelper::Delegate,
      public RenderViewObserver,
      public RenderViewObserverTracker<PepperMediaDeviceManager>,
      public base::SupportsWeakPtr<PepperMediaDeviceManager> {
 public:
  static PepperMediaDeviceManager* GetForRenderView(RenderView* render_view);
  virtual ~PepperMediaDeviceManager();

  // PepperDeviceEnumerationHostHelper::Delegate implementation:
  virtual int EnumerateDevices(PP_DeviceType_Dev type,
                               const GURL& document_url,
                               const EnumerateDevicesCallback& callback)
      OVERRIDE;
  virtual void StopEnumerateDevices(int request_id) OVERRIDE;

  typedef base::Callback<void(int /* request_id */,
                              bool /* succeeded */,
                              const std::string& /* label */)>
      OpenDeviceCallback;

  // Opens the specified device. The request ID passed into the callback will be
  // the same as the return value. If successful, the label passed into the
  // callback identifies a audio/video steam, which can be used to call
  // CloseDevice() and GetSesssionID().
  int OpenDevice(PP_DeviceType_Dev type,
                 const std::string& device_id,
                 const GURL& document_url,
                 const OpenDeviceCallback& callback);
  // Cancels an request to open device, using the request ID returned by
  // OpenDevice(). It is guaranteed that the callback passed into OpenDevice()
  // won't be called afterwards.
  void CancelOpenDevice(int request_id);
  void CloseDevice(const std::string& label);
  // Gets audio/video session ID given a label.
  int GetSessionID(PP_DeviceType_Dev type, const std::string& label);

  // MediaStreamDispatcherEventHandler implementation.
  virtual void OnStreamGenerated(
      int request_id,
      const std::string& label,
      const StreamDeviceInfoArray& audio_device_array,
      const StreamDeviceInfoArray& video_device_array) OVERRIDE;
  virtual void OnStreamGenerationFailed(
      int request_id,
      content::MediaStreamRequestResult result) OVERRIDE;
  virtual void OnDeviceStopped(const std::string& label,
                               const StreamDeviceInfo& device_info) OVERRIDE;
  virtual void OnDevicesEnumerated(int request_id,
                                   const StreamDeviceInfoArray& device_array)
      OVERRIDE;
  virtual void OnDeviceOpened(int request_id,
                              const std::string& label,
                              const StreamDeviceInfo& device_info) OVERRIDE;
  virtual void OnDeviceOpenFailed(int request_id) OVERRIDE;

  // Stream type conversion.
  static MediaStreamType FromPepperDeviceType(PP_DeviceType_Dev type);
  static PP_DeviceType_Dev FromMediaStreamType(MediaStreamType type);

 private:
  PepperMediaDeviceManager(RenderView* render_view);

  void NotifyDeviceOpened(int request_id,
                          bool succeeded,
                          const std::string& label);

  RenderViewImpl* GetRenderViewImpl();

  int next_id_;

  typedef std::map<int, EnumerateDevicesCallback> EnumerateCallbackMap;
  EnumerateCallbackMap enumerate_callbacks_;

  typedef std::map<int, OpenDeviceCallback> OpenCallbackMap;
  OpenCallbackMap open_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(PepperMediaDeviceManager);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_MEDIA_DEVICE_MANAGER_H_
