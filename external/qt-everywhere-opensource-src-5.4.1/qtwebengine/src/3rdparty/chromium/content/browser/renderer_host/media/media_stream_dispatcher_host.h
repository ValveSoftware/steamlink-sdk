// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_DISPATCHER_HOST_H_

#include <map>
#include <string>
#include <utility>

#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/media_stream_requester.h"
#include "content/common/content_export.h"
#include "content/common/media/media_stream_options.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/resource_context.h"

namespace content {

class MediaStreamManager;
class ResourceContext;

// MediaStreamDispatcherHost is a delegate for Media Stream API messages used by
// MediaStreamImpl. It's the complement of MediaStreamDispatcher
// (owned by RenderView).
class CONTENT_EXPORT MediaStreamDispatcherHost : public BrowserMessageFilter,
                                                 public MediaStreamRequester {
 public:
  MediaStreamDispatcherHost(
      int render_process_id,
      const ResourceContext::SaltCallback& salt_callback,
      MediaStreamManager* media_stream_manager,
      ResourceContext* resource_context);

  // MediaStreamRequester implementation.
  virtual void StreamGenerated(
      int render_view_id,
      int page_request_id,
      const std::string& label,
      const StreamDeviceInfoArray& audio_devices,
      const StreamDeviceInfoArray& video_devices) OVERRIDE;
  virtual void StreamGenerationFailed(
      int render_view_id,
      int page_request_id,
      content::MediaStreamRequestResult result) OVERRIDE;
  virtual void DeviceStopped(int render_view_id,
                             const std::string& label,
                             const StreamDeviceInfo& device) OVERRIDE;
  virtual void DevicesEnumerated(int render_view_id,
                                 int page_request_id,
                                 const std::string& label,
                                 const StreamDeviceInfoArray& devices) OVERRIDE;
  virtual void DeviceOpened(int render_view_id,
                            int page_request_id,
                            const std::string& label,
                            const StreamDeviceInfo& video_device) OVERRIDE;

  // BrowserMessageFilter implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelClosing() OVERRIDE;

 protected:
  virtual ~MediaStreamDispatcherHost();

 private:
  friend class MockMediaStreamDispatcherHost;

  void OnGenerateStream(int render_view_id,
                        int page_request_id,
                        const StreamOptions& components,
                        const GURL& security_origin,
                        bool user_gesture);
  void OnCancelGenerateStream(int render_view_id,
                              int page_request_id);
  void OnStopStreamDevice(int render_view_id,
                          const std::string& device_id);

  void OnEnumerateDevices(int render_view_id,
                          int page_request_id,
                          MediaStreamType type,
                          const GURL& security_origin,
                          bool hide_labels_if_no_access);

  void OnCancelEnumerateDevices(int render_view_id,
                                int page_request_id);

  void OnOpenDevice(int render_view_id,
                    int page_request_id,
                    const std::string& device_id,
                    MediaStreamType type,
                    const GURL& security_origin);

  void OnCloseDevice(int render_view_id,
                     const std::string& label);

  void StoreRequest(int render_view_id,
                    int page_request_id,
                    const std::string& label);

  bool IsURLAllowed(const GURL& url);

  int render_process_id_;
  ResourceContext::SaltCallback salt_callback_;
  MediaStreamManager* media_stream_manager_;

  // Owned by ProfileIOData which is guaranteed to outlive MSDH.
  ResourceContext* const resource_context_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_DISPATCHER_HOST_H_
