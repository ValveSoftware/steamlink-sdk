// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/blocking_url_protocol.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/macros.h"
#include "media/base/data_source.h"
#include "media/ffmpeg/ffmpeg_common.h"

namespace media {

BlockingUrlProtocol::BlockingUrlProtocol(DataSource* data_source,
                                         const base::Closure& error_cb)
    : data_source_(data_source),
      error_cb_(error_cb),
      aborted_(base::WaitableEvent::ResetPolicy::MANUAL,
               base::WaitableEvent::InitialState::NOT_SIGNALED),  // We never
                                                                  // want to
                                                                  // reset
                                                                  // |aborted_|.
      read_complete_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                     base::WaitableEvent::InitialState::NOT_SIGNALED),
      last_read_bytes_(0),
      read_position_(0) {}

BlockingUrlProtocol::~BlockingUrlProtocol() {}

void BlockingUrlProtocol::Abort() {
  aborted_.Signal();
}

int BlockingUrlProtocol::Read(int size, uint8_t* data) {
  // Read errors are unrecoverable.
  if (aborted_.IsSignaled())
    return AVERROR(EIO);

  // Even though FFmpeg defines AVERROR_EOF, it's not to be used with I/O
  // routines. Instead return 0 for any read at or past EOF.
  int64_t file_size;
  if (data_source_->GetSize(&file_size) && read_position_ >= file_size)
    return 0;

  // Blocking read from data source until either:
  //   1) |last_read_bytes_| is set and |read_complete_| is signalled
  //   2) |aborted_| is signalled
  data_source_->Read(read_position_, size, data, base::Bind(
      &BlockingUrlProtocol::SignalReadCompleted, base::Unretained(this)));

  base::WaitableEvent* events[] = { &aborted_, &read_complete_ };
  size_t index = base::WaitableEvent::WaitMany(events, arraysize(events));

  if (events[index] == &aborted_)
    return AVERROR(EIO);

  if (last_read_bytes_ == DataSource::kReadError) {
    aborted_.Signal();
    error_cb_.Run();
    return AVERROR(EIO);
  }

  read_position_ += last_read_bytes_;
  return last_read_bytes_;
}

bool BlockingUrlProtocol::GetPosition(int64_t* position_out) {
  *position_out = read_position_;
  return true;
}

bool BlockingUrlProtocol::SetPosition(int64_t position) {
  int64_t file_size;
  if ((data_source_->GetSize(&file_size) && position > file_size) ||
      position < 0) {
    return false;
  }

  read_position_ = position;
  return true;
}

bool BlockingUrlProtocol::GetSize(int64_t* size_out) {
  return data_source_->GetSize(size_out);
}

bool BlockingUrlProtocol::IsStreaming() {
  return data_source_->IsStreaming();
}

void BlockingUrlProtocol::SignalReadCompleted(int size) {
  last_read_bytes_ = size;
  read_complete_.Signal();
}

}  // namespace media
