// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/content_video_capture_device_core.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "content/public/browser/browser_thread.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/video/capture/video_capture_types.h"
#include "ui/gfx/rect.h"

namespace content {

namespace {

void DeleteCaptureMachineOnUIThread(
    scoped_ptr<VideoCaptureMachine> capture_machine) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  capture_machine.reset();
}

}  // namespace

ThreadSafeCaptureOracle::ThreadSafeCaptureOracle(
    scoped_ptr<media::VideoCaptureDevice::Client> client,
    scoped_ptr<VideoCaptureOracle> oracle,
    const media::VideoCaptureParams& params)
    : client_(client.Pass()),
      oracle_(oracle.Pass()),
      params_(params),
      capture_size_updated_(false) {
  switch (params_.requested_format.pixel_format) {
    case media::PIXEL_FORMAT_I420:
      video_frame_format_ = media::VideoFrame::I420;
      break;
    case media::PIXEL_FORMAT_TEXTURE:
      video_frame_format_ = media::VideoFrame::NATIVE_TEXTURE;
      break;
    default:
      LOG(FATAL) << "Unexpected pixel_format "
                 << params_.requested_format.pixel_format;
  }
}

ThreadSafeCaptureOracle::~ThreadSafeCaptureOracle() {}

bool ThreadSafeCaptureOracle::ObserveEventAndDecideCapture(
    VideoCaptureOracle::Event event,
    base::TimeTicks event_time,
    scoped_refptr<media::VideoFrame>* storage,
    CaptureFrameCallback* callback) {
  base::AutoLock guard(lock_);

  if (!client_)
    return false;  // Capture is stopped.

  scoped_refptr<media::VideoCaptureDevice::Client::Buffer> output_buffer =
      client_->ReserveOutputBuffer(video_frame_format_,
                                   params_.requested_format.frame_size);
  const bool should_capture =
      oracle_->ObserveEventAndDecideCapture(event, event_time);
  const bool content_is_dirty =
      (event == VideoCaptureOracle::kCompositorUpdate ||
       event == VideoCaptureOracle::kSoftwarePaint);
  const char* event_name =
      (event == VideoCaptureOracle::kTimerPoll ? "poll" :
       (event == VideoCaptureOracle::kCompositorUpdate ? "gpu" :
       "paint"));

  // Consider the various reasons not to initiate a capture.
  if (should_capture && !output_buffer) {
    TRACE_EVENT_INSTANT1("mirroring",
                         "EncodeLimited",
                         TRACE_EVENT_SCOPE_THREAD,
                         "trigger",
                         event_name);
    return false;
  } else if (!should_capture && output_buffer) {
    if (content_is_dirty) {
      // This is a normal and acceptable way to drop a frame. We've hit our
      // capture rate limit: for example, the content is animating at 60fps but
      // we're capturing at 30fps.
      TRACE_EVENT_INSTANT1("mirroring", "FpsRateLimited",
                           TRACE_EVENT_SCOPE_THREAD,
                           "trigger", event_name);
    }
    return false;
  } else if (!should_capture && !output_buffer) {
    // We decided not to capture, but we wouldn't have been able to if we wanted
    // to because no output buffer was available.
    TRACE_EVENT_INSTANT1("mirroring", "NearlyEncodeLimited",
                         TRACE_EVENT_SCOPE_THREAD,
                         "trigger", event_name);
    return false;
  }
  int frame_number = oracle_->RecordCapture();
  TRACE_EVENT_ASYNC_BEGIN2("mirroring", "Capture", output_buffer.get(),
                           "frame_number", frame_number,
                           "trigger", event_name);
  // NATIVE_TEXTURE frames wrap a texture mailbox, which we don't have at the
  // moment.  We do not construct those frames.
  if (video_frame_format_ != media::VideoFrame::NATIVE_TEXTURE) {
    *storage = media::VideoFrame::WrapExternalPackedMemory(
        video_frame_format_,
        params_.requested_format.frame_size,
        gfx::Rect(params_.requested_format.frame_size),
        params_.requested_format.frame_size,
        static_cast<uint8*>(output_buffer->data()),
        output_buffer->size(),
        base::SharedMemory::NULLHandle(),
        base::TimeDelta(),
        base::Closure());
  }
  *callback = base::Bind(&ThreadSafeCaptureOracle::DidCaptureFrame,
                         this,
                         frame_number,
                         output_buffer);
  return true;
}

gfx::Size ThreadSafeCaptureOracle::GetCaptureSize() const {
  base::AutoLock guard(lock_);
  return params_.requested_format.frame_size;
}

void ThreadSafeCaptureOracle::UpdateCaptureSize(const gfx::Size& source_size) {
  base::AutoLock guard(lock_);

  // If this is the first call to UpdateCaptureSize(), or the receiver supports
  // variable resolution, then determine the capture size by treating the
  // requested width and height as maxima.
  if (!capture_size_updated_ || params_.allow_resolution_change) {
    // The capture resolution should not exceed the source frame size.
    // In other words it should downscale the image but not upscale it.
    if (source_size.width() > params_.requested_format.frame_size.width() ||
        source_size.height() > params_.requested_format.frame_size.height()) {
      gfx::Rect capture_rect = media::ComputeLetterboxRegion(
          gfx::Rect(params_.requested_format.frame_size), source_size);
      params_.requested_format.frame_size.SetSize(
          MakeEven(capture_rect.width()), MakeEven(capture_rect.height()));
    } else {
      params_.requested_format.frame_size.SetSize(
          MakeEven(source_size.width()), MakeEven(source_size.height()));
    }
    capture_size_updated_ = true;
  }
}

