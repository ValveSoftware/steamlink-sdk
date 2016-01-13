// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_stream_ui_proxy.h"

#include "base/command_line.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "media/video/capture/fake_video_capture_device.h"

namespace content {

class MediaStreamUIProxy::Core {
 public:
  explicit Core(const base::WeakPtr<MediaStreamUIProxy>& proxy,
                RenderViewHostDelegate* test_render_delegate);
  ~Core();

  void RequestAccess(const MediaStreamRequest& request);
  void OnStarted(gfx::NativeViewId* window_id);

 private:
  void ProcessAccessRequestResponse(const MediaStreamDevices& devices,
                                    content::MediaStreamRequestResult result,
                                    scoped_ptr<MediaStreamUI> stream_ui);
  void ProcessStopRequestFromUI();

  base::WeakPtr<MediaStreamUIProxy> proxy_;
  scoped_ptr<MediaStreamUI> ui_;

  RenderViewHostDelegate* const test_render_delegate_;

  // WeakPtr<> is used to RequestMediaAccessPermission() because there is no way
  // cancel media requests.
  base::WeakPtrFactory<Core> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

MediaStreamUIProxy::Core::Core(const base::WeakPtr<MediaStreamUIProxy>& proxy,
                               RenderViewHostDelegate* test_render_delegate)
    : proxy_(proxy),
      test_render_delegate_(test_render_delegate),
      weak_factory_(this) {
}

MediaStreamUIProxy::Core::~Core() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void MediaStreamUIProxy::Core::RequestAccess(
    const MediaStreamRequest& request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderViewHostDelegate* render_delegate;

  if (test_render_delegate_) {
    render_delegate = test_render_delegate_;
  } else {
    RenderViewHostImpl* host = RenderViewHostImpl::FromID(
        request.render_process_id, request.render_view_id);

    // Tab may have gone away.
    if (!host || !host->GetDelegate()) {
      ProcessAccessRequestResponse(
          MediaStreamDevices(),
          MEDIA_DEVICE_INVALID_STATE,
          scoped_ptr<MediaStreamUI>());
      return;
    }

    render_delegate = host->GetDelegate();
  }

  render_delegate->RequestMediaAccessPermission(
      request, base::Bind(&Core::ProcessAccessRequestResponse,
                          weak_factory_.GetWeakPtr()));
}

void MediaStreamUIProxy::Core::OnStarted(gfx::NativeViewId* window_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (ui_) {
    *window_id = ui_->OnStarted(
        base::Bind(&Core::ProcessStopRequestFromUI, base::Unretained(this)));
  }
}

void MediaStreamUIProxy::Core::ProcessAccessRequestResponse(
    const MediaStreamDevices& devices,
    content::MediaStreamRequestResult result,
    scoped_ptr<MediaStreamUI> stream_ui) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ui_ = stream_ui.Pass();
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&MediaStreamUIProxy::ProcessAccessRequestResponse,
                 proxy_, devices, result));
}

void MediaStreamUIProxy::Core::ProcessStopRequestFromUI() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&MediaStreamUIProxy::ProcessStopRequestFromUI, proxy_));
}

// static
scoped_ptr<MediaStreamUIProxy> MediaStreamUIProxy::Create() {
  return scoped_ptr<MediaStreamUIProxy>(new MediaStreamUIProxy(NULL));
}

// static
scoped_ptr<MediaStreamUIProxy> MediaStreamUIProxy::CreateForTests(
    RenderViewHostDelegate* render_delegate) {
  return scoped_ptr<MediaStreamUIProxy>(
      new MediaStreamUIProxy(render_delegate));
}

MediaStreamUIProxy::MediaStreamUIProxy(
    RenderViewHostDelegate* test_render_delegate)
    : weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  core_.reset(new Core(weak_factory_.GetWeakPtr(), test_render_delegate));
}

MediaStreamUIProxy::~MediaStreamUIProxy() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::DeleteSoon(BrowserThread::UI, FROM_HERE, core_.release());
}

