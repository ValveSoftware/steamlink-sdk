// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_headers_block_parser.h"

#include "base/sys_byteorder.h"

namespace net {

const size_t SpdyHeadersBlockParser::kMaximumFieldLength = 16 * 1024;

SpdyHeadersBlockParser::SpdyHeadersBlockParser(
    SpdyMajorVersion spdy_version,
    SpdyHeadersHandlerInterface* handler) :
    state_(READING_HEADER_BLOCK_LEN),
    length_field_size_(LengthFieldSizeForVersion(spdy_version)),
    max_headers_in_block_(MaxNumberOfHeadersForVersion(spdy_version)),
    total_bytes_received_(0),
    remaining_key_value_pairs_for_frame_(0),
    handler_(handler),
    error_(OK) {
  // The handler that we set must not be NULL.
  DCHECK(handler_ != NULL);
}

SpdyHeadersBlockParser::~SpdyHeadersBlockParser() {}

bool SpdyHeadersBlockParser::HandleControlFrameHeadersData(
    SpdyStreamId stream_id,
    const char* headers_data,
    size_t headers_data_length) {
  if (error_ == NEED_MORE_DATA) {
    error_ = OK;
  }
  CHECK_EQ(error_, OK);

  // If this is the first call with the current header block,
  // save its stream id.
  if (state_ == READING_HEADER_BLOCK_LEN) {
    stream_id_ = stream_id;
  }
  CHECK_EQ(stream_id_, stream_id);

  total_bytes_received_ += headers_data_length;

  SpdyPinnableBufferPiece prefix, key, value;
  // Simultaneously tie lifetimes to the stack, and clear member variables.
  prefix.Swap(&headers_block_prefix_);
  key.Swap(&key_);

  // Apply the parsing state machine to the remaining prefix
  // from last invocation, plus newly-available headers data.
  Reader reader(prefix.buffer(), prefix.length(),
                headers_data, headers_data_length);
  while (error_ == OK) {
    ParserState next_state(FINISHED_HEADER);

    switch (state_) {
      case READING_HEADER_BLOCK_LEN:
        next_state = READING_KEY_LEN;
        ParseBlockLength(&reader);
        break;
      case READING_KEY_LEN:
        next_state = READING_KEY;
        ParseFieldLength(&reader);
        break;
      case READING_KEY:
        next_state = READING_VALUE_LEN;
        if (!reader.ReadN(next_field_length_, &key)) {
          error_ = NEED_MORE_DATA;
        }
        break;
      case READING_VALUE_LEN:
        next_state = READING_VALUE;
        ParseFieldLength(&reader);
        break;
      case READING_VALUE:
        next_state = FINISHED_HEADER;
        if (!reader.ReadN(next_field_length_, &value)) {
          error_ = NEED_MORE_DATA;
        } else {
          handler_->OnHeader(stream_id, key, value);
        }
        break;
      case FINISHED_HEADER:
        // Prepare for next header or block.
        if (--remaining_key_value_pairs_for_frame_ > 0) {
          next_state = READING_KEY_LEN;
        } else {
          next_state = READING_HEADER_BLOCK_LEN;
          handler_->OnHeaderBlockEnd(stream_id, total_bytes_received_);
          // Expect to have consumed all buffer.
          if (reader.Available() != 0) {
            error_ = TOO_MUCH_DATA;
          }
        }
        break;
      default:
        CHECK(false) << "Not reached.";
    }

    if (error_ == OK) {
      state_ = next_state;

      if (next_state == READING_HEADER_BLOCK_LEN) {
        // We completed reading a full header block. Return to caller.
        total_bytes_received_ = 0;
        break;
      }
    } else if (error_ == NEED_MORE_DATA) {
      // We can't continue parsing until more data is available. Make copies of
      // the key and buffer remainder, in preperation for the next invocation.
      if (state_ > READING_KEY) {
        key_.Swap(&key);
        key_.Pin();
      }
      reader.ReadN(reader.Available(), &headers_block_prefix_);
      headers_block_prefix_.Pin();
    }
  }
  return error_ == OK;
}

void SpdyHeadersBlockParser::ParseBlockLength(Reader* reader) {
  ParseLength(reader, &remaining_key_value_pairs_for_frame_);
  if (error_ == OK &&
    remaining_key_value_pairs_for_frame_ > max_headers_in_block_) {
    error_ = HEADER_BLOCK_TOO_LARGE;
  }
  if (error_ == OK) {
    handler_->OnHeaderBlock(stream_id_, remaining_key_value_pairs_for_frame_);
  }
}

void SpdyHeadersBlockParser::ParseFieldLength(Reader* reader) {
  ParseLength(reader, &next_field_length_);
  if (error_ == OK &&
      next_field_length_ > kMaximumFieldLength) {
    error_ = HEADER_FIELD_TOO_LARGE;
  }
}

void SpdyHeadersBlockParser::ParseLength(Reader* reader,
                                         uint32_t* parsed_length) {
  char buffer[] = {0, 0, 0, 0};
  if (!reader->ReadN(length_field_size_, buffer)) {
    error_ = NEED_MORE_DATA;
    return;
  }
  // Convert from network to host order and return the parsed out integer.
  if (length_field_size_ == sizeof(uint32_t)) {
    *parsed_length = ntohl(*reinterpret_cast<const uint32_t *>(buffer));
  } else {
    *parsed_length = ntohs(*reinterpret_cast<const uint16_t *>(buffer));
  }
}

void SpdyHeadersBlockParser::Reset() {
  {
    SpdyPinnableBufferPiece empty;
    headers_block_prefix_.Swap(&empty);
  }
  {
    SpdyPinnableBufferPiece empty;
    key_.Swap(&empty);
  }
  error_ = OK;
  state_ = READING_HEADER_BLOCK_LEN;
  stream_id_ = 0;
  total_bytes_received_ = 0;
}

size_t SpdyHeadersBlockParser::LengthFieldSizeForVersion(
    SpdyMajorVersion spdy_version) {
  if (spdy_version < SPDY3) {
    return sizeof(uint16_t);
  }
  return sizeof(uint32_t);
}

size_t SpdyHeadersBlockParser::MaxNumberOfHeadersForVersion(
    SpdyMajorVersion spdy_version) {
  // Account for the length of the header block field.
  size_t max_bytes_for_headers =
      kMaximumFieldLength - LengthFieldSizeForVersion(spdy_version);

  // A minimal size header is twice the length field size (and has a
  // zero-lengthed key and a zero-lengthed value).
  return max_bytes_for_headers / (2 * LengthFieldSizeForVersion(spdy_version));
}

}  // namespace net
