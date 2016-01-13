// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_video_source.h"

#include <algorithm>
#include <limits>
#include <string>

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "content/child/child_process.h"
#include "content/renderer/media/media_stream_constraints_util.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/video_track_adapter.h"

namespace content {

// Constraint keys. Specified by draft-alvestrand-constraints-resolution-00b
const char MediaStreamVideoSource::kMinAspectRatio[] = "minAspectRatio";
const char MediaStreamVideoSource::kMaxAspectRatio[] = "maxAspectRatio";
const char MediaStreamVideoSource::kMaxWidth[] = "maxWidth";
const char MediaStreamVideoSource::kMinWidth[] = "minWidth";
const char MediaStreamVideoSource::kMaxHeight[] = "maxHeight";
const char MediaStreamVideoSource::kMinHeight[] = "minHeight";
const char MediaStreamVideoSource::kMaxFrameRate[] = "maxFrameRate";
const char MediaStreamVideoSource::kMinFrameRate[] = "minFrameRate";

const char* kSupportedConstraints[] = {
  MediaStreamVideoSource::kMaxAspectRatio,
  MediaStreamVideoSource::kMinAspectRatio,
  MediaStreamVideoSource::kMaxWidth,
  MediaStreamVideoSource::kMinWidth,
  MediaStreamVideoSource::kMaxHeight,
  MediaStreamVideoSource::kMinHeight,
  MediaStreamVideoSource::kMaxFrameRate,
  MediaStreamVideoSource::kMinFrameRate,
};

const int MediaStreamVideoSource::kDefaultWidth = 640;
const int MediaStreamVideoSource::kDefaultHeight = 480;
const int MediaStreamVideoSource::kDefaultFrameRate = 30;

namespace {

// Google-specific key prefix. Constraints with this prefix are ignored if they
// are unknown.
const char kGooglePrefix[] = "goog";

// Returns true if |constraint| has mandatory constraints.
bool HasMandatoryConstraints(const blink::WebMediaConstraints& constraints) {
  blink::WebVector<blink::WebMediaConstraint> mandatory_constraints;
  constraints.getMandatoryConstraints(mandatory_constraints);
  return !mandatory_constraints.isEmpty();
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

  bool mandatory = GetMandatoryConstraintValueAsInteger(
      constraints,
      MediaStreamVideoSource::kMaxWidth,
      desired_width);
  mandatory |= GetMandatoryConstraintValueAsInteger(
      constraints,
      MediaStreamVideoSource::kMaxHeight,
      desired_height);
  if (mandatory)
    return;

  GetOptionalConstraintValueAsInteger(constraints,
                                      MediaStreamVideoSource::kMaxWidth,
                                      desired_width);
  GetOptionalConstraintValueAsInteger(constraints,
                                      MediaStreamVideoSource::kMaxHeight,
                                      desired_height);
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

  bool mandatory = GetMandatoryConstraintValueAsDouble(
      constraints,
      MediaStreamVideoSource::kMinAspectRatio,
      min_aspect_ratio);
  mandatory |= GetMandatoryConstraintValueAsDouble(
      constraints,
      MediaStreamVideoSource::kMaxAspectRatio,
      max_aspect_ratio);
  if (mandatory)
    return;

  GetOptionalConstraintValueAsDouble(
      constraints,
      MediaStreamVideoSource::kMinAspectRatio,
      min_aspect_ratio);
  GetOptionalConstraintValueAsDouble(
      constraints,
      MediaStreamVideoSource::kMaxAspectRatio,
      max_aspect_ratio);
}

// Returns true if |constraint| is fulfilled. |format| can be changed by a
// constraint, e.g. the frame rate can be changed by setting maxFrameRate.
bool UpdateFormatForConstraint(
    const blink::WebMediaConstraint& constraint,
    bool mandatory,
    media::VideoCaptureFormat* format) {
  DCHECK(format != NULL);

  if (!format->IsValid())
    return false;

  std::string constraint_name = constraint.m_name.utf8();
  std::string constraint_value = constraint.m_value.utf8();

  if (constraint_name.find(kGooglePrefix) == 0) {
    // These are actually options, not constraints, so they can be satisfied
    // regardless of the format.
    return true;
  }

  if (constraint_name == MediaStreamSource::kSourceId) {
    // This is a constraint that doesn't affect the format.
    return true;
  }

  // Ignore Chrome specific Tab capture constraints.
  if (constraint_name == kMediaStreamSource ||
      constraint_name == kMediaStreamSourceId)
    return true;

  if (constraint_name == MediaStreamVideoSource::kMinAspectRatio ||
      constraint_name == MediaStreamVideoSource::kMaxAspectRatio) {
    // These constraints are handled by cropping if the camera outputs the wrong
    // aspect ratio.
    double value;
    return base::StringToDouble(constraint_value, &value);
  }

  double value = 0.0;
  if (!base::StringToDouble(constraint_value, &value)) {
    DLOG(WARNING) << "Can't parse MediaStream constraint. Name:"
                  <<  constraint_name << " Value:" << constraint_value;
    return false;
  }

  if (constraint_name == MediaStreamVideoSource::kMinWidth) {
    return (value <= format->frame_size.width());
  } else if (constraint_name == MediaStreamVideoSource::kMaxWidth) {
    return value > 0.0;
  } else if (constraint_name == MediaStreamVideoSource::kMinHeight) {
    return (value <= format->frame_size.height());
  } else if (constraint_name == MediaStreamVideoSource::kMaxHeight) {
     return value > 0.0;
  } else if (constraint_name == MediaStreamVideoSource::kMinFrameRate) {
    return (value <= format->frame_rate);
  } else if (constraint_name == MediaStreamVideoSource::kMaxFrameRate) {
    if (value == 0.0) {
      // The frame rate is set by constraint.
      // Don't allow 0 as frame rate if it is a mandatory constraint.
      // Set the frame rate to 1 if it is not mandatory.
      if (mandatory) {
        return false;
      } else {
        value = 1.0;
      }
    }
    format->frame_rate =
        (format->frame_rate > value) ? value : format->frame_rate;
    return true;
  } else {
    LOG(WARNING) << "Found unknown MediaStream constraint. Name:"
                 <<  constraint_name << " Value:" << constraint_value;
    return false;
  }
}

// Removes media::VideoCaptureFormats from |formats| that don't meet
// |constraint|.
void FilterFormatsByConstraint(
    const blink::WebMediaConstraint& constraint,
    bool mandatory,
    media::VideoCaptureFormats* formats) {
  DVLOG(3) << "FilterFormatsByConstraint("
           << "{ constraint.m_name = " << constraint.m_name.utf8()
           << "  constraint.m_value = " << constraint.m_value.utf8()
           << "  mandatory =  " << mandatory << "})";
  media::VideoCaptureFormats::iterator format_it = formats->begin();
  while (format_it != formats->end()) {
    // Modify the format_it to fulfill the constraint if possible.
    // Delete it otherwise.
    if (!UpdateFormatForConstraint(constraint, mandatory, &(*format_it))) {
      format_it = formats->erase(format_it);
    } else {
      ++format_it;
    }
  }
}

// Returns the media::VideoCaptureFormats that matches |constraints|.
media::VideoCaptureFormats FilterFormats(
    const blink::WebMediaConstraints& constraints,
    const media::VideoCaptureFormats& supported_formats) {
  if (constraints.isNull()) {
    return supported_formats;
  }

  double max_aspect_ratio;
  double min_aspect_ratio;
  GetDesiredMinAndMaxAspectRatio(constraints,
                                 &min_aspect_ratio,
                                 &max_aspect_ratio);

  if (min_aspect_ratio > max_aspect_ratio || max_aspect_ratio < 0.05f) {
    DLOG(WARNING) << "Wrong requested aspect ratio.";
    return media::VideoCaptureFormats();
  }

  int min_width = 0;
  GetMandatoryConstraintValueAsInteger(constraints,
                                       MediaStreamVideoSource::kMinWidth,
                                       &min_width);
  int min_height = 0;
  GetMandatoryConstraintValueAsInteger(constraints,
                                       MediaStreamVideoSource::kMinHeight,
                                       &min_height);
  int max_width;
  int max_height;
  GetDesiredMaxWidthAndHeight(constraints, &max_width, &max_height);

  if (min_width > max_width || min_height > max_height)
    return media::VideoCaptureFormats();

  blink::WebVector<blink::WebMediaConstraint> mandatory;
  blink::WebVector<blink::WebMediaConstraint> optional;
  constraints.getMandatoryConstraints(mandatory);
  constraints.getOptionalConstraints(optional);
  media::VideoCaptureFormats candidates = supported_formats;
  for (size_t i = 0; i < mandatory.size(); ++i)
    FilterFormatsByConstraint(mandatory[i], true, &candidates);

  if (candidates.empty())
    return candidates;

  // Ok - all mandatory checked and we still have candidates.
  // Let's try filtering using the optional constraints. The optional
  // constraints must be filtered in the order they occur in |optional|.
  // But if a constraint produce zero candidates, the constraint is ignored and
  // the next constraint is tested.
  // http://dev.w3.org/2011/webrtc/editor/getusermedia.html#idl-def-Constraints
  for (size_t i = 0; i < optional.size(); ++i) {
    media::VideoCaptureFormats current_candidates = candidates;
    FilterFormatsByConstraint(optional[i], false, &current_candidates);
    if (!current_candidates.empty()) {
      candidates = current_candidates;
    }
  }

  // We have done as good as we can to filter the supported resolutions.
  return candidates;
}

const media::VideoCaptureFormat& GetBestFormatBasedOnArea(
    const media::VideoCaptureFormats& formats,
    int area) {
  media::VideoCaptureFormats::const_iterator it = formats.begin();
  media::VideoCaptureFormats::const_iterator best_it = formats.begin();
  int best_diff = std::numeric_limits<int>::max();
  for (; it != formats.end(); ++it) {
    int diff = abs(area - it->frame_size.width() * it->frame_size.height());
    if (diff < best_diff) {
      best_diff = diff;
      best_it = it;
    }
  }
  return *best_it;
}

// Find the format that best matches the default video size.
// This algorithm is chosen since a resolution must be picked even if no
// constraints are provided. We don't just select the maximum supported
// resolution since higher resolutions cost more in terms of complexity and
// many cameras have lower frame rate and have more noise in the image at
// their maximum supported resolution.
void GetBestCaptureFormat(
    const media::VideoCaptureFormats& formats,
    const blink::WebMediaConstraints& constraints,
    media::VideoCaptureFormat* capture_format) {
  DCHECK(!formats.empty());

  int max_width;
  int max_height;
  GetDesiredMaxWidthAndHeight(constraints, &max_width, &max_height);

  *capture_format = GetBestFormatBasedOnArea(
      formats,
      std::min(max_width, MediaStreamVideoSource::kDefaultWidth) *
      std::min(max_height, MediaStreamVideoSource::kDefaultHeight));
}

}  // anonymous namespace

// static
MediaStreamVideoSource* MediaStreamVideoSource::GetVideoSource(
    const blink::WebMediaStreamSource& source) {
  return static_cast<MediaStreamVideoSource*>(source.extraData());
}

// static
bool MediaStreamVideoSource::IsConstraintSupported(const std::string& name) {
  for (size_t i = 0; i < arraysize(kSupportedConstraints); ++i) {
    if (kSupportedConstraints[i] == name)
      return true;
  }
  return false;
}

MediaStreamVideoSource::MediaStreamVideoSource()
    : state_(NEW),
      track_adapter_(new VideoTrackAdapter(
          ChildProcess::current()->io_message_loop_proxy())),
      weak_factory_(this) {
}

MediaStreamVideoSource::~MediaStreamVideoSource() {
  DVLOG(3) << "~MediaStreamVideoSource()";
}

void MediaStreamVideoSource::AddTrack(
    MediaStreamVideoTrack* track,
    const VideoCaptureDeliverFrameCB& frame_callback,
    const blink::WebMediaConstraints& constraints,
    const ConstraintsCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!constraints.isNull());
  DCHECK(std::find(tracks_.begin(), tracks_.end(),
                   track) == tracks_.end());
  tracks_.push_back(track);

