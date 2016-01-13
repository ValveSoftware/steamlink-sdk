// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_TOOLS_PLAYER_X11_DATA_SOURCE_LOGGER_H_
#define MEDIA_TOOLS_PLAYER_X11_DATA_SOURCE_LOGGER_H_

#include "media/base/data_source.h"

// Logs all DataSource operations to VLOG(1) for debugging purposes.
class DataSourceLogger : public media::DataSource {
 public:
  // Constructs a DataSourceLogger to log operations against another DataSource.
  //
  // |data_source| must be initialized in advance.
  //
  // |streaming| when set to true will override the implementation
  // IsStreaming() to always return true, otherwise it will delegate to
  // |data_source|.
  DataSourceLogger(scoped_ptr<DataSource> data_source,
                   bool force_streaming);
  virtual ~DataSourceLogger();

  // media::DataSource implementation.
  virtual void Stop(const base::Closure& closure) OVERRIDE;
  virtual void Read(
      int64 position, int size, uint8* data,
      const media::DataSource::ReadCB& read_cb) OVERRIDE;
  virtual bool GetSize(int64* size_out) OVERRIDE;
  virtual bool IsStreaming() OVERRIDE;
  virtual void SetBitrate(int bitrate) OVERRIDE;

 private:
  scoped_ptr<media::DataSource> data_source_;
  bool streaming_;

  DISALLOW_COPY_AND_ASSIGN(DataSourceLogger);
};

#endif  // MEDIA_TOOLS_PLAYER_X11_DATA_SOURCE_LOGGER_H_
