// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_video_capturer_source.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "content/common/media/media_stream_messages.h"
#include "content/public/common/media_stream_request.h"
#include "content/renderer/media/media_stream_constraints_util.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "content/renderer/render_thread_impl.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/limits.h"
#include "media/base/video_capturer_source.h"
#include "media/base/video_frame.h"

namespace content {

namespace {

// Resolutions used if the source doesn't support capability enumeration.
struct {
  int width;
  int height;
} const kVideoResolutions[] = {{1920, 1080},
                               {1280, 720},
                               {960, 720},
                               {640, 480},
                               {640, 360},
                               {320, 240},
                               {320, 180}};

// Frame rates for sources with no support for capability enumeration.
const int kVideoFrameRates[] = {30, 60};

// Hard upper-bound frame rate for tab/desktop capture.
const double kMaxScreenCastFrameRate = 120.0;

// Allows the user to Override default power line frequency.
const char kPowerLineFrequency[] = "googPowerLineFrequency";

// Returns true if the value for width or height is reasonable.
bool DimensionValueIsValid(int x) {
  return x > 0 && x <= media::limits::kMaxDimension;
}

// Returns true if the value for frame rate is reasonable.
bool FrameRateValueIsValid(double frame_rate) {
  return (frame_rate > (1.0 / 60.0)) &&  // Lower-bound: One frame per minute.
      (frame_rate <= media::limits::kMaxFramesPerSecond);
}

// Returns true if the aspect ratio of |a| and |b| are equivalent to two
// significant digits.
bool AreNearlyEquivalentInAspectRatio(const gfx::Size& a, const gfx::Size& b) {
  DCHECK(!a.IsEmpty());
  DCHECK(!b.IsEmpty());
  const int aspect_ratio_a = (100 * a.width()) / a.height();
  const int aspect_ratio_b = (100 * b.width()) / b.height();
  return aspect_ratio_a == aspect_ratio_b;
}

// Checks if |device_info|s type is a generated content, e.g. Tab or Desktop.
bool IsContentVideoCaptureDevice(const StreamDeviceInfo& device_info) {
  return device_info.device.type == MEDIA_TAB_VIDEO_CAPTURE ||
         device_info.device.type == MEDIA_DESKTOP_VIDEO_CAPTURE;
}

// Interprets the properties in |constraints| to override values in |params| and
// determine the resolution change policy.
void SetContentCaptureParamsFromConstraints(
    const blink::WebMediaConstraints& constraints,
    MediaStreamType type,
    media::VideoCaptureParams* params) {
  // The default resolution change policies for tab versus desktop capture are
  // the way they are for legacy reasons.
  if (type == MEDIA_TAB_VIDEO_CAPTURE) {
    params->resolution_change_policy =
        media::RESOLUTION_POLICY_FIXED_RESOLUTION;
  } else if (type == MEDIA_DESKTOP_VIDEO_CAPTURE) {
    params->resolution_change_policy =
        media::RESOLUTION_POLICY_ANY_WITHIN_LIMIT;
  } else {
    NOTREACHED();
  }

  // If the maximum frame resolution was provided in the constraints, use it if
  // either: 1) none has been set yet; or 2) the maximum specificed is smaller
  // than the current setting.
  int width = 0;
  int height = 0;
  gfx::Size desired_max_frame_size;
  if (GetConstraintMaxAsInteger(
          constraints, &blink::WebMediaTrackConstraintSet::width, &width) &&
      GetConstraintMaxAsInteger(
          constraints, &blink::WebMediaTrackConstraintSet::height, &height) &&
      DimensionValueIsValid(width) && DimensionValueIsValid(height)) {
    desired_max_frame_size.SetSize(width, height);
    if (params->requested_format.frame_size.IsEmpty() ||
        desired_max_frame_size.width() <
            params->requested_format.frame_size.width() ||
        desired_max_frame_size.height() <
            params->requested_format.frame_size.height()) {
      params->requested_format.frame_size = desired_max_frame_size;
    }
  }

  // Set the default frame resolution if none was provided.
  if (params->requested_format.frame_size.IsEmpty()) {
    params->requested_format.frame_size.SetSize(
        MediaStreamVideoSource::kDefaultWidth,
        MediaStreamVideoSource::kDefaultHeight);
  }

  // If the maximum frame rate was provided, use it if either: 1) none has been
  // set yet; or 2) the maximum specificed is smaller than the current setting.
  double frame_rate = 0.0;
  if (GetConstraintMaxAsDouble(constraints,
                               &blink::WebMediaTrackConstraintSet::frameRate,
                               &frame_rate) &&
      FrameRateValueIsValid(frame_rate)) {
    if (params->requested_format.frame_rate <= 0.0f ||
        frame_rate < params->requested_format.frame_rate) {
      params->requested_format.frame_rate = frame_rate;
    }
  }

  // Set the default frame rate if none was provided.
  if (params->requested_format.frame_rate <= 0.0f) {
    params->requested_format.frame_rate =
        MediaStreamVideoSource::kDefaultFrameRate;
  }

  // If the minimum frame resolution was provided, compare it to the maximum
  // frame resolution to determine the intended resolution change policy.
  if (!desired_max_frame_size.IsEmpty() &&
      GetConstraintMinAsInteger(
          constraints, &blink::WebMediaTrackConstraintSet::width, &width) &&
      GetConstraintMinAsInteger(
          constraints, &blink::WebMediaTrackConstraintSet::height, &height) &&
      width <= desired_max_frame_size.width() &&
      height <= desired_max_frame_size.height()) {
    if (width == desired_max_frame_size.width() &&
        height == desired_max_frame_size.height()) {
      // Constraints explicitly require a single frame resolution.
      params->resolution_change_policy =
          media::RESOLUTION_POLICY_FIXED_RESOLUTION;
    } else if (DimensionValueIsValid(width) &&
               DimensionValueIsValid(height) &&
               AreNearlyEquivalentInAspectRatio(gfx::Size(width, height),
                                                desired_max_frame_size)) {
      // Constraints only mention a single aspect ratio.
      params->resolution_change_policy =
          media::RESOLUTION_POLICY_FIXED_ASPECT_RATIO;
    } else {
      // Constraints specify a minimum resolution that is smaller than the
      // maximum resolution and has a different aspect ratio (possibly even
      // 0x0). This indicates any frame resolution and aspect ratio is
      // acceptable.
      params->resolution_change_policy =
          media::RESOLUTION_POLICY_ANY_WITHIN_LIMIT;
    }
  }

  DVLOG(1) << __FUNCTION__ << " "
           << media::VideoCaptureFormat::ToString(params->requested_format)
           << " with resolution change policy "
           << params->resolution_change_policy;
}

// Interprets the properties in |constraints| to override values in |params| and
// determine the power line frequency.
void SetPowerLineFrequencyParamFromConstraints(
    const blink::WebMediaConstraints& constraints,
    media::VideoCaptureParams* params) {
  int freq;
  params->power_line_frequency = media::PowerLineFrequency::FREQUENCY_DEFAULT;
  if (!GetConstraintValueAsInteger(
          constraints,
          &blink::WebMediaTrackConstraintSet::googPowerLineFrequency, &freq)) {
    return;
  }
  if (freq == static_cast<int>(media::PowerLineFrequency::FREQUENCY_50HZ))
    params->power_line_frequency = media::PowerLineFrequency::FREQUENCY_50HZ;
  else if (freq == static_cast<int>(media::PowerLineFrequency::FREQUENCY_60HZ))
    params->power_line_frequency = media::PowerLineFrequency::FREQUENCY_60HZ;
}

// LocalVideoCapturerSource is a delegate used by MediaStreamVideoCapturerSource
// for local video capture. It uses the Render singleton VideoCaptureImplManager
// to start / stop and receive I420 frames from Chrome's video capture
// implementation. This is a main Render thread only object.
class LocalVideoCapturerSource final : public media::VideoCapturerSource {
 public:
  explicit LocalVideoCapturerSource(const StreamDeviceInfo& device_info);
  ~LocalVideoCapturerSource() override;

