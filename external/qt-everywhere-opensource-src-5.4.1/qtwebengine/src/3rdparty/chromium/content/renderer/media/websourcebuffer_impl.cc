// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/websourcebuffer_impl.h"

#include <limits>

#include "base/float_util.h"
#include "media/filters/chunk_demuxer.h"

namespace content {

static base::TimeDelta DoubleToTimeDelta(double time) {
  DCHECK(!base::IsNaN(time));
  DCHECK_NE(time, -std::numeric_limits<double>::infinity());

  if (time == std::numeric_limits<double>::infinity())
    return media::kInfiniteDuration();

  // Don't use base::TimeDelta::Max() here, as we want the largest finite time
  // delta.
  base::TimeDelta max_time = base::TimeDelta::FromInternalValue(kint64max - 1);
  double max_time_in_seconds = max_time.InSecondsF();

  if (time >= max_time_in_seconds)
    return max_time;

  return base::TimeDelta::FromMicroseconds(
      time * base::Time::kMicrosecondsPerSecond);
}

WebSourceBufferImpl::WebSourceBufferImpl(
    const std::string& id, media::ChunkDemuxer* demuxer)
    : id_(id),
      demuxer_(demuxer),
      append_window_end_(media::kInfiniteDuration()) {
  DCHECK(demuxer_);
}

WebSourceBufferImpl::~WebSourceBufferImpl() {
  DCHECK(!demuxer_) << "Object destroyed w/o removedFromMediaSource() call";
}

bool WebSourceBufferImpl::setMode(WebSourceBuffer::AppendMode mode) {
  if (demuxer_->IsParsingMediaSegment(id_))
    return false;

  switch (mode) {
    case WebSourceBuffer::AppendModeSegments:
      demuxer_->SetSequenceMode(id_, false);
      return true;
    case WebSourceBuffer::AppendModeSequence:
      demuxer_->SetSequenceMode(id_, true);
      return true;
  }

  NOTREACHED();
  return false;
}

blink::WebTimeRanges WebSourceBufferImpl::buffered() {
  media::Ranges<base::TimeDelta> ranges = demuxer_->GetBufferedRanges(id_);
  blink::WebTimeRanges result(ranges.size());
  for (size_t i = 0; i < ranges.size(); i++) {
    result[i].start = ranges.start(i).InSecondsF();
    result[i].end = ranges.end(i).InSecondsF();
  }
  return result;
}

void WebSourceBufferImpl::append(
    const unsigned char* data,
    unsigned length,
    double* timestamp_offset) {
  base::TimeDelta old_offset = timestamp_offset_;
  demuxer_->AppendData(id_, data, length,
                       append_window_start_, append_window_end_,
                       &timestamp_offset_);

  // Coded frame processing may update the timestamp offset. If the caller
  // provides a non-NULL |timestamp_offset| and frame processing changes the
  // timestamp offset, report the new offset to the caller. Do not update the
  // caller's offset otherwise, to preserve any pre-existing value that may have
  // more than microsecond precision.
  if (timestamp_offset && old_offset != timestamp_offset_)
    *timestamp_offset = timestamp_offset_.InSecondsF();
}

void WebSourceBufferImpl::abort() {
  demuxer_->Abort(id_,
                  append_window_start_, append_window_end_,
                  &timestamp_offset_);

  // TODO(wolenetz): abort should be able to modify the caller timestamp offset
  // (just like WebSourceBufferImpl::append).
  // See http://crbug.com/370229 for further details.
}

void WebSourceBufferImpl::remove(double start, double end) {
  DCHECK_GE(start, 0);
  DCHECK_GE(end, 0);
  demuxer_->Remove(id_, DoubleToTimeDelta(start), DoubleToTimeDelta(end));
}

bool WebSourceBufferImpl::setTimestampOffset(double offset) {
  if (demuxer_->IsParsingMediaSegment(id_))
    return false;

  timestamp_offset_ = DoubleToTimeDelta(offset);

  // http://www.w3.org/TR/media-source/#widl-SourceBuffer-timestampOffset
  // Step 6: If the mode attribute equals "sequence", then set the group start
  // timestamp to new timestamp offset.
  demuxer_->SetGroupStartTimestampIfInSequenceMode(id_, timestamp_offset_);
  return true;
}

void WebSourceBufferImpl::setAppendWindowStart(double start) {
  DCHECK_GE(start, 0);
  append_window_start_ = DoubleToTimeDelta(start);
}

void WebSourceBufferImpl::setAppendWindowEnd(double end) {
  DCHECK_GE(end, 0);
  append_window_end_ = DoubleToTimeDelta(end);
}

void WebSourceBufferImpl::removedFromMediaSource() {
  demuxer_->RemoveId(id_);
  demuxer_ = NULL;
}

}  // namespace content