  requested_constraints_.push_back(
      RequestedConstraints(track, frame_callback, constraints, callback));

  switch (state_) {
    case NEW: {
      // Tab capture and Screen capture needs the maximum requested height
      // and width to decide on the resolution.
      int max_requested_width = 0;
      GetMandatoryConstraintValueAsInteger(constraints, kMaxWidth,
                                           &max_requested_width);

      int max_requested_height = 0;
      GetMandatoryConstraintValueAsInteger(constraints, kMaxHeight,
                                           &max_requested_height);

      state_ = RETRIEVING_CAPABILITIES;
      GetCurrentSupportedFormats(
          max_requested_width,
          max_requested_height,
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

  // Check if |video_track| is waiting for applying new constraints and remove
  // the request in that case.
  for (std::vector<RequestedConstraints>::iterator it =
           requested_constraints_.begin();
       it != requested_constraints_.end(); ++it) {
    if (it->track == video_track) {
      requested_constraints_.erase(it);
      break;
    }
  }
  // Call |frame_adapter_->RemoveTrack| here even if adding the track has
  // failed and |frame_adapter_->AddCallback| has not been called.
  track_adapter_->RemoveTrack(video_track);

  if (tracks_.empty())
    StopSource();
}

const scoped_refptr<base::MessageLoopProxy>&
MediaStreamVideoSource::io_message_loop() const {
  DCHECK(CalledOnValidThread());
  return track_adapter_->io_message_loop();
}

void MediaStreamVideoSource::DoStopSource() {
  DCHECK(CalledOnValidThread());
  DVLOG(3) << "DoStopSource()";
  if (state_ == ENDED)
    return;
  StopSourceImpl();
  state_ = ENDED;
  SetReadyState(blink::WebMediaStreamSource::ReadyStateEnded);
}

void MediaStreamVideoSource::OnSupportedFormats(
    const media::VideoCaptureFormats& formats) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(RETRIEVING_CAPABILITIES, state_);

  supported_formats_ = formats;
  if (!FindBestFormatWithConstraints(supported_formats_,
                                     &current_format_)) {
    SetReadyState(blink::WebMediaStreamSource::ReadyStateEnded);
    // This object can be deleted after calling FinalizeAddTrack. See comment
    // in the header file.
    FinalizeAddTrack();
    return;
  }

  state_ = STARTING;
  DVLOG(3) << "Starting the capturer with"
           << " width = " << current_format_.frame_size.width()
           << " height = " << current_format_.frame_size.height()
           << " frame rate = " << current_format_.frame_rate;

  media::VideoCaptureParams params;
  params.requested_format = current_format_;
  StartSourceImpl(
      params,
      base::Bind(&VideoTrackAdapter::DeliverFrameOnIO, track_adapter_));
}

bool MediaStreamVideoSource::FindBestFormatWithConstraints(
    const media::VideoCaptureFormats& formats,
    media::VideoCaptureFormat* best_format) {
  // Find the first constraints that we can fulfill.
  for (std::vector<RequestedConstraints>::iterator request_it =
           requested_constraints_.begin();
       request_it != requested_constraints_.end(); ++request_it) {
    const blink::WebMediaConstraints& requested_constraints =
        request_it->constraints;

    // If the source doesn't support capability enumeration it is still ok if
    // no mandatory constraints have been specified. That just means that
    // we will start with whatever format is native to the source.
    if (formats.empty() && !HasMandatoryConstraints(requested_constraints)) {
      *best_format = media::VideoCaptureFormat();
      return true;
    }
    media::VideoCaptureFormats filtered_formats =
        FilterFormats(requested_constraints, formats);
    if (filtered_formats.size() > 0) {
      // A request with constraints that can be fulfilled.
      GetBestCaptureFormat(filtered_formats,
                           requested_constraints,
                           best_format);
      return true;
    }
  }
  return false;
}

void MediaStreamVideoSource::OnStartDone(bool success) {
  DCHECK(CalledOnValidThread());
  DVLOG(3) << "OnStartDone({success =" << success << "})";
  if (success) {
    DCHECK_EQ(STARTING, state_);
    state_ = STARTED;
    SetReadyState(blink::WebMediaStreamSource::ReadyStateLive);
  } else {
    state_ = ENDED;
    SetReadyState(blink::WebMediaStreamSource::ReadyStateEnded);
    StopSourceImpl();
  }

  // This object can be deleted after calling FinalizeAddTrack. See comment in
  // the header file.
  FinalizeAddTrack();
}

void MediaStreamVideoSource::FinalizeAddTrack() {
  media::VideoCaptureFormats formats;
  formats.push_back(current_format_);

  std::vector<RequestedConstraints> callbacks;
  callbacks.swap(requested_constraints_);
  for (std::vector<RequestedConstraints>::iterator it = callbacks.begin();
       it != callbacks.end(); ++it) {
    // The track has been added successfully if the source has started and
    // there are either no mandatory constraints and the source doesn't expose
    // its format capabilities, or the constraints and the format match.
    // For example, a remote source doesn't expose its format capabilities.
    bool success =
        state_ == STARTED &&
        ((!current_format_.IsValid() && !HasMandatoryConstraints(
            it->constraints)) ||
         !FilterFormats(it->constraints, formats).empty());

    if (success) {
      int max_width;
      int max_height;
      GetDesiredMaxWidthAndHeight(it->constraints, &max_width, &max_height);
      double max_aspect_ratio;
      double min_aspect_ratio;
      GetDesiredMinAndMaxAspectRatio(it->constraints,
                                     &min_aspect_ratio,
                                     &max_aspect_ratio);
      track_adapter_->AddTrack(it->track,it->frame_callback,
                               max_width, max_height,
                               min_aspect_ratio, max_aspect_ratio);
    }

    DVLOG(3) << "FinalizeAddTrack() success " << success;

    if (!it->callback.is_null())
      it->callback.Run(this, success);
  }
}

void MediaStreamVideoSource::SetReadyState(
    blink::WebMediaStreamSource::ReadyState state) {
  if (!owner().isNull()) {
    owner().setReadyState(state);
  }
  for (std::vector<MediaStreamVideoTrack*>::iterator it = tracks_.begin();
       it != tracks_.end(); ++it) {
    (*it)->OnReadyStateChanged(state);
  }
}

MediaStreamVideoSource::RequestedConstraints::RequestedConstraints(
    MediaStreamVideoTrack* track,
    const VideoCaptureDeliverFrameCB& frame_callback,
    const blink::WebMediaConstraints& constraints,
    const ConstraintsCallback& callback)
    : track(track),
      frame_callback(frame_callback),
      constraints(constraints),
      callback(callback) {
}

MediaStreamVideoSource::RequestedConstraints::~RequestedConstraints() {
}

}  // namespace content