  // VideoCaptureDelegate Implementation.
  void GetCurrentSupportedFormats(
      int max_requested_width,
      int max_requested_height,
      double max_requested_frame_rate,
      const VideoCaptureDeviceFormatsCB& callback) override;
  void StartCapture(const media::VideoCaptureParams& params,
                    const VideoCaptureDeliverFrameCB& new_frame_callback,
                    const RunningCallback& running_callback) override;
  void RequestRefreshFrame() override;
  void StopCapture() override;

 private:
  void OnStateUpdate(VideoCaptureState state);
  void OnDeviceFormatsInUseReceived(const media::VideoCaptureFormats& formats);
  void OnDeviceSupportedFormatsEnumerated(
      const media::VideoCaptureFormats& formats);

  // |session_id_| identifies the capture device used for this capture session.
  const media::VideoCaptureSessionId session_id_;

  VideoCaptureImplManager* const manager_;

  const base::Closure release_device_cb_;

  // Indicates if we are capturing generated content, e.g. Tab or Desktop.
  const bool is_content_capture_;

  // These two are valid between StartCapture() and StopCapture().
  base::Closure stop_capture_cb_;
  RunningCallback running_callback_;

  // Placeholder keeping the callback between asynchronous device enumeration
  // calls.
  VideoCaptureDeviceFormatsCB formats_enumerated_callback_;

