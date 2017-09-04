// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/image_capture_impl.h"

#include <utility>

#include "base/bind_helpers.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/common/media/media_stream_options.h"
#include "content/public/browser/browser_thread.h"
#include "media/base/bind_to_current_loop.h"
#include "media/capture/video/video_capture_device.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace content {

namespace {

void RunGetCapabilitiesCallbackOnUIThread(
    const ImageCaptureImpl::GetCapabilitiesCallback& callback,
    media::mojom::PhotoCapabilitiesPtr capabilities) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(callback, base::Passed(&capabilities)));
}

void RunFailedGetCapabilitiesCallback(
    const ImageCaptureImpl::GetCapabilitiesCallback& cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  media::mojom::PhotoCapabilitiesPtr empty_capabilities =
      media::mojom::PhotoCapabilities::New();
  empty_capabilities->iso = media::mojom::Range::New();
  empty_capabilities->width = media::mojom::Range::New();
  empty_capabilities->height = media::mojom::Range::New();
  empty_capabilities->zoom = media::mojom::Range::New();
  empty_capabilities->exposure_compensation = media::mojom::Range::New();
  empty_capabilities->color_temperature = media::mojom::Range::New();
  empty_capabilities->brightness = media::mojom::Range::New();
  empty_capabilities->contrast = media::mojom::Range::New();
  empty_capabilities->saturation = media::mojom::Range::New();
  empty_capabilities->sharpness = media::mojom::Range::New();
  cb.Run(std::move(empty_capabilities));
}

void RunSetOptionsCallbackOnUIThread(
    const ImageCaptureImpl::SetOptionsCallback& callback,
    bool success) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(callback, success));
}

void RunFailedSetOptionsCallback(
    const ImageCaptureImpl::SetOptionsCallback& cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  cb.Run(false);
}

void RunTakePhotoCallbackOnUIThread(
    const ImageCaptureImpl::TakePhotoCallback& callback,
    media::mojom::BlobPtr blob) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(callback, base::Passed(std::move(blob))));
}

void RunFailedTakePhotoCallback(const ImageCaptureImpl::TakePhotoCallback& cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  cb.Run(media::mojom::Blob::New());
}

void GetCapabilitiesOnIOThread(
    const std::string& source_id,
    MediaStreamManager* media_stream_manager,
    media::ScopedResultCallback<ImageCaptureImpl::GetCapabilitiesCallback>
        callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const int session_id =
      media_stream_manager->VideoDeviceIdToSessionId(source_id);

  if (session_id == StreamDeviceInfo::kNoId)
    return;
  media_stream_manager->video_capture_manager()->GetPhotoCapabilities(
      session_id, std::move(callback));
}

void SetOptionsOnIOThread(
    const std::string& source_id,
    MediaStreamManager* media_stream_manager,
    media::mojom::PhotoSettingsPtr settings,
    media::ScopedResultCallback<ImageCaptureImpl::SetOptionsCallback>
        callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const int session_id =
      media_stream_manager->VideoDeviceIdToSessionId(source_id);

  if (session_id == StreamDeviceInfo::kNoId)
    return;
  media_stream_manager->video_capture_manager()->SetPhotoOptions(
      session_id, std::move(settings), std::move(callback));
}

void TakePhotoOnIOThread(
    const std::string& source_id,
    MediaStreamManager* media_stream_manager,
    media::ScopedResultCallback<ImageCaptureImpl::TakePhotoCallback> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const int session_id =
      media_stream_manager->VideoDeviceIdToSessionId(source_id);

  if (session_id == StreamDeviceInfo::kNoId)
    return;
  media_stream_manager->video_capture_manager()->TakePhoto(session_id,
                                                           std::move(callback));
}

}  // anonymous namespace

ImageCaptureImpl::ImageCaptureImpl() {}

ImageCaptureImpl::~ImageCaptureImpl() {}

// static
void ImageCaptureImpl::Create(media::mojom::ImageCaptureRequest request) {
  mojo::MakeStrongBinding(base::MakeUnique<ImageCaptureImpl>(),
                          std::move(request));
}

void ImageCaptureImpl::GetCapabilities(
    const std::string& source_id,
    const GetCapabilitiesCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  media::ScopedResultCallback<GetCapabilitiesCallback> scoped_callback(
      base::Bind(&RunGetCapabilitiesCallbackOnUIThread, callback),
      media::BindToCurrentLoop(base::Bind(&RunFailedGetCapabilitiesCallback)));

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&GetCapabilitiesOnIOThread, source_id,
                 BrowserMainLoop::GetInstance()->media_stream_manager(),
                 base::Passed(&scoped_callback)));
}

void ImageCaptureImpl::SetOptions(const std::string& source_id,
                                  media::mojom::PhotoSettingsPtr settings,
                                  const SetOptionsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  media::ScopedResultCallback<SetOptionsCallback> scoped_callback(
      base::Bind(&RunSetOptionsCallbackOnUIThread, callback),
      media::BindToCurrentLoop(base::Bind(&RunFailedSetOptionsCallback)));

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SetOptionsOnIOThread, source_id,
                 BrowserMainLoop::GetInstance()->media_stream_manager(),
                 base::Passed(&settings), base::Passed(&scoped_callback)));
}

void ImageCaptureImpl::TakePhoto(const std::string& source_id,
                                 const TakePhotoCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  media::ScopedResultCallback<TakePhotoCallback> scoped_callback(
      base::Bind(&RunTakePhotoCallbackOnUIThread, callback),
      media::BindToCurrentLoop(base::Bind(&RunFailedTakePhotoCallback)));

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&TakePhotoOnIOThread, source_id,
                 BrowserMainLoop::GetInstance()->media_stream_manager(),
                 base::Passed(&scoped_callback)));
}

}  // namespace content
