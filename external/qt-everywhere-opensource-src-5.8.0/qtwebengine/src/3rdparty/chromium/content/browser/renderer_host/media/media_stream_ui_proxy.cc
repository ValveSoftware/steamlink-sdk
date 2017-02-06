// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_stream_ui_proxy.h"

#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_delegate.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "media/capture/video/fake_video_capture_device.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

GURL ConvertToGURL(const url::Origin& origin) {
  return origin.unique() ? GURL() : GURL(origin.Serialize());
}

}  // namespace

void SetAndCheckAncestorFlag(MediaStreamRequest* request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHostImpl* rfh =
      RenderFrameHostImpl::FromID(request->render_process_id,
                                  request->render_frame_id);

  if (rfh == NULL) {
    // RenderFrame destroyed before the request is handled?
    return;
  }
  FrameTreeNode* node = rfh->frame_tree_node();

  while (node->parent() != NULL) {
    if (!node->HasSameOrigin(*node->parent())) {
      request->all_ancestors_have_same_origin =  false;
      return;
    }
    node = node->parent();
  }
  request->all_ancestors_have_same_origin = true;
}

class MediaStreamUIProxy::Core {
 public:
  explicit Core(const base::WeakPtr<MediaStreamUIProxy>& proxy,
                RenderFrameHostDelegate* test_render_delegate);
  ~Core();

  void RequestAccess(std::unique_ptr<MediaStreamRequest> request);
  bool CheckAccess(const GURL& security_origin,
                   MediaStreamType type,
                   int process_id,
                   int frame_id);
  void OnStarted(gfx::NativeViewId* window_id);

 private:
  void ProcessAccessRequestResponse(const MediaStreamDevices& devices,
                                    content::MediaStreamRequestResult result,
                                    std::unique_ptr<MediaStreamUI> stream_ui);
  void ProcessStopRequestFromUI();
  RenderFrameHostDelegate* GetRenderFrameHostDelegate(int render_process_id,
                                                      int render_frame_id);

  base::WeakPtr<MediaStreamUIProxy> proxy_;
  std::unique_ptr<MediaStreamUI> ui_;

  RenderFrameHostDelegate* const test_render_delegate_;

  // WeakPtr<> is used to RequestMediaAccessPermission() because there is no way
  // cancel media requests.
  base::WeakPtrFactory<Core> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

MediaStreamUIProxy::Core::Core(const base::WeakPtr<MediaStreamUIProxy>& proxy,
                               RenderFrameHostDelegate* test_render_delegate)
    : proxy_(proxy),
      test_render_delegate_(test_render_delegate),
      weak_factory_(this) {
}

MediaStreamUIProxy::Core::~Core() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void MediaStreamUIProxy::Core::RequestAccess(
    std::unique_ptr<MediaStreamRequest> request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderFrameHostDelegate* render_delegate = GetRenderFrameHostDelegate(
      request->render_process_id, request->render_frame_id);

  // Tab may have gone away, or has no delegate from which to request access.
  if (!render_delegate) {
    ProcessAccessRequestResponse(MediaStreamDevices(),
                                 MEDIA_DEVICE_FAILED_DUE_TO_SHUTDOWN,
                                 std::unique_ptr<MediaStreamUI>());
    return;
  }
  SetAndCheckAncestorFlag(request.get());

  render_delegate->RequestMediaAccessPermission(
      *request, base::Bind(&Core::ProcessAccessRequestResponse,
                           weak_factory_.GetWeakPtr()));
}

bool MediaStreamUIProxy::Core::CheckAccess(const GURL& security_origin,
                                           MediaStreamType type,
                                           int render_process_id,
                                           int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderFrameHostDelegate* render_delegate =
      GetRenderFrameHostDelegate(render_process_id, render_frame_id);
  if (!render_delegate)
    return false;

  return render_delegate->CheckMediaAccessPermission(security_origin, type);
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
    std::unique_ptr<MediaStreamUI> stream_ui) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ui_ = std::move(stream_ui);
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

RenderFrameHostDelegate* MediaStreamUIProxy::Core::GetRenderFrameHostDelegate(
    int render_process_id,
    int render_frame_id) {
  if (test_render_delegate_)
    return test_render_delegate_;
  RenderFrameHostImpl* host =
      RenderFrameHostImpl::FromID(render_process_id, render_frame_id);
  return host ? host->delegate() : NULL;
}

// static
std::unique_ptr<MediaStreamUIProxy> MediaStreamUIProxy::Create() {
  return std::unique_ptr<MediaStreamUIProxy>(new MediaStreamUIProxy(NULL));
}

// static
std::unique_ptr<MediaStreamUIProxy> MediaStreamUIProxy::CreateForTests(
    RenderFrameHostDelegate* render_delegate) {
  return std::unique_ptr<MediaStreamUIProxy>(
      new MediaStreamUIProxy(render_delegate));
}

MediaStreamUIProxy::MediaStreamUIProxy(
    RenderFrameHostDelegate* test_render_delegate)
    : weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  core_.reset(new Core(weak_factory_.GetWeakPtr(), test_render_delegate));
}