void ThreadSafeCaptureOracle::Stop() {
  base::AutoLock guard(lock_);
  client_.reset();
}

void ThreadSafeCaptureOracle::ReportError(const std::string& reason) {
  base::AutoLock guard(lock_);
  if (client_)
    client_->OnError(reason);
}

void ThreadSafeCaptureOracle::DidCaptureFrame(
    int frame_number,
    const scoped_refptr<media::VideoCaptureDevice::Client::Buffer>& buffer,
    const scoped_refptr<media::VideoFrame>& frame,
    base::TimeTicks timestamp,
    bool success) {
  base::AutoLock guard(lock_);
  TRACE_EVENT_ASYNC_END2("mirroring", "Capture", buffer.get(),
                         "success", success,
                         "timestamp", timestamp.ToInternalValue());

  if (!client_)
    return;  // Capture is stopped.

  if (success) {
    if (oracle_->CompleteCapture(frame_number, timestamp)) {
      media::VideoCaptureFormat format = params_.requested_format;
      format.frame_size = frame->coded_size();
      client_->OnIncomingCapturedVideoFrame(buffer, format, frame, timestamp);
    }
  }
}

void ContentVideoCaptureDeviceCore::AllocateAndStart(
    const media::VideoCaptureParams& params,
    scoped_ptr<media::VideoCaptureDevice::Client> client) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ != kIdle) {
    DVLOG(1) << "Allocate() invoked when not in state Idle.";
    return;
  }

  if (params.requested_format.frame_rate <= 0) {
    std::string error_msg("Invalid frame_rate: ");
    error_msg += base::DoubleToString(params.requested_format.frame_rate);
    DVLOG(1) << error_msg;
    client->OnError(error_msg);
    return;
  }

  if (params.requested_format.pixel_format != media::PIXEL_FORMAT_I420 &&
      params.requested_format.pixel_format != media::PIXEL_FORMAT_TEXTURE) {
    std::string error_msg = base::StringPrintf(
        "unsupported format: %d", params.requested_format.pixel_format);
    DVLOG(1) << error_msg;
    client->OnError(error_msg);
    return;
  }

   if (params.requested_format.frame_size.width() < kMinFrameWidth ||
       params.requested_format.frame_size.height() < kMinFrameHeight) {
     std::string error_msg =
         "invalid frame size: " + params.requested_format.frame_size.ToString();
     DVLOG(1) << error_msg;
     client->OnError(error_msg);
     return;
   }

  media::VideoCaptureParams new_params = params;
  // Frame dimensions must each be an even integer since the client wants (or
  // will convert to) YUV420.
  new_params.requested_format.frame_size.SetSize(
      MakeEven(params.requested_format.frame_size.width()),
      MakeEven(params.requested_format.frame_size.height()));

  base::TimeDelta capture_period = base::TimeDelta::FromMicroseconds(
      1000000.0 / params.requested_format.frame_rate + 0.5);

  scoped_ptr<VideoCaptureOracle> oracle(
      new VideoCaptureOracle(capture_period,
                             kAcceleratedSubscriberIsSupported));
  oracle_proxy_ =
      new ThreadSafeCaptureOracle(client.Pass(), oracle.Pass(), new_params);

  // Starts the capture machine asynchronously.
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&VideoCaptureMachine::Start,
                 base::Unretained(capture_machine_.get()),
                 oracle_proxy_,
                 new_params),
      base::Bind(&ContentVideoCaptureDeviceCore::CaptureStarted, AsWeakPtr()));

  TransitionStateTo(kCapturing);
}

void ContentVideoCaptureDeviceCore::StopAndDeAllocate() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ != kCapturing)
    return;

  oracle_proxy_->Stop();
  oracle_proxy_ = NULL;

  TransitionStateTo(kIdle);

  // Stops the capture machine asynchronously.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE, base::Bind(
          &VideoCaptureMachine::Stop,
          base::Unretained(capture_machine_.get()),
          base::Bind(&base::DoNothing)));
}

void ContentVideoCaptureDeviceCore::CaptureStarted(bool success) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!success) {
    std::string reason("Failed to start capture machine.");
    DVLOG(1) << reason;
    Error(reason);
  }
}

ContentVideoCaptureDeviceCore::ContentVideoCaptureDeviceCore(
    scoped_ptr<VideoCaptureMachine> capture_machine)
    : state_(kIdle),
      capture_machine_(capture_machine.Pass()) {}

ContentVideoCaptureDeviceCore::~ContentVideoCaptureDeviceCore() {
  // If capture_machine is not NULL, then we need to return to the UI thread to
  // safely stop the capture machine.
  if (capture_machine_) {
    VideoCaptureMachine* capture_machine_ptr = capture_machine_.get();
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&VideoCaptureMachine::Stop,
                   base::Unretained(capture_machine_ptr),
                   base::Bind(&DeleteCaptureMachineOnUIThread,
                              base::Passed(&capture_machine_))));
  }
  DVLOG(1) << "ContentVideoCaptureDeviceCore@" << this << " destroying.";
}

void ContentVideoCaptureDeviceCore::TransitionStateTo(State next_state) {
  DCHECK(thread_checker_.CalledOnValidThread());

#ifndef NDEBUG
  static const char* kStateNames[] = {
    "Idle", "Allocated", "Capturing", "Error"
  };
  DVLOG(1) << "State change: " << kStateNames[state_]
           << " --> " << kStateNames[next_state];
#endif

  state_ = next_state;
}

void ContentVideoCaptureDeviceCore::Error(const std::string& reason) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ == kIdle)
    return;

  if (oracle_proxy_)
    oracle_proxy_->ReportError(reason);

  StopAndDeAllocate();
  TransitionStateTo(kError);
}

}  // namespace content
