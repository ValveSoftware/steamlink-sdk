// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_PROCESSOR_OPTIONS_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_PROCESSOR_OPTIONS_H_

#include <string>

#include "base/files/file.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"

namespace webrtc {

class AudioFrame;
class AudioProcessing;
class MediaConstraintsInterface;
class TypingDetection;

}

namespace content {

class RTCMediaConstraints;

using webrtc::AudioProcessing;
using webrtc::MediaConstraintsInterface;

// A helper class to parse audio constraints from a blink::WebMediaConstraints
// object.
class CONTENT_EXPORT MediaAudioConstraints {
 public:
  // Constraint keys used by audio processing.
  static const char kEchoCancellation[];
  static const char kGoogEchoCancellation[];
  static const char kGoogExperimentalEchoCancellation[];
  static const char kGoogAutoGainControl[];
  static const char kGoogExperimentalAutoGainControl[];
  static const char kGoogNoiseSuppression[];
  static const char kGoogExperimentalNoiseSuppression[];
  static const char kGoogHighpassFilter[];
  static const char kGoogTypingNoiseDetection[];
  static const char kGoogAudioMirroring[];

  // Merge |constraints| with |kDefaultAudioConstraints|. For any key which
  // exists in both, the value from |constraints| is maintained, including its
  // mandatory/optional status. New values from |kDefaultAudioConstraints| will
  // be added with optional status.
  static void ApplyFixedAudioConstraints(RTCMediaConstraints* constraints);

  // |effects| is the bitmasks telling whether certain platform
  // hardware audio effects are enabled, like hardware echo cancellation. If
  // some hardware effect is enabled, the corresponding software audio
  // processing will be disabled.
  MediaAudioConstraints(const blink::WebMediaConstraints& constraints,
                        int effects);
  virtual ~MediaAudioConstraints();

  // Checks if any audio constraints are set that requires audio processing to
  // be applied.
  bool NeedsAudioProcessing();

  // Gets the property of the constraint named by |key| in |constraints_|.
  // Returns the constraint's value if the key is found; Otherwise returns the
  // default value of the constraint.
  // Note, for constraint of |kEchoCancellation| or |kGoogEchoCancellation|,
  // clients should use GetEchoCancellationProperty().
  bool GetProperty(const std::string& key);

  // Gets the property of echo cancellation defined in |constraints_|. The
  // returned value depends on a combination of |effects_|, |kEchoCancellation|
  // and |kGoogEchoCancellation| in |constraints_|.
  bool GetEchoCancellationProperty();

  // Returns true if all the mandatory constraints in |constraints_| are valid;
  // Otherwise return false.
  bool IsValid();

 private:
  // Gets the default value of constraint named by |key| in |constraints|.
  bool GetDefaultValueForConstraint(
      const blink::WebMediaConstraints& constraints, const std::string& key);

  const blink::WebMediaConstraints constraints_;
  const int effects_;
  bool default_audio_processing_constraint_value_;
};

// Enables the echo cancellation in |audio_processing|.
void EnableEchoCancellation(AudioProcessing* audio_processing);

// Enables the noise suppression in |audio_processing|.
void EnableNoiseSuppression(AudioProcessing* audio_processing);

// Enables the experimental noise suppression in |audio_processing|.
void EnableExperimentalNoiseSuppression(AudioProcessing* audio_processing);

// Enables the high pass filter in |audio_processing|.
void EnableHighPassFilter(AudioProcessing* audio_processing);

// Enables the typing detection in |audio_processing|.
void EnableTypingDetection(AudioProcessing* audio_processing,
                           webrtc::TypingDetection* typing_detector);

// Enables the experimental echo cancellation in |audio_processing|.
void EnableExperimentalEchoCancellation(AudioProcessing* audio_processing);

// Starts the echo cancellation dump in |audio_processing|.
void StartEchoCancellationDump(AudioProcessing* audio_processing,
                               base::File aec_dump_file);

// Stops the echo cancellation dump in |audio_processing|.
// This method has no impact if echo cancellation dump has not been started on
// |audio_processing|.
void StopEchoCancellationDump(AudioProcessing* audio_processing);

void EnableAutomaticGainControl(AudioProcessing* audio_processing);

void GetAecStats(AudioProcessing* audio_processing,
                 webrtc::AudioProcessorInterface::AudioProcessorStats* stats);

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_PROCESSOR_OPTIONS_H_
