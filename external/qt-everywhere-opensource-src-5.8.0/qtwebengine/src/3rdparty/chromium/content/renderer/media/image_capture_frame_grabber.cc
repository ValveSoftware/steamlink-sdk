// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/image_capture_frame_grabber.h"

#include "media/base/bind_to_current_loop.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/platform/WebCallbacks.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace content {

using blink::WebImageCaptureGrabFrameCallbacks;

namespace {

void OnError(std::unique_ptr<WebImageCaptureGrabFrameCallbacks> callbacks) {
  callbacks->onError();
}

// This internal method receives a |frame| and converts its pixels into a
// SkImage via an internal SkSurface and SkPixmap. Alpha channel, if any, is
// copied.
void OnVideoFrame(const ImageCaptureFrameGrabber::SkImageDeliverCB& callback,
                  const scoped_refptr<media::VideoFrame>& frame,
                  base::TimeTicks /* current_time */) {
  DCHECK(frame->format() == media::PIXEL_FORMAT_YV12 ||
         frame->format() == media::PIXEL_FORMAT_I420 ||
         frame->format() == media::PIXEL_FORMAT_YV12A);

  const SkAlphaType alpha = media::IsOpaque(frame->format())
                                ? kOpaque_SkAlphaType
                                : kPremul_SkAlphaType;
  const SkImageInfo info = SkImageInfo::MakeN32(
      frame->visible_rect().width(), frame->visible_rect().height(), alpha);

  sk_sp<SkSurface> surface = SkSurface::MakeRaster(info);
  DCHECK(surface);

  SkPixmap pixmap;
  if (!skia::GetWritablePixels(surface->getCanvas(), &pixmap)) {
    DLOG(ERROR) << "Error trying to map SkSurface's pixels";
    callback.Run(sk_sp<SkImage>());
    return;
  }

  libyuv::I420ToARGB(frame->visible_data(media::VideoFrame::kYPlane),
                     frame->stride(media::VideoFrame::kYPlane),
                     frame->visible_data(media::VideoFrame::kUPlane),
                     frame->stride(media::VideoFrame::kUPlane),
                     frame->visible_data(media::VideoFrame::kVPlane),
                     frame->stride(media::VideoFrame::kVPlane),
                     static_cast<uint8*>(pixmap.writable_addr()),
                     pixmap.width() * 4, pixmap.width(), pixmap.height());

  if (frame->format() == media::PIXEL_FORMAT_YV12A) {
    DCHECK(!info.isOpaque());
    // This function copies any plane into the alpha channel of an ARGB image.
    libyuv::ARGBCopyYToAlpha(frame->visible_data(media::VideoFrame::kAPlane),
                             frame->stride(media::VideoFrame::kAPlane),
                             static_cast<uint8*>(pixmap.writable_addr()),
                             pixmap.width() * 4, pixmap.width(),
                             pixmap.height());
  }

  callback.Run(surface->makeImageSnapshot());
}

}  // anonymous namespace

ImageCaptureFrameGrabber::ImageCaptureFrameGrabber() : weak_factory_(this) {}

ImageCaptureFrameGrabber::~ImageCaptureFrameGrabber() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void ImageCaptureFrameGrabber::grabFrame(
    blink::WebMediaStreamTrack* track,
    WebImageCaptureGrabFrameCallbacks* callbacks) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!!callbacks);

  DCHECK(track && !track->isNull() && track->getTrackData());
  DCHECK_EQ(blink::WebMediaStreamSource::TypeVideo, track->source().getType());

  ScopedWebCallbacks<WebImageCaptureGrabFrameCallbacks> scoped_callbacks =
      make_scoped_web_callbacks(callbacks, base::Bind(&OnError));

  // ConnectToTrack() must happen on render's Main Thread, whereas VideoFrames
  // are delivered on a background thread though, so we Bind the callback to our
  // current thread.
  MediaStreamVideoSink::ConnectToTrack(
      *track,
      base::Bind(&OnVideoFrame, media::BindToCurrentLoop(base::Bind(
                                    &ImageCaptureFrameGrabber::OnSkImage,
                                    weak_factory_.GetWeakPtr(),
                                    base::Passed(&scoped_callbacks)))),
      false);
}

void ImageCaptureFrameGrabber::OnSkImage(
    ScopedWebCallbacks<WebImageCaptureGrabFrameCallbacks> callbacks,
    sk_sp<SkImage> image) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  MediaStreamVideoSink::DisconnectFromTrack();
  if (image)
    callbacks.PassCallbacks()->onSuccess(image);
  else
    callbacks.PassCallbacks()->onError();
}

}  // namespace content
