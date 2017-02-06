// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_video_source.h"

#include <algorithm>
#include <limits>
#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "content/child/child_process.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/video_track_adapter.h"

namespace content {

namespace {

const char* const kLegalVideoConstraints[] = {
    "width",      "height",   "aspectRatio", "frameRate",
    "facingMode", "deviceId", "groupId",     "mediaStreamSource",
};

// Returns true if |constraint| has mandatory constraints.
bool HasMandatoryConstraints(const blink::WebMediaConstraints& constraints) {
  return constraints.basic().hasMandatory();
}

// Retrieve the desired max width and height from |constraints|. If not set,
// the |desired_width| and |desired_height| are set to
// std::numeric_limits<int>::max();
// If either max width or height is set as a mandatory constraint, the optional
// constraints are not checked.
void GetDesiredMaxWidthAndHeight(const blink::WebMediaConstraints& constraints,
                                 int* desired_width, int* desired_height) {
  *desired_width = std::numeric_limits<int>::max();
  *desired_height = std::numeric_limits<int>::max();

  const auto& basic_constraints = constraints.basic();
  if (basic_constraints.width.hasMax() || basic_constraints.height.hasMax()) {
    if (basic_constraints.width.hasMax())
      *desired_width = basic_constraints.width.max();
    if (basic_constraints.height.hasMax())
      *desired_height = basic_constraints.height.max();
    return;
  }

  for (const auto& constraint_set : constraints.advanced()) {
    if (constraint_set.width.hasMax())
      *desired_width = constraint_set.width.max();
    if (constraint_set.height.hasMax())
      *desired_height = constraint_set.height.max();
  }
}

// Retrieve the desired max and min aspect ratio from |constraints|. If not set,
// the |min_aspect_ratio| is set to 0 and |max_aspect_ratio| is set to
// std::numeric_limits<double>::max();
// If either min or max aspect ratio is set as a mandatory constraint, the
// optional constraints are not checked.
void GetDesiredMinAndMaxAspectRatio(
    const blink::WebMediaConstraints& constraints,
    double* min_aspect_ratio,
    double* max_aspect_ratio) {
  *min_aspect_ratio = 0;
  *max_aspect_ratio = std::numeric_limits<double>::max();

  if (constraints.basic().aspectRatio.hasMin() ||
      constraints.basic().aspectRatio.hasMax()) {
    if (constraints.basic().aspectRatio.hasMin())
      *min_aspect_ratio = constraints.basic().aspectRatio.min();
    if (constraints.basic().aspectRatio.hasMax())
      *max_aspect_ratio = constraints.basic().aspectRatio.max();
    return;
    // Note - the code will ignore attempts at successive refinement
    // of the aspect ratio with advanced constraint. This may be wrong.
  }
  // Note - the code below will potentially pick min and max from different
  // constraint sets, some of which might have been ignored.
  for (const auto& constraint_set : constraints.advanced()) {
    if (constraint_set.aspectRatio.hasMin()) {
      *min_aspect_ratio = constraint_set.aspectRatio.min();
      break;
    }
  }
  for (const auto& constraint_set : constraints.advanced()) {
    if (constraint_set.aspectRatio.hasMax()) {
      *max_aspect_ratio = constraint_set.aspectRatio.max();
      break;
    }
  }
}

// Returns true if |constraints| are fulfilled. |format| can be changed by a
// constraint, e.g. the frame rate can be changed by setting maxFrameRate.
bool UpdateFormatForConstraints(
    const blink::WebMediaTrackConstraintSet& constraints,
    media::VideoCaptureFormat* format,
    std::string* failing_constraint_name) {
  DCHECK(format != NULL);

  if (!format->IsValid())
    return false;

  // The width and height are matched based on cropping occuring later:
  // min width/height has to be >= the size of the frame (no upscale).
  // max width/height just has to be > 0 (we can crop anything too large).
  if ((constraints.width.hasMin() &&
       constraints.width.min() > format->frame_size.width()) ||
      (constraints.width.hasMax() && constraints.width.max() <= 0)) {
    *failing_constraint_name = constraints.width.name();
  } else if ((constraints.height.hasMin() &&
              constraints.height.min() > format->frame_size.height()) ||
             (constraints.height.hasMax() && constraints.height.max() <= 0)) {
    *failing_constraint_name = constraints.height.name();
  } else if (!constraints.frameRate.matches(format->frame_rate)) {
    if (constraints.frameRate.hasMax()) {
      const double value = constraints.frameRate.max();
      // TODO(hta): Check if handling of max = 0.0 is relevant.
      // (old handling was to set rate to 1.0 if 0.0 was specified)
      if (constraints.frameRate.matches(value)) {
        format->frame_rate =
            (format->frame_rate > value) ? value : format->frame_rate;
        return true;
      }
    }
    *failing_constraint_name = constraints.frameRate.name();
  } else {
    return true;
  }

  DCHECK(!failing_constraint_name->empty());
  return false;
}

// Removes media::VideoCaptureFormats from |formats| that don't meet
// |constraints|.
void FilterFormatsByConstraints(
    const blink::WebMediaTrackConstraintSet& constraints,
    media::VideoCaptureFormats* formats,
    std::string* failing_constraint_name) {
  media::VideoCaptureFormats::iterator format_it = formats->begin();
  while (format_it != formats->end()) {
    // Modify |format_it| to fulfill the constraint if possible.
    // Delete it otherwise.
    if (!UpdateFormatForConstraints(constraints, &(*format_it),
                                    failing_constraint_name)) {
      format_it = formats->erase(format_it);
    } else {
      ++format_it;
    }
  }
}

// Returns the media::VideoCaptureFormats that matches |constraints|.
// If the return value is empty, and the reason is a specific constraint,
// |unsatisfied_constraint| returns the name of the constraint.
media::VideoCaptureFormats FilterFormats(
    const blink::WebMediaConstraints& constraints,
    const media::VideoCaptureFormats& supported_formats,
    std::string* unsatisfied_constraint) {
  if (constraints.isNull())
    return supported_formats;

  const auto& basic = constraints.basic();

  // Do some checks that won't be done when filtering candidates.

  if (basic.width.hasMin() && basic.width.hasMax() &&
      basic.width.min() > basic.width.max()) {
    *unsatisfied_constraint = basic.width.name();
    return media::VideoCaptureFormats();
  }

  if (basic.height.hasMin() && basic.height.hasMax() &&
      basic.height.min() > basic.height.max()) {
    *unsatisfied_constraint = basic.height.name();
    return media::VideoCaptureFormats();
  }

  double max_aspect_ratio;
  double min_aspect_ratio;
  GetDesiredMinAndMaxAspectRatio(constraints,
                                 &min_aspect_ratio,
                                 &max_aspect_ratio);

  if (min_aspect_ratio > max_aspect_ratio || max_aspect_ratio < 0.05f) {
    DLOG(WARNING) << "Wrong requested aspect ratio: min " << min_aspect_ratio
                  << " max " << max_aspect_ratio;
    *unsatisfied_constraint = basic.aspectRatio.name();
    return media::VideoCaptureFormats();
  }

  std::vector<std::string> temp(
      &kLegalVideoConstraints[0],
      &kLegalVideoConstraints[sizeof(kLegalVideoConstraints) /
                              sizeof(kLegalVideoConstraints[0])]);
  std::string failing_name;
  if (basic.hasMandatoryOutsideSet(temp, failing_name)) {
    *unsatisfied_constraint = failing_name;
    return media::VideoCaptureFormats();
  }

  media::VideoCaptureFormats candidates = supported_formats;
  FilterFormatsByConstraints(basic, &candidates, unsatisfied_constraint);

  if (candidates.empty())
    return candidates;

  // Ok - all mandatory checked and we still have candidates.
  // Let's try filtering using the advanced constraints. The advanced
  // constraints must be filtered in the order they occur in |advanced|.
  // But if a constraint produce zero candidates, the constraint is ignored and
  // the next constraint is tested.
  // http://w3c.github.io/mediacapture-main/getusermedia.html#dfn-selectsettings
  for (const auto& constraint_set : constraints.advanced()) {
    media::VideoCaptureFormats current_candidates = candidates;
    std::string unsatisfied_constraint;
    FilterFormatsByConstraints(constraint_set, &current_candidates,
                               &unsatisfied_constraint);
    if (!current_candidates.empty())
      candidates = current_candidates;
  }

  // We have done as good as we can to filter the supported resolutions.
  return candidates;
}

media::VideoCaptureFormat GetBestFormatBasedOnArea(
    const media::VideoCaptureFormats& formats,
    int area) {
  DCHECK(!formats.empty());
  const media::VideoCaptureFormat* best_format = nullptr;
  int best_diff = std::numeric_limits<int>::max();
  for (const auto& format : formats) {
    const int diff = abs(area - format.frame_size.GetArea());
    if (diff < best_diff) {
      best_diff = diff;
      best_format = &format;
    }
  }
  DVLOG(3) << "GetBestFormatBasedOnArea chose format "
           << media::VideoCaptureFormat::ToString(*best_format);
  return *best_format;
}

// Find the format that best matches the default video size.
// This algorithm is chosen since a resolution must be picked even if no
// constraints are provided. We don't just select the maximum supported
// resolution since higher resolutions cost more in terms of complexity and
// many cameras have lower frame rate and have more noise in the image at
// their maximum supported resolution.
media::VideoCaptureFormat GetBestCaptureFormat(
    const media::VideoCaptureFormats& formats,
    const blink::WebMediaConstraints& constraints) {
  DCHECK(!formats.empty());

  int max_width;
  int max_height;
  GetDesiredMaxWidthAndHeight(constraints, &max_width, &max_height);
  const int area =
      std::min(max_width,
               static_cast<int>(MediaStreamVideoSource::kDefaultWidth)) *
      std::min(max_height,
               static_cast<int>(MediaStreamVideoSource::kDefaultHeight));

  return GetBestFormatBasedOnArea(formats, area);
}

}  // anonymous namespace

// static
MediaStreamVideoSource* MediaStreamVideoSource::GetVideoSource(
    const blink::WebMediaStreamSource& source) {
  if (source.isNull() ||
      source.getType() != blink::WebMediaStreamSource::TypeVideo) {
    return nullptr;
  }
  return static_cast<MediaStreamVideoSource*>(source.getExtraData());
}

MediaStreamVideoSource::MediaStreamVideoSource()
    : state_(NEW),
      track_adapter_(
          new VideoTrackAdapter(ChildProcess::current()->io_task_runner())),
      weak_factory_(this) {}

MediaStreamVideoSource::~MediaStreamVideoSource() {
  DCHECK(CalledOnValidThread());
}

void MediaStreamVideoSource::AddTrack(
    MediaStreamVideoTrack* track,
    const VideoCaptureDeliverFrameCB& frame_callback,
    const blink::WebMediaConstraints& constraints,
    const ConstraintsCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!constraints.isNull());
  DCHECK(std::find(tracks_.begin(), tracks_.end(), track) == tracks_.end());
  tracks_.push_back(track);
  secure_tracker_.Add(track, true);

