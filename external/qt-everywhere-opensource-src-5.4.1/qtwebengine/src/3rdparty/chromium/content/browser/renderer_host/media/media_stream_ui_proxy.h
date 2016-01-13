// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_UI_PROXY_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_UI_PROXY_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/common/media_stream_request.h"

namespace content {

class RenderViewHostDelegate;

// MediaStreamUIProxy proxies calls to media stream UI between IO thread and UI
// thread. One instance of this class is create per MediaStream object. It must
// be create, used and destroyed on IO thread.
class CONTENT_EXPORT MediaStreamUIProxy {
 public:
  typedef base::Callback<
      void (const MediaStreamDevices& devices,
            content::MediaStreamRequestResult result)>
        ResponseCallback;

  typedef base::Callback<void(gfx::NativeViewId window_id)> WindowIdCallback;

  static scoped_ptr<MediaStreamUIProxy> Create();
  static scoped_ptr<MediaStreamUIProxy> CreateForTests(
      RenderViewHostDelegate* render_delegate);

  virtual ~MediaStreamUIProxy();

  // Requests access for the MediaStream by calling
  // WebContentsDelegate::RequestMediaAccessPermission(). The specified
  // |response_callback| is called when the WebContentsDelegate approves or
  // denies request.
  virtual void RequestAccess(const MediaStreamRequest& request,
                             const ResponseCallback& response_callback);

  // Notifies the UI that the MediaStream has been started. Must be called after
  // access has been approved using RequestAccess(). |stop_callback| is be
  // called on the IO thread after the user has requests the stream to be
  // stopped. |window_id_callback| is called on the IO thread with the platform-
  // dependent window ID of the UI.
  virtual void OnStarted(const base::Closure& stop_callback,
                         const WindowIdCallback& window_id_callback);

  void SetRenderViewHostDelegateForTests(RenderViewHostDelegate* delegate);

 protected:
  MediaStreamUIProxy(RenderViewHostDelegate* test_render_delegate);

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

  scoped_ptr<Core> core_;
  ResponseCallback response_callback_;
  base::Closure stop_callback_;

  base::WeakPtrFactory<MediaStreamUIProxy> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamUIProxy);
};

class CONTENT_EXPORT FakeMediaStreamUIProxy : public MediaStreamUIProxy {
 public:
  explicit FakeMediaStreamUIProxy();
  virtual ~FakeMediaStreamUIProxy();

  void SetAvailableDevices(const MediaStreamDevices& devices);

  // MediaStreamUIProxy overrides.
  virtual void RequestAccess(
      const MediaStreamRequest& request,
      const ResponseCallback& response_callback) OVERRIDE;
  virtual void OnStarted(const base::Closure& stop_callback,
                         const WindowIdCallback& window_id_callback) OVERRIDE;

 private:
  MediaStreamDevices devices_;

  DISALLOW_COPY_AND_ASSIGN(FakeMediaStreamUIProxy);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_UI_PROXY_H_
