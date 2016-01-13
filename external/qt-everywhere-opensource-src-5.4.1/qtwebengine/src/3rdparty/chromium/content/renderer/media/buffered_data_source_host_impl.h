// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_BUFFERED_DATA_SOURCE_HOST_IMPL_H_
#define CONTENT_RENDERER_MEDIA_BUFFERED_DATA_SOURCE_HOST_IMPL_H_

#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/renderer/media/buffered_data_source.h"
#include "media/base/ranges.h"

namespace content {

// Provides an implementation of BufferedDataSourceHost that translates the
// buffered byte ranges into estimated time ranges.
class CONTENT_EXPORT BufferedDataSourceHostImpl
    : public BufferedDataSourceHost {
 public:
  BufferedDataSourceHostImpl();
  virtual ~BufferedDataSourceHostImpl();

  // BufferedDataSourceHost implementation.
  virtual void SetTotalBytes(int64 total_bytes) OVERRIDE;
  virtual void AddBufferedByteRange(int64 start, int64 end) OVERRIDE;

  // Translate the byte ranges to time ranges and append them to the list.
  // TODO(sandersd): This is a confusing name, find something better.
  void AddBufferedTimeRanges(
      media::Ranges<base::TimeDelta>* buffered_time_ranges,
      base::TimeDelta media_duration) const;

  bool DidLoadingProgress();

 private:
  // Total size of the data source.
  int64 total_bytes_;

  // List of buffered byte ranges for estimating buffered time.
  media::Ranges<int64> buffered_byte_ranges_;

  // True when AddBufferedByteRange() has been called more recently than
  // DidLoadingProgress().
  bool did_loading_progress_;

  DISALLOW_COPY_AND_ASSIGN(BufferedDataSourceHostImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_BUFFERED_DATA_SOURCE_HOST_IMPL_H_