  track_descriptors_.push_back(
      TrackDescriptor(track, frame_callback, constraints, callback));

  switch (state_) {
    case NEW: {
      // Tab capture and Screen capture needs the maximum requested height
      // and width to decide on the resolution.
      // NOTE: Optional constraints are deliberately ignored.
      int max_requested_width = 0;
      if (constraints.basic().width.hasMax())
        max_requested_width = constraints.basic().width.max();

      int max_requested_height = 0;
      if (constraints.basic().height.hasMax())
        max_requested_height = constraints.basic().height.max();

      double max_requested_frame_rate = kDefaultFrameRate;
      if (constraints.basic().frameRate.hasMax())
        max_requested_frame_rate = constraints.basic().frameRate.max();

      state_ = RETRIEVING_CAPABILITIES;
      GetCurrentSupportedFormats(
          max_requested_width,
          max_requested_height,
          max_requested_frame_rate,
          base::Bind(&MediaStreamVideoSource::OnSupportedFormats,
                     weak_factory_.GetWeakPtr()));

      break;
    }
    case STARTING:
    case RETRIEVING_CAPABILITIES: {
      // The |callback| will be triggered once the source has started or
      // the capabilities have been retrieved.
      break;
    }
    case ENDED:
    case STARTED: {
      // Currently, reconfiguring the source is not supported.
      FinalizeAddTrack();
    }
  }
}

