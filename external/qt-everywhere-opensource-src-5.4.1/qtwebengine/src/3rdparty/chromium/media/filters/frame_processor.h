// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_FRAME_PROCESSOR_H_
#define MEDIA_FILTERS_FRAME_PROCESSOR_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/base/stream_parser.h"
#include "media/filters/frame_processor_base.h"

namespace media {

// Helper class that implements Media Source Extension's coded frame processing
// algorithm.
class MEDIA_EXPORT FrameProcessor : public FrameProcessorBase {
 public:
  typedef base::Callback<void(base::TimeDelta)> UpdateDurationCB;
  explicit FrameProcessor(const UpdateDurationCB& update_duration_cb);
  virtual ~FrameProcessor();

  // FrameProcessorBase implementation
  virtual void SetSequenceMode(bool sequence_mode) OVERRIDE;
  virtual bool ProcessFrames(const StreamParser::BufferQueue& audio_buffers,
                             const StreamParser::BufferQueue& video_buffers,
                             const StreamParser::TextBufferQueueMap& text_map,
                             base::TimeDelta append_window_start,
                             base::TimeDelta append_window_end,
                             bool* new_media_segment,
                             base::TimeDelta* timestamp_offset) OVERRIDE;

 private:
  // Helper that processes one frame with the coded frame processing algorithm.
  // Returns false on error or true on success.
  bool ProcessFrame(const scoped_refptr<StreamParserBuffer>& frame,
                    base::TimeDelta append_window_start,
                    base::TimeDelta append_window_end,
                    base::TimeDelta* timestamp_offset,
                    bool* new_media_segment);

  // Tracks the MSE coded frame processing variable of same name. It stores the
  // highest coded frame end timestamp across all coded frames in the current
  // coded frame group. It is set to 0 when the SourceBuffer object is created
  // and gets updated by ProcessFrames().
  base::TimeDelta group_end_timestamp_;

  UpdateDurationCB update_duration_cb_;

  DISALLOW_COPY_AND_ASSIGN(FrameProcessor);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FRAME_PROCESSOR_H_
