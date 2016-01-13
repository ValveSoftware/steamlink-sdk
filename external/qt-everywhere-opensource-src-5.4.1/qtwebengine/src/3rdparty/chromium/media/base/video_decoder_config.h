// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_DECODER_CONFIG_H_
#define MEDIA_BASE_VIDEO_DECODER_CONFIG_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "media/base/media_export.h"
#include "media/base/video_frame.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace media {

enum VideoCodec {
  // These values are histogrammed over time; do not change their ordinal
  // values.  When deleting a codec replace it with a dummy value; when adding a
  // codec, do so at the bottom (and update kVideoCodecMax).
  kUnknownVideoCodec = 0,
  kCodecH264,
  kCodecVC1,
  kCodecMPEG2,
  kCodecMPEG4,
  kCodecTheora,
  kCodecVP8,
  kCodecVP9,
  // DO NOT ADD RANDOM VIDEO CODECS!
  //
  // The only acceptable time to add a new codec is if there is production code
  // that uses said codec in the same CL.

  kVideoCodecMax = kCodecVP9  // Must equal the last "real" codec above.
};

// Video stream profile.  This *must* match PP_VideoDecoder_Profile.
// (enforced in webkit/plugins/ppapi/ppb_video_decoder_impl.cc)
enum VideoCodecProfile {
  // Keep the values in this enum unique, as they imply format (h.264 vs. VP8,
  // for example), and keep the values for a particular format grouped
  // together for clarity.
  VIDEO_CODEC_PROFILE_UNKNOWN = -1,
  VIDEO_CODEC_PROFILE_MIN = VIDEO_CODEC_PROFILE_UNKNOWN,
  H264PROFILE_MIN = 0,
  H264PROFILE_BASELINE = H264PROFILE_MIN,
  H264PROFILE_MAIN = 1,
  H264PROFILE_EXTENDED = 2,
  H264PROFILE_HIGH = 3,
  H264PROFILE_HIGH10PROFILE = 4,
  H264PROFILE_HIGH422PROFILE = 5,
  H264PROFILE_HIGH444PREDICTIVEPROFILE = 6,
  H264PROFILE_SCALABLEBASELINE = 7,
  H264PROFILE_SCALABLEHIGH = 8,
  H264PROFILE_STEREOHIGH = 9,
  H264PROFILE_MULTIVIEWHIGH = 10,
  H264PROFILE_MAX = H264PROFILE_MULTIVIEWHIGH,
  VP8PROFILE_MIN = 11,
  VP8PROFILE_MAIN = VP8PROFILE_MIN,
  VP8PROFILE_MAX = VP8PROFILE_MAIN,
  VP9PROFILE_MIN = 12,
  VP9PROFILE_MAIN = VP9PROFILE_MIN,
  VP9PROFILE_MAX = VP9PROFILE_MAIN,
  VIDEO_CODEC_PROFILE_MAX = VP9PROFILE_MAX,
};

class MEDIA_EXPORT VideoDecoderConfig {
 public:
  // Constructs an uninitialized object. Clients should call Initialize() with
  // appropriate values before using.
  VideoDecoderConfig();

  // Constructs an initialized object. It is acceptable to pass in NULL for
  // |extra_data|, otherwise the memory is copied.
  VideoDecoderConfig(VideoCodec codec,
                     VideoCodecProfile profile,
                     VideoFrame::Format format,
                     const gfx::Size& coded_size,
                     const gfx::Rect& visible_rect,
                     const gfx::Size& natural_size,
                     const uint8* extra_data, size_t extra_data_size,
                     bool is_encrypted);

  ~VideoDecoderConfig();

  // Resets the internal state of this object.
  void Initialize(VideoCodec codec,
                  VideoCodecProfile profile,
                  VideoFrame::Format format,
                  const gfx::Size& coded_size,
                  const gfx::Rect& visible_rect,
                  const gfx::Size& natural_size,
                  const uint8* extra_data, size_t extra_data_size,
                  bool is_encrypted,
                  bool record_stats);

  // Returns true if this object has appropriate configuration values, false
  // otherwise.
  bool IsValidConfig() const;

  // Returns true if all fields in |config| match this config.
  // Note: The contents of |extra_data_| are compared not the raw pointers.
  bool Matches(const VideoDecoderConfig& config) const;

  // Returns a human-readable string describing |*this|.  For debugging & test
  // output only.
  std::string AsHumanReadableString() const;

  VideoCodec codec() const;
  VideoCodecProfile profile() const;

  // Video format used to determine YUV buffer sizes.
  VideoFrame::Format format() const;

  // Width and height of video frame immediately post-decode. Not all pixels
  // in this region are valid.
  gfx::Size coded_size() const;

  // Region of |coded_size_| that is visible.
  gfx::Rect visible_rect() const;

  // Final visible width and height of a video frame with aspect ratio taken
  // into account.
  gfx::Size natural_size() const;

  // Optional byte data required to initialize video decoders, such as H.264
  // AAVC data.
  const uint8* extra_data() const;
  size_t extra_data_size() const;

  // Whether the video stream is potentially encrypted.
  // Note that in a potentially encrypted video stream, individual buffers
  // can be encrypted or not encrypted.
  bool is_encrypted() const;

 private:
  VideoCodec codec_;
  VideoCodecProfile profile_;

  VideoFrame::Format format_;

  gfx::Size coded_size_;
  gfx::Rect visible_rect_;
  gfx::Size natural_size_;

  std::vector<uint8> extra_data_;

  bool is_encrypted_;

  // Not using DISALLOW_COPY_AND_ASSIGN here intentionally to allow the compiler
  // generated copy constructor and assignment operator. Since the extra data is
  // typically small, the performance impact is minimal.
};

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_DECODER_CONFIG_H_
