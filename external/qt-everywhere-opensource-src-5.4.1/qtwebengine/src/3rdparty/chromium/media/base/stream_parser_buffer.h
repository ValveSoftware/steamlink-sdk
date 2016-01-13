// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_STREAM_PARSER_BUFFER_H_
#define MEDIA_BASE_STREAM_PARSER_BUFFER_H_

#include <deque>

#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_export.h"
#include "media/base/stream_parser.h"

namespace media {

class MEDIA_EXPORT StreamParserBuffer : public DecoderBuffer {
 public:
  // Value used to signal an invalid decoder config ID.
  enum { kInvalidConfigId = -1 };

  typedef DemuxerStream::Type Type;
  typedef StreamParser::TrackId TrackId;

  static scoped_refptr<StreamParserBuffer> CreateEOSBuffer();

  static scoped_refptr<StreamParserBuffer> CopyFrom(
      const uint8* data, int data_size, bool is_keyframe, Type type,
      TrackId track_id);
  static scoped_refptr<StreamParserBuffer> CopyFrom(
      const uint8* data, int data_size,
      const uint8* side_data, int side_data_size, bool is_keyframe, Type type,
      TrackId track_id);
  bool IsKeyframe() const { return is_keyframe_; }

  // Decode timestamp. If not explicitly set, or set to kNoTimestamp(), the
  // value will be taken from the normal timestamp.
  base::TimeDelta GetDecodeTimestamp() const;
  void SetDecodeTimestamp(base::TimeDelta timestamp);

  // Gets/sets the ID of the decoder config associated with this buffer.
  int GetConfigId() const;
  void SetConfigId(int config_id);

  // Gets the parser's media type associated with this buffer. Value is
  // meaningless for EOS buffers.
  Type type() const { return type_; }

  // Gets the parser's track ID associated with this buffer. Value is
  // meaningless for EOS buffers.
  TrackId track_id() const { return track_id_; }

  // Converts this buffer to a splice buffer.  |pre_splice_buffers| must not
  // have any EOS buffers, must not have any splice buffers, nor must have any
  // buffer with preroll.
  //
  // |pre_splice_buffers| will be deep copied and each copy's splice_timestamp()
  // will be set to this buffer's splice_timestamp().  A copy of |this|, with a
  // splice_timestamp() of kNoTimestamp(), will be added to the end of
  // |splice_buffers_|.
  //
  // See the Audio Splice Frame Algorithm in the MSE specification for details.
  typedef StreamParser::BufferQueue BufferQueue;
  void ConvertToSpliceBuffer(const BufferQueue& pre_splice_buffers);
  const BufferQueue& splice_buffers() const { return splice_buffers_; }

  // Specifies a buffer which must be decoded prior to this one to ensure this
  // buffer can be accurately decoded.  The given buffer must be of the same
  // type, must not be a splice buffer, must not have any discard padding, and
  // must not be an end of stream buffer.  |preroll| is not copied.
  //
  // It's expected that this preroll buffer will be discarded entirely post
  // decoding.  As such it's discard_padding() will be set to kInfiniteDuration.
  //
  // All future timestamp, decode timestamp, config id, or track id changes to
  // this buffer will be applied to the preroll buffer as well.
  void SetPrerollBuffer(const scoped_refptr<StreamParserBuffer>& preroll);
  const scoped_refptr<StreamParserBuffer>& preroll_buffer() {
    return preroll_buffer_;
  }

  virtual void set_timestamp(base::TimeDelta timestamp) OVERRIDE;

 private:
  StreamParserBuffer(const uint8* data, int data_size,
                     const uint8* side_data, int side_data_size,
                     bool is_keyframe, Type type,
                     TrackId track_id);
  virtual ~StreamParserBuffer();

  bool is_keyframe_;
  base::TimeDelta decode_timestamp_;
  int config_id_;
  Type type_;
  TrackId track_id_;
  BufferQueue splice_buffers_;
  scoped_refptr<StreamParserBuffer> preroll_buffer_;

  DISALLOW_COPY_AND_ASSIGN(StreamParserBuffer);
};

}  // namespace media

#endif  // MEDIA_BASE_STREAM_PARSER_BUFFER_H_