void MediaStreamUIProxy::RequestAccess(
    const MediaStreamRequest& request,
    const ResponseCallback& response_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  response_callback_ = response_callback;
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&Core::RequestAccess, base::Unretained(core_.get()), request));
}

void MediaStreamUIProxy::OnStarted(const base::Closure& stop_callback,
                                   const WindowIdCallback& window_id_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  stop_callback_ = stop_callback;

  // Owned by the PostTaskAndReply callback.
  gfx::NativeViewId* window_id = new gfx::NativeViewId(0);

  BrowserThread::PostTaskAndReply(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&Core::OnStarted, base::Unretained(core_.get()), window_id),
      base::Bind(&MediaStreamUIProxy::OnWindowId,
                 weak_factory_.GetWeakPtr(),
                 window_id_callback,
                 base::Owned(window_id)));
}

void MediaStreamUIProxy::OnWindowId(const WindowIdCallback& window_id_callback,
                                    gfx::NativeViewId* window_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!window_id_callback.is_null())
    window_id_callback.Run(*window_id);
}

void MediaStreamUIProxy::ProcessAccessRequestResponse(
    const MediaStreamDevices& devices,
    content::MediaStreamRequestResult result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!response_callback_.is_null());

  ResponseCallback cb = response_callback_;
  response_callback_.Reset();
  cb.Run(devices, result);
}

void MediaStreamUIProxy::ProcessStopRequestFromUI() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!stop_callback_.is_null());

  base::Closure cb = stop_callback_;
  stop_callback_.Reset();
  cb.Run();
}

FakeMediaStreamUIProxy::FakeMediaStreamUIProxy()
  : MediaStreamUIProxy(NULL) {
}

FakeMediaStreamUIProxy::~FakeMediaStreamUIProxy() {}

void FakeMediaStreamUIProxy::SetAvailableDevices(
    const MediaStreamDevices& devices) {
  devices_ = devices;
}

void FakeMediaStreamUIProxy::RequestAccess(
    const MediaStreamRequest& request,
    const ResponseCallback& response_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  response_callback_ = response_callback;

  if (CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kUseFakeUIForMediaStream) == "deny") {
    // Immediately deny the request.
    BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(&MediaStreamUIProxy::ProcessAccessRequestResponse,
                     weak_factory_.GetWeakPtr(),
                     MediaStreamDevices(),
                     MEDIA_DEVICE_PERMISSION_DENIED));
    return;
  }

  MediaStreamDevices devices_to_use;
  bool accepted_audio = false;
  bool accepted_video = false;

  // Use the first capture device of the same media type in the list for the
  // fake UI.
  for (MediaStreamDevices::const_iterator it = devices_.begin();
       it != devices_.end(); ++it) {
    if (!accepted_audio &&
        IsAudioInputMediaType(request.audio_type) &&
        IsAudioInputMediaType(it->type) &&
        (request.requested_audio_device_id.empty() ||
         request.requested_audio_device_id == it->id)) {
      devices_to_use.push_back(*it);
      accepted_audio = true;
    } else if (!accepted_video &&
               IsVideoMediaType(request.video_type) &&
               IsVideoMediaType(it->type) &&
               (request.requested_video_device_id.empty() ||
                request.requested_video_device_id == it->id)) {
      devices_to_use.push_back(*it);
      accepted_video = true;
    }
  }

  // Fail the request if a device exist for the requested type.
  if ((request.audio_type != MEDIA_NO_SERVICE && !accepted_audio) ||
      (request.video_type != MEDIA_NO_SERVICE && !accepted_video)) {
    devices_to_use.clear();
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&MediaStreamUIProxy::ProcessAccessRequestResponse,
                 weak_factory_.GetWeakPtr(),
                 devices_to_use,
                 devices_to_use.empty() ?
                     MEDIA_DEVICE_NO_HARDWARE :
                     MEDIA_DEVICE_OK));
}

void FakeMediaStreamUIProxy::OnStarted(
    const base::Closure& stop_callback,
    const WindowIdCallback& window_id_callback) {}

}  // namespace content