MediaStreamUIProxy::~MediaStreamUIProxy() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void MediaStreamUIProxy::RequestAccess(
    std::unique_ptr<MediaStreamRequest> request,
    const ResponseCallback& response_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  response_callback_ = response_callback;
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&Core::RequestAccess, base::Unretained(core_.get()),
                 base::Passed(&request)));
}

void MediaStreamUIProxy::CheckAccess(
    const url::Origin& security_origin,
    MediaStreamType type,
    int render_process_id,
    int render_frame_id,
    const base::Callback<void(bool)>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&Core::CheckAccess, base::Unretained(core_.get()),
                 ConvertToGURL(security_origin), type, render_process_id,
                 render_frame_id),
      base::Bind(&MediaStreamUIProxy::OnCheckedAccess,
                 weak_factory_.GetWeakPtr(), callback));
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

void MediaStreamUIProxy::OnWindowId(const WindowIdCallback& window_id_callback,
                                    gfx::NativeViewId* window_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!window_id_callback.is_null())
    window_id_callback.Run(*window_id);
}

void MediaStreamUIProxy::OnCheckedAccess(
    const base::Callback<void(bool)>& callback,
    bool have_access) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!callback.is_null())
    callback.Run(have_access);
}

FakeMediaStreamUIProxy::FakeMediaStreamUIProxy()
  : MediaStreamUIProxy(NULL),
    mic_access_(true),
    camera_access_(true) {
}

FakeMediaStreamUIProxy::~FakeMediaStreamUIProxy() {}

void FakeMediaStreamUIProxy::SetAvailableDevices(
    const MediaStreamDevices& devices) {
  devices_ = devices;
}

void FakeMediaStreamUIProxy::SetMicAccess(bool access) {
  mic_access_ = access;
}

void FakeMediaStreamUIProxy::SetCameraAccess(bool access) {
  camera_access_ = access;
}

void FakeMediaStreamUIProxy::RequestAccess(
    std::unique_ptr<MediaStreamRequest> request,
    const ResponseCallback& response_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  response_callback_ = response_callback;

  if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
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
        IsAudioInputMediaType(request->audio_type) &&
        IsAudioInputMediaType(it->type) &&
        (request->requested_audio_device_id.empty() ||
         request->requested_audio_device_id == it->id)) {
      devices_to_use.push_back(*it);
      accepted_audio = true;
    } else if (!accepted_video &&
               IsVideoMediaType(request->video_type) &&
               IsVideoMediaType(it->type) &&
               (request->requested_video_device_id.empty() ||
                request->requested_video_device_id == it->id)) {
      devices_to_use.push_back(*it);
      accepted_video = true;
    }
  }

  // Fail the request if a device doesn't exist for the requested type.
  if ((request->audio_type != MEDIA_NO_SERVICE && !accepted_audio) ||
      (request->video_type != MEDIA_NO_SERVICE && !accepted_video)) {
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

void FakeMediaStreamUIProxy::CheckAccess(
    const url::Origin& security_origin,
    MediaStreamType type,
    int render_process_id,
    int render_frame_id,
    const base::Callback<void(bool)>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(type == MEDIA_DEVICE_AUDIO_CAPTURE ||
         type == MEDIA_DEVICE_VIDEO_CAPTURE);

  bool have_access = false;
  if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kUseFakeUIForMediaStream) != "deny") {
    have_access =
        type == MEDIA_DEVICE_AUDIO_CAPTURE ? mic_access_ : camera_access_;
  }

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&MediaStreamUIProxy::OnCheckedAccess,
                 weak_factory_.GetWeakPtr(),
                 callback,
                 have_access));
  return;
}

void FakeMediaStreamUIProxy::OnStarted(
    const base::Closure& stop_callback,
    const WindowIdCallback& window_id_callback) {}

}  // namespace content
