// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/logging.h"
#include "media/tools/player_x11/data_source_logger.h"

static void LogAndRunStopClosure(const base::Closure& closure) {
  VLOG(1) << "Stop() finished";
  closure.Run();
}

static void LogAndRunReadCB(
    int64 position, int size,
    const media::DataSource::ReadCB& read_cb, int result) {
  VLOG(1) << "Read(" << position << ", " << size << ") -> " << result;
  read_cb.Run(result);
}

DataSourceLogger::DataSourceLogger(
    scoped_ptr<media::DataSource> data_source,
    bool streaming)
    : data_source_(data_source.Pass()),
      streaming_(streaming) {
}

void DataSourceLogger::Stop(const base::Closure& closure) {
  VLOG(1) << "Stop() started";
  data_source_->Stop(base::Bind(&LogAndRunStopClosure, closure));
}

void DataSourceLogger::Read(
    int64 position, int size, uint8* data,
    const media::DataSource::ReadCB& read_cb) {
  VLOG(1) << "Read(" << position << ", " << size << ")";
  data_source_->Read(position, size, data, base::Bind(
      &LogAndRunReadCB, position, size, read_cb));
}

bool DataSourceLogger::GetSize(int64* size_out) {
  bool success = data_source_->GetSize(size_out);
  VLOG(1) << "GetSize() -> " << (success ? "true" : "false")
          << ", " << *size_out;
  return success;
}

bool DataSourceLogger::IsStreaming() {
  if (streaming_) {
    VLOG(1) << "IsStreaming() -> true (overridden)";
    return true;
  }

  bool streaming = data_source_->IsStreaming();
  VLOG(1) << "IsStreaming() -> " << (streaming ? "true" : "false");
  return streaming;
}

void DataSourceLogger::SetBitrate(int bitrate) {
  VLOG(1) << "SetBitrate(" << bitrate << ")";
  data_source_->SetBitrate(bitrate);
}

DataSourceLogger::~DataSourceLogger() {}