void MediaStreamVideoSource::RemoveTrack(MediaStreamVideoTrack* video_track) {
  DCHECK(CalledOnValidThread());
  std::vector<MediaStreamVideoTrack*>::iterator it =
      std::find(tracks_.begin(), tracks_.end(), video_track);
  DCHECK(it != tracks_.end());
  tracks_.erase(it);
  secure_tracker_.Remove(video_track);

  for (std::vector<TrackDescriptor>::iterator it = track_descriptors_.begin();
       it != track_descriptors_.end(); ++it) {
    if (it->track == video_track) {
      track_descriptors_.erase(it);
      break;
    }
  }

  // Call |frame_adapter_->RemoveTrack| here even if adding the track has
  // failed and |frame_adapter_->AddCallback| has not been called.
  track_adapter_->RemoveTrack(video_track);

  if (tracks_.empty())
    StopSource();
}

void MediaStreamVideoSource::UpdateCapturingLinkSecure(
    MediaStreamVideoTrack* track,
    bool is_secure) {
  secure_tracker_.Update(track, is_secure);
  SetCapturingLinkSecured(secure_tracker_.is_capturing_secure());
}

base::SingleThreadTaskRunner* MediaStreamVideoSource::io_task_runner() const {
  DCHECK(CalledOnValidThread());
  return track_adapter_->io_task_runner();
}

