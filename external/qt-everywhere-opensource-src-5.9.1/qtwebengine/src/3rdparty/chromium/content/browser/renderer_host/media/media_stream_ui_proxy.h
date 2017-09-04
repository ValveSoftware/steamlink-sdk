// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_UI_PROXY_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_UI_PROXY_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/media_stream_request.h"

namespace url {
class Origin;
}

namespace content {

class RenderFrameHostDelegate;

// MediaStreamUIProxy proxies calls to media stream UI between IO thread and UI
// thread. One instance of this class is create per MediaStream object. It must
// be created, used and destroyed on IO thread.
class CONTENT_EXPORT MediaStreamUIProxy {
 public:
  typedef base::Callback<
      void(const MediaStreamDevices& devices,
           content::MediaStreamRequestResult result)>
        ResponseCallback;

  typedef base::Callback<void(gfx::NativeViewId window_id)> WindowIdCallback;

  static std::unique_ptr<MediaStreamUIProxy> Create();
  static std::unique_ptr<MediaStreamUIProxy> CreateForTests(
      RenderFrameHostDelegate* render_delegate);

  virtual ~MediaStreamUIProxy();

  // Requests access for the MediaStream by calling
  // WebContentsDelegate::RequestMediaAccessPermission(). The specified
  // |response_callback| is called when the WebContentsDelegate approves or
  // denies request.
  virtual void RequestAccess(std::unique_ptr<MediaStreamRequest> request,
                             const ResponseCallback& response_callback);

  // Checks if we have permission to access the microphone or camera. Note that
  // this does not query the user, it checks any stored settings such as policy
  // or extension permissions. |type| must be MEDIA_DEVICE_AUDIO_CAPTURE
  // or MEDIA_DEVICE_VIDEO_CAPTURE.
  virtual void CheckAccess(const url::Origin& security_origin,
                           MediaStreamType type,
                           int render_process_id,
                           int render_frame_id,
                           const base::Callback<void(bool)>& callback);

  // Notifies the UI that the MediaStream has been started. Must be called after
  // access has been approved using RequestAccess(). |stop_callback| is be
  // called on the IO thread after the user has requests the stream to be
  // stopped. |window_id_callback| is called on the IO thread with the platform-
  // dependent window ID of the UI.
  virtual void OnStarted(const base::Closure& stop_callback,
                         const WindowIdCallback& window_id_callback);

  void SetRenderFrameHostDelegateForTests(RenderFrameHostDelegate* delegate);

 protected:
  explicit MediaStreamUIProxy(RenderFrameHostDelegate* test_render_delegate);

 private:
  class Core;
  friend class Core;
  friend class FakeMediaStreamUIProxy;

  void ProcessAccessRequestResponse(
      const MediaStreamDevices& devices,
      content::MediaStreamRequestResult result);
  void ProcessStopRequestFromUI();
  void OnWindowId(const WindowIdCallback& window_id_callback,
                  gfx::NativeViewId* window_id);
  void OnCheckedAccess(const base::Callback<void(bool)>& callback,
                       bool have_access);

  std::unique_ptr<Core, content::BrowserThread::DeleteOnUIThread> core_;
  ResponseCallback response_callback_;
  base::Closure stop_callback_;

  base::WeakPtrFactory<MediaStreamUIProxy> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamUIProxy);
};

class CONTENT_EXPORT FakeMediaStreamUIProxy : public MediaStreamUIProxy {
 public:
  explicit FakeMediaStreamUIProxy();
  ~FakeMediaStreamUIProxy() override;

  void SetAvailableDevices(const MediaStreamDevices& devices);
  void SetMicAccess(bool access);
  void SetCameraAccess(bool access);

  // MediaStreamUIProxy overrides.
  void RequestAccess(std::unique_ptr<MediaStreamRequest> request,
                     const ResponseCallback& response_callback) override;
  void CheckAccess(const url::Origin& security_origin,
                   MediaStreamType type,
                   int render_process_id,
                   int render_frame_id,
                   const base::Callback<void(bool)>& callback) override;
  void OnStarted(const base::Closure& stop_callback,
                 const WindowIdCallback& window_id_callback) override;

 private:
  // This is used for RequestAccess().
  MediaStreamDevices devices_;

  // These are used for CheckAccess().
  bool mic_access_;
  bool camera_access_;

  DISALLOW_COPY_AND_ASSIGN(FakeMediaStreamUIProxy);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_UI_PROXY_H_