  // Bound to the main render thread.
  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<LocalVideoCapturerSource> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(LocalVideoCapturerSource);
};

}  // namespace

LocalVideoCapturerSource::LocalVideoCapturerSource(
    const StreamDeviceInfo& device_info)
    : session_id_(device_info.session_id),
      manager_(RenderThreadImpl::current()->video_capture_impl_manager()),
      release_device_cb_(manager_->UseDevice(session_id_)),
      is_content_capture_(IsContentVideoCaptureDevice(device_info)),
      weak_factory_(this) {
  DCHECK(RenderThreadImpl::current());
}

LocalVideoCapturerSource::~LocalVideoCapturerSource() {
  DCHECK(thread_checker_.CalledOnValidThread());
  release_device_cb_.Run();
}

void LocalVideoCapturerSource::GetCurrentSupportedFormats(
    int max_requested_width,
    int max_requested_height,
    double max_requested_frame_rate,
    const VideoCaptureDeviceFormatsCB& callback) {
  DVLOG(3) << "GetCurrentSupportedFormats({ max_requested_height = "
           << max_requested_height << "}) { max_requested_width = "
           << max_requested_width << "}) { max_requested_frame_rate = "
           << max_requested_frame_rate << "})";
  DCHECK(thread_checker_.CalledOnValidThread());

  if (is_content_capture_) {
    const int width = max_requested_width ?
        max_requested_width : MediaStreamVideoSource::kDefaultWidth;
    const int height = max_requested_height ?
        max_requested_height : MediaStreamVideoSource::kDefaultHeight;
    callback.Run(media::VideoCaptureFormats(
        1, media::VideoCaptureFormat(
               gfx::Size(width, height),
               static_cast<float>(
                   std::min(kMaxScreenCastFrameRate, max_requested_frame_rate)),
               media::PIXEL_FORMAT_I420)));
    return;
  }

  DCHECK(formats_enumerated_callback_.is_null());
  formats_enumerated_callback_ = callback;
  manager_->GetDeviceFormatsInUse(
      session_id_, media::BindToCurrentLoop(base::Bind(
                       &LocalVideoCapturerSource::OnDeviceFormatsInUseReceived,
                       weak_factory_.GetWeakPtr())));
}

void LocalVideoCapturerSource::StartCapture(
    const media::VideoCaptureParams& params,
    const VideoCaptureDeliverFrameCB& new_frame_callback,
    const RunningCallback& running_callback) {
  DCHECK(params.requested_format.IsValid());
  DCHECK(thread_checker_.CalledOnValidThread());
  running_callback_ = running_callback;

  stop_capture_cb_ = manager_->StartCapture(
      session_id_, params, media::BindToCurrentLoop(base::Bind(
                               &LocalVideoCapturerSource::OnStateUpdate,
                               weak_factory_.GetWeakPtr())),
      new_frame_callback);
}

void LocalVideoCapturerSource::RequestRefreshFrame() {
  DVLOG(3) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  if (stop_capture_cb_.is_null())
    return;  // Do not request frames if the source is stopped.
  manager_->RequestRefreshFrame(session_id_);
}

void LocalVideoCapturerSource::StopCapture() {
  DVLOG(3) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  // Immediately make sure we don't provide more frames.
  if (!stop_capture_cb_.is_null())
    base::ResetAndReturn(&stop_capture_cb_).Run();
  running_callback_.Reset();
  // Invalidate any potential format enumerations going on.
  formats_enumerated_callback_.Reset();
}

void LocalVideoCapturerSource::OnStateUpdate(VideoCaptureState state) {
  DVLOG(3) << __FUNCTION__ << " state = " << state;
  DCHECK(thread_checker_.CalledOnValidThread());
  if (running_callback_.is_null())
    return;
  const bool is_started_ok = state == VIDEO_CAPTURE_STATE_STARTED;
  running_callback_.Run(is_started_ok);
  if (!is_started_ok)
    running_callback_.Reset();
}

void LocalVideoCapturerSource::OnDeviceFormatsInUseReceived(
    const media::VideoCaptureFormats& formats_in_use) {
  DVLOG(3) << __FUNCTION__ << ", #formats received: " << formats_in_use.size();
  DCHECK(thread_checker_.CalledOnValidThread());
  // StopCapture() might have destroyed |formats_enumerated_callback_| before
  // arriving here.
  if (formats_enumerated_callback_.is_null())
    return;
  if (formats_in_use.size()) {
    base::ResetAndReturn(&formats_enumerated_callback_).Run(formats_in_use);
    return;
  }

  // The device doesn't seem to have formats in use so try and retrieve the
  // whole list of supported ones.
  manager_->GetDeviceSupportedFormats(
      session_id_,
      media::BindToCurrentLoop(
          base::Bind(
              &LocalVideoCapturerSource::OnDeviceSupportedFormatsEnumerated,
              weak_factory_.GetWeakPtr())));
}

void LocalVideoCapturerSource::OnDeviceSupportedFormatsEnumerated(
    const media::VideoCaptureFormats& formats) {
  DVLOG(3) << __FUNCTION__ << ", #formats received: " << formats.size();
  DCHECK(thread_checker_.CalledOnValidThread());
  // StopCapture() might have destroyed |formats_enumerated_callback_| before
  // arriving here.
  if (formats_enumerated_callback_.is_null())
    return;
  if (formats.size()) {
    base::ResetAndReturn(&formats_enumerated_callback_).Run(formats);
    return;
  }

  // The capture device doesn't seem to support capability enumeration, compose
  // a fallback list of capabilities.
  media::VideoCaptureFormats default_formats;
  for (const auto& resolution : kVideoResolutions) {
    for (const auto frame_rate : kVideoFrameRates) {
      default_formats.push_back(media::VideoCaptureFormat(
          gfx::Size(resolution.width, resolution.height), frame_rate,
          media::PIXEL_FORMAT_I420));
    }
  }
  base::ResetAndReturn(&formats_enumerated_callback_).Run(default_formats);
}

MediaStreamVideoCapturerSource::MediaStreamVideoCapturerSource(
    const SourceStoppedCallback& stop_callback,
    std::unique_ptr<media::VideoCapturerSource> source)
    : RenderFrameObserver(nullptr), source_(std::move(source)) {
  SetStopCallback(stop_callback);
}

MediaStreamVideoCapturerSource::MediaStreamVideoCapturerSource(
    const SourceStoppedCallback& stop_callback,
    const StreamDeviceInfo& device_info,
    RenderFrame* render_frame)
    : RenderFrameObserver(render_frame),
      source_(new LocalVideoCapturerSource(device_info)) {
  SetStopCallback(stop_callback);
  SetDeviceInfo(device_info);
}

MediaStreamVideoCapturerSource::~MediaStreamVideoCapturerSource() {
}

void MediaStreamVideoCapturerSource::RequestRefreshFrame() {
  source_->RequestRefreshFrame();
}

void MediaStreamVideoCapturerSource::SetCapturingLinkSecured(bool is_secure) {
  Send(new MediaStreamHostMsg_SetCapturingLinkSecured(
      device_info().session_id, device_info().device.type, is_secure));
}

void MediaStreamVideoCapturerSource::GetCurrentSupportedFormats(
    int max_requested_width,
    int max_requested_height,
    double max_requested_frame_rate,
    const VideoCaptureDeviceFormatsCB& callback) {
  source_->GetCurrentSupportedFormats(
      max_requested_width,
      max_requested_height,
      max_requested_frame_rate,
      callback);
}

void MediaStreamVideoCapturerSource::StartSourceImpl(
    const media::VideoCaptureFormat& format,
    const blink::WebMediaConstraints& constraints,
    const VideoCaptureDeliverFrameCB& frame_callback) {
  media::VideoCaptureParams new_params;
  new_params.requested_format = format;
  if (IsContentVideoCaptureDevice(device_info())) {
    SetContentCaptureParamsFromConstraints(
        constraints, device_info().device.type, &new_params);
  } else if (device_info().device.type == MEDIA_DEVICE_VIDEO_CAPTURE) {
    SetPowerLineFrequencyParamFromConstraints(constraints, &new_params);
  }

  source_->StartCapture(new_params,
                          frame_callback,
                          base::Bind(&MediaStreamVideoCapturerSource::OnStarted,
                                     base::Unretained(this)));
}

void MediaStreamVideoCapturerSource::StopSourceImpl() {
  source_->StopCapture();
}

void MediaStreamVideoCapturerSource::OnStarted(bool result) {
  OnStartDone(result ? MEDIA_DEVICE_OK : MEDIA_DEVICE_TRACK_START_FAILURE);
}

const char*
MediaStreamVideoCapturerSource::GetPowerLineFrequencyForTesting() const {
  return kPowerLineFrequency;
}

}  // namespace content