const media::VideoCaptureFormat*
    MediaStreamVideoSource::GetCurrentFormat() const {
  DCHECK(CalledOnValidThread());
  if (state_ == STARTING || state_ == STARTED)
    return &current_format_;
  return nullptr;
}

void MediaStreamVideoSource::DoStopSource() {
  DCHECK(CalledOnValidThread());
  DVLOG(3) << "DoStopSource()";
  if (state_ == ENDED)
    return;
  track_adapter_->StopFrameMonitoring();
  StopSourceImpl();
  state_ = ENDED;
  SetReadyState(blink::WebMediaStreamSource::ReadyStateEnded);
}

void MediaStreamVideoSource::OnSupportedFormats(
    const media::VideoCaptureFormats& formats) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(RETRIEVING_CAPABILITIES, state_);

  supported_formats_ = formats;
  blink::WebMediaConstraints fulfilled_constraints;
  if (!FindBestFormatWithConstraints(supported_formats_,
                                     &current_format_,
                                     &fulfilled_constraints)) {
    SetReadyState(blink::WebMediaStreamSource::ReadyStateEnded);
    DVLOG(3) << "OnSupportedFormats failed to find an usable format";
    // This object can be deleted after calling FinalizeAddTrack. See comment
    // in the header file.
    FinalizeAddTrack();
    return;
  }

  state_ = STARTING;
  DVLOG(3) << "Starting the capturer with "
           << media::VideoCaptureFormat::ToString(current_format_);

  StartSourceImpl(
      current_format_,
      fulfilled_constraints,
      base::Bind(&VideoTrackAdapter::DeliverFrameOnIO, track_adapter_));
}

bool MediaStreamVideoSource::FindBestFormatWithConstraints(
    const media::VideoCaptureFormats& formats,
    media::VideoCaptureFormat* best_format,
    blink::WebMediaConstraints* fulfilled_constraints) {
  DCHECK(CalledOnValidThread());
  DVLOG(3) << "MediaStreamVideoSource::FindBestFormatWithConstraints "
           << "with " << formats.size() << " formats";
  // Find the first track descriptor that can fulfil the constraints.
  for (const auto& track : track_descriptors_) {
    const blink::WebMediaConstraints& track_constraints = track.constraints;

    // If the source doesn't support capability enumeration it is still ok if
    // no mandatory constraints have been specified. That just means that
    // we will start with whatever format is native to the source.
    if (formats.empty() && !HasMandatoryConstraints(track_constraints)) {
      DVLOG(3) << "No mandatory constraints and no formats";
      *fulfilled_constraints = track_constraints;
      *best_format = media::VideoCaptureFormat();
      return true;
    }
    std::string unsatisfied_constraint;
    const media::VideoCaptureFormats filtered_formats =
        FilterFormats(track_constraints, formats, &unsatisfied_constraint);
    if (filtered_formats.empty())
      continue;

    // A request with constraints that can be fulfilled.
    *fulfilled_constraints = track_constraints;
    *best_format = GetBestCaptureFormat(filtered_formats, track_constraints);
    DVLOG(3) << "Found a track that matches the constraints";
    return true;
  }
  DVLOG(3) << "No usable format found";
  return false;
}

