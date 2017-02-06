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
  empty_capabilities->zoom = media::mojom::Range::New();
  cb.Run(std::move(empty_capabilities));
}

void RunTakePhotoCallbackOnUIThread(
    const ImageCaptureImpl::TakePhotoCallback& callback,
    mojo::String mime_type,
    mojo::Array<uint8_t> data) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(callback, mime_type, base::Passed(std::move(data))));
}

void RunFailedTakePhotoCallback(const ImageCaptureImpl::TakePhotoCallback& cb) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  cb.Run("", mojo::Array<uint8_t>());
}

void GetCapabilitiesOnIOThread(
    const mojo::String& source_id,
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

void TakePhotoOnIOThread(
    const mojo::String& source_id,
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

// static
void ImageCaptureImpl::Create(
    mojo::InterfaceRequest<media::mojom::ImageCapture> request) {
  // |binding_| will take ownership of ImageCaptureImpl.
  new ImageCaptureImpl(std::move(request));
}

ImageCaptureImpl::~ImageCaptureImpl() {}

void ImageCaptureImpl::GetCapabilities(
    const mojo::String& source_id,
    const GetCapabilitiesCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  media::ScopedResultCallback<GetCapabilitiesCallback> scoped_callback(
      base::Bind(&RunGetCapabilitiesCallbackOnUIThread, callback),
      media::BindToCurrentLoop(base::Bind(&RunFailedGetCapabilitiesCallback)));

  // BrowserMainLoop::GetInstance() can only be called on UI thread.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&GetCapabilitiesOnIOThread, source_id,
                 BrowserMainLoop::GetInstance()->media_stream_manager(),
                 base::Passed(&scoped_callback)));
}

void ImageCaptureImpl::SetOptions(const mojo::String& source_id,
                                  media::mojom::PhotoSettingsPtr settings,
                                  const SetOptionsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(mcasas): This is just a stub and needs wiring to VideoCaptureManager
  // etc, see https://crbug.com/518807.
  callback.Run(false);
}

void ImageCaptureImpl::TakePhoto(const mojo::String& source_id,
                                 const TakePhotoCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  media::ScopedResultCallback<TakePhotoCallback> scoped_callback(
      base::Bind(&RunTakePhotoCallbackOnUIThread, callback),
      media::BindToCurrentLoop(base::Bind(&RunFailedTakePhotoCallback)));

  // BrowserMainLoop::GetInstance() can only be called on UI thread.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&TakePhotoOnIOThread, source_id,
                 BrowserMainLoop::GetInstance()->media_stream_manager(),
                 base::Passed(&scoped_callback)));
}

ImageCaptureImpl::ImageCaptureImpl(
    mojo::InterfaceRequest<media::mojom::ImageCapture> request)
    : binding_(this, std::move(request)) {}

}  // namespace content
