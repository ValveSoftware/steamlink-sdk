// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBSOURCEBUFFER_IMPL_H_
#define CONTENT_RENDERER_MEDIA_WEBSOURCEBUFFER_IMPL_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "third_party/WebKit/public/platform/WebSourceBuffer.h"

namespace media {
class ChunkDemuxer;
}

namespace content {

class WebSourceBufferImpl : public blink::WebSourceBuffer {
 public:
  WebSourceBufferImpl(const std::string& id, media::ChunkDemuxer* demuxer);
  virtual ~WebSourceBufferImpl();

  // blink::WebSourceBuffer implementation.
  virtual bool setMode(AppendMode mode);
  virtual blink::WebTimeRanges buffered();
  virtual void append(
      const unsigned char* data,
      unsigned length,
      double* timestamp_offset);
  virtual void abort();
  virtual void remove(double start, double end);
  virtual bool setTimestampOffset(double offset);
  virtual void setAppendWindowStart(double start);
  virtual void setAppendWindowEnd(double end);
  virtual void removedFromMediaSource();

 private:
  std::string id_;
  media::ChunkDemuxer* demuxer_;  // Owned by WebMediaPlayerImpl.

  // Controls the offset applied to timestamps when processing appended media
  // segments. It is initially 0, which indicates that no offset is being
  // applied. Both setTimestampOffset() and append() may update this value.
  base::TimeDelta timestamp_offset_;

  base::TimeDelta append_window_start_;
  base::TimeDelta append_window_end_;

  DISALLOW_COPY_AND_ASSIGN(WebSourceBufferImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBSOURCEBUFFER_IMPL_H_