void MediaStreamVideoSource::OnStartDone(MediaStreamRequestResult result) {
  DCHECK(CalledOnValidThread());
  DVLOG(3) << "OnStartDone({result =" << result << "})";
  if (result == MEDIA_DEVICE_OK) {
    DCHECK_EQ(STARTING, state_);
    state_ = STARTED;
    SetReadyState(blink::WebMediaStreamSource::ReadyStateLive);

    track_adapter_->StartFrameMonitoring(
        current_format_.frame_rate,
        base::Bind(&MediaStreamVideoSource::SetMutedState,
                   weak_factory_.GetWeakPtr()));

  } else {
    StopSource();
  }

  // This object can be deleted after calling FinalizeAddTrack. See comment in
  // the header file.
  FinalizeAddTrack();
}

void MediaStreamVideoSource::FinalizeAddTrack() {
  DCHECK(CalledOnValidThread());
  const media::VideoCaptureFormats formats(1, current_format_);

  std::vector<TrackDescriptor> track_descriptors;
  track_descriptors.swap(track_descriptors_);
  for (const auto& track : track_descriptors) {
    MediaStreamRequestResult result = MEDIA_DEVICE_OK;
    std::string unsatisfied_constraint;

    if (HasMandatoryConstraints(track.constraints) &&
        FilterFormats(track.constraints, formats, &unsatisfied_constraint)
            .empty()) {
      result = MEDIA_DEVICE_CONSTRAINT_NOT_SATISFIED;
      DVLOG(3) << "FinalizeAddTrack() ignoring device on constraint "
               << unsatisfied_constraint;
    }

    if (state_ != STARTED && result == MEDIA_DEVICE_OK)
      result = MEDIA_DEVICE_TRACK_START_FAILURE;

    if (result == MEDIA_DEVICE_OK) {
      int max_width;
      int max_height;
      GetDesiredMaxWidthAndHeight(track.constraints, &max_width, &max_height);
      double max_aspect_ratio;
      double min_aspect_ratio;
      GetDesiredMinAndMaxAspectRatio(track.constraints,
                                     &min_aspect_ratio,
                                     &max_aspect_ratio);
      double max_frame_rate = 0.0f;
      // Note: Optional and ideal constraints are ignored; this is
      // purely a hard max limit.
      if (track.constraints.basic().frameRate.hasMax())
        max_frame_rate = track.constraints.basic().frameRate.max();

      track_adapter_->AddTrack(track.track, track.frame_callback, max_width,
                               max_height, min_aspect_ratio, max_aspect_ratio,
                               max_frame_rate);
    }

    DVLOG(3) << "FinalizeAddTrack() result " << result;

    if (!track.callback.is_null())
      track.callback.Run(this, result,
                         blink::WebString::fromUTF8(unsatisfied_constraint));
  }
}

void MediaStreamVideoSource::SetReadyState(
    blink::WebMediaStreamSource::ReadyState state) {
  DVLOG(3) << "MediaStreamVideoSource::SetReadyState state " << state;
  DCHECK(CalledOnValidThread());
  if (!owner().isNull())
    owner().setReadyState(state);
  for (const auto& track : tracks_)
    track->OnReadyStateChanged(state);
}

void MediaStreamVideoSource::SetMutedState(bool muted_state) {
  DVLOG(3) << "MediaStreamVideoSource::SetMutedState state=" << muted_state;
  DCHECK(CalledOnValidThread());
  if (!owner().isNull()) {
    owner().setReadyState(muted_state
        ? blink::WebMediaStreamSource::ReadyStateMuted
        : blink::WebMediaStreamSource::ReadyStateLive);
  }
}

MediaStreamVideoSource::TrackDescriptor::TrackDescriptor(
    MediaStreamVideoTrack* track,
    const VideoCaptureDeliverFrameCB& frame_callback,
    const blink::WebMediaConstraints& constraints,
    const ConstraintsCallback& callback)
    : track(track),
      frame_callback(frame_callback),
      constraints(constraints),
      callback(callback) {
}

MediaStreamVideoSource::TrackDescriptor::TrackDescriptor(
    const TrackDescriptor& other) = default;

MediaStreamVideoSource::TrackDescriptor::~TrackDescriptor() {
}

}  // namespace content
