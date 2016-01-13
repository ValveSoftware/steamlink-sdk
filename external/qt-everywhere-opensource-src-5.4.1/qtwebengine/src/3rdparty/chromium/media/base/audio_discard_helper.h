// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_DISCARD_HELPER_H_
#define MEDIA_BASE_AUDIO_DISCARD_HELPER_H_

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/buffers.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_export.h"

namespace media {

class AudioBuffer;

// Helper class for managing timestamps and discard events around decoding.
class MEDIA_EXPORT AudioDiscardHelper {
 public:
  // |sample_rate| is the sample rate of decoded data which will be handed into
  // the ProcessBuffers() call.
  //
  // |decoder_delay| is the number of frames a decoder will output before data
  // corresponding to the first encoded buffer is output.  Callers only need to
  // specify this if the decoder inserts frames which have no corresponding
  // encoded buffer.
  //
  // For example, most MP3 decoders will output 529 junk frames before the data
  // corresponding to the first encoded buffer is output.  These frames are not
  // represented in the encoded data stream and instead are an artifact of how
  // most MP3 decoders work.  See http://lame.sourceforge.net/tech-FAQ.txt
  //
  // NOTE: End discard is only supported when there is no |decoder_delay|.
  AudioDiscardHelper(int sample_rate, size_t decoder_delay);
  ~AudioDiscardHelper();

  // Converts a TimeDelta to a frame count based on the constructed sample rate.
  // |duration| must be positive.
  size_t TimeDeltaToFrames(base::TimeDelta duration) const;

  // Resets internal state and indicates that |initial_discard| of upcoming
  // frames should be discarded.
  void Reset(size_t initial_discard);

  // Applies discard padding from the encoded buffer along with any initial
  // discards.  |decoded_buffer| may be NULL, if not the timestamp and duration
  // will be set after discards are applied.  Returns true if |decoded_buffer|
  // exists after processing discard events.  Returns false if |decoded_buffer|
  // was NULL, is completely discarded, or a processing error occurs.
  //
  // If AudioDiscardHelper is not initialized() the timestamp of the first
  // |encoded_buffer| will be used as the basis for all future timestamps set on
  // |decoded_buffer|s.  If the first buffer has a negative timestamp it will be
  // clamped to zero.
  bool ProcessBuffers(const scoped_refptr<DecoderBuffer>& encoded_buffer,
                      const scoped_refptr<AudioBuffer>& decoded_buffer);

  // Whether any buffers have been processed.
  bool initialized() const {
    return timestamp_helper_.base_timestamp() != kNoTimestamp();
  }

 private:
  const int sample_rate_;
  const size_t decoder_delay_;
  AudioTimestampHelper timestamp_helper_;

  size_t discard_frames_;
  base::TimeDelta last_input_timestamp_;

  bool delayed_discard_;
  DecoderBuffer::DiscardPadding delayed_discard_padding_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(AudioDiscardHelper);
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_DISCARD_HELPER_H_
