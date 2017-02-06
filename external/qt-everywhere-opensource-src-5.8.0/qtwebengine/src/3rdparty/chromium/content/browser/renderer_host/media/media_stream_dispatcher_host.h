// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_DISPATCHER_HOST_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/media_stream_requester.h"
#include "content/common/content_export.h"
#include "content/common/media/media_stream_options.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/resource_context.h"
#include "url/origin.h"

namespace content {

class MediaStreamManager;
class MediaStreamUIProxy;
class ResourceContext;

// MediaStreamDispatcherHost is a delegate for Media Stream API messages used by
// MediaStreamImpl.  There is one MediaStreamDispatcherHost per
// RenderProcessHost, the former owned by the latter.
class CONTENT_EXPORT MediaStreamDispatcherHost : public BrowserMessageFilter,
                                                 public MediaStreamRequester {
 public:
  MediaStreamDispatcherHost(int render_process_id,
                            const std::string& salt,
                            MediaStreamManager* media_stream_manager,
                            bool use_fake_ui = false);

  // MediaStreamRequester implementation.
  void StreamGenerated(int render_frame_id,
                       int page_request_id,
                       const std::string& label,
                       const StreamDeviceInfoArray& audio_devices,
                       const StreamDeviceInfoArray& video_devices) override;
  void StreamGenerationFailed(
      int render_frame_id,
      int page_request_id,
      content::MediaStreamRequestResult result) override;
  void DeviceStopped(int render_frame_id,
                     const std::string& label,
                     const StreamDeviceInfo& device) override;
  void DevicesEnumerated(int render_frame_id,
                         int page_request_id,
                         const std::string& label,
                         const StreamDeviceInfoArray& devices) override;
  void DeviceOpened(int render_frame_id,
                    int page_request_id,
                    const std::string& label,
                    const StreamDeviceInfo& video_device) override;
  void DevicesChanged(MediaStreamType type) override;

  // BrowserMessageFilter implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnChannelClosing() override;

 protected:
  ~MediaStreamDispatcherHost() override;

 private:
  friend class MockMediaStreamDispatcherHost;

  void OnGenerateStream(int render_frame_id,
                        int page_request_id,
                        const StreamControls& controls,
                        const url::Origin& security_origin,
                        bool user_gesture);
  void OnCancelGenerateStream(int render_frame_id,
                              int page_request_id);
  void OnStopStreamDevice(int render_frame_id,
                          const std::string& device_id);

  void OnEnumerateDevices(int render_frame_id,
                          int page_request_id,
                          MediaStreamType type,
                          const url::Origin& security_origin);

  void OnCancelEnumerateDevices(int render_frame_id,
                                int page_request_id);

  void OnOpenDevice(int render_frame_id,
                    int page_request_id,
                    const std::string& device_id,
                    MediaStreamType type,
                    const url::Origin& security_origin);

  void OnCloseDevice(int render_frame_id,
                     const std::string& label);

  void OnSubscribeToDeviceChangeNotifications(
      int render_frame_id,
      const url::Origin& security_origin);

  void OnCancelDeviceChangeNotifications(int render_frame_id);

  void StoreRequest(int render_frame_id,
                    int page_request_id,
                    const std::string& label);

  // IPC message handler: Set if the video capturing is secure.
  void OnSetCapturingLinkSecured(int session_id,
                                 content::MediaStreamType type,
                                 bool is_secure);

  std::unique_ptr<MediaStreamUIProxy> CreateMediaStreamUIProxy();
  void HandleCheckAccessResponse(std::unique_ptr<MediaStreamUIProxy> ui_proxy,
                                 int render_frame_id,
                                 bool have_access);

  int render_process_id_;
  std::string salt_;
  MediaStreamManager* media_stream_manager_;

  struct DeviceChangeSubscriberInfo {
    int render_frame_id;
    url::Origin security_origin;
  };
  std::vector<DeviceChangeSubscriberInfo> device_change_subscribers_;
  bool use_fake_ui_;

  base::WeakPtrFactory<MediaStreamDispatcherHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_DISPATCHER_HOST_H_
