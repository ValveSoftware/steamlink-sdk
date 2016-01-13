// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/hpack_decoder.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "net/spdy/hpack_constants.h"
#include "net/spdy/hpack_output_stream.h"

namespace net {

using base::StringPiece;
using std::string;

namespace {

const uint8 kNoState = 0;
// Set on entries added to the reference set during this decoding.
const uint8 kReferencedThisEncoding = 1;

const char kCookieKey[] = "cookie";

}  // namespace

HpackDecoder::HpackDecoder(const HpackHuffmanTable& table)
    : max_string_literal_size_(kDefaultMaxStringLiteralSize),
      huffman_table_(table) {}

HpackDecoder::~HpackDecoder() {}

bool HpackDecoder::HandleControlFrameHeadersData(SpdyStreamId id,
                                                 const char* headers_data,
                                                 size_t headers_data_length) {
  decoded_block_.clear();

  size_t new_size = headers_block_buffer_.size() + headers_data_length;
  if (new_size > kMaxDecodeBufferSize) {
    return false;
  }
  headers_block_buffer_.insert(headers_block_buffer_.end(),
                               headers_data,
                               headers_data + headers_data_length);
  return true;
}

bool HpackDecoder::HandleControlFrameHeadersComplete(SpdyStreamId id) {
  HpackInputStream input_stream(max_string_literal_size_,
                                headers_block_buffer_);
  while (input_stream.HasMoreData()) {
    if (!DecodeNextOpcode(&input_stream)) {
      headers_block_buffer_.clear();
      return false;
    }
  }
  headers_block_buffer_.clear();

  // Emit everything in the reference set that hasn't already been emitted.
  // Also clear entry state for the next decoded headers block.
  // TODO(jgraettinger): We may need to revisit the order in which headers
  // are emitted (b/14051713).
  for (HpackHeaderTable::OrderedEntrySet::const_iterator it =
          header_table_.reference_set().begin();
       it != header_table_.reference_set().end(); ++it) {
    HpackEntry* entry = *it;

    if (entry->state() == kNoState) {
      HandleHeaderRepresentation(entry->name(), entry->value());
    } else {
      entry->set_state(kNoState);
    }
  }
  // Emit the Cookie header, if any crumbles were encountered.
  if (!cookie_value_.empty()) {
    decoded_block_[kCookieKey] = cookie_value_;
    cookie_value_.clear();
  }
  return true;
}

void HpackDecoder::HandleHeaderRepresentation(StringPiece name,
                                              StringPiece value) {
  typedef std::pair<std::map<string, string>::iterator, bool> InsertResult;

  if (name == kCookieKey) {
    if (cookie_value_.empty()) {
      cookie_value_.assign(value.data(), value.size());
    } else {
      cookie_value_ += "; ";
      cookie_value_.insert(cookie_value_.end(), value.begin(), value.end());
    }
  } else {
    InsertResult result = decoded_block_.insert(
        std::make_pair(name.as_string(), value.as_string()));
    if (!result.second) {
      result.first->second.push_back('\0');
      result.first->second.insert(result.first->second.end(),
                                  value.begin(),
                                  value.end());
    }
  }
}

bool HpackDecoder::DecodeNextOpcode(HpackInputStream* input_stream) {
  // Implements 4.2: Indexed Header Field Representation.
  if (input_stream->MatchPrefixAndConsume(kIndexedOpcode)) {
    return DecodeNextIndexedHeader(input_stream);
  }
  // Implements 4.3.1: Literal Header Field without Indexing.
  if (input_stream->MatchPrefixAndConsume(kLiteralNoIndexOpcode)) {
    return DecodeNextLiteralHeader(input_stream, false);
  }
  // Implements 4.3.2: Literal Header Field with Incremental Indexing.
  if (input_stream->MatchPrefixAndConsume(kLiteralIncrementalIndexOpcode)) {
    return DecodeNextLiteralHeader(input_stream, true);
  }
  // Implements 4.3.3: Literal Header Field never Indexed.
  // TODO(jgraettinger): Preserve the never-indexed bit.
  if (input_stream->MatchPrefixAndConsume(kLiteralNeverIndexOpcode)) {
    return DecodeNextLiteralHeader(input_stream, false);
  }
  // Implements 4.4: Encoding context update.
  if (input_stream->MatchPrefixAndConsume(kEncodingContextOpcode)) {
    return DecodeNextContextUpdate(input_stream);
  }
  // Unrecognized opcode.
  return false;
}

bool HpackDecoder::DecodeNextContextUpdate(HpackInputStream* input_stream) {
  if (input_stream->MatchPrefixAndConsume(kEncodingContextEmptyReferenceSet)) {
    header_table_.ClearReferenceSet();
    return true;
  }
  if (input_stream->MatchPrefixAndConsume(kEncodingContextNewMaximumSize)) {
    uint32 size = 0;
    if (!input_stream->DecodeNextUint32(&size)) {
      return false;
    }
    if (size > header_table_.settings_size_bound()) {
      return false;
    }
    header_table_.SetMaxSize(size);
    return true;
  }
  // Unrecognized encoding context update.
  return false;
}

bool HpackDecoder::DecodeNextIndexedHeader(HpackInputStream* input_stream) {
  uint32 index = 0;
  if (!input_stream->DecodeNextUint32(&index))
    return false;

  HpackEntry* entry = header_table_.GetByIndex(index);
  if (entry == NULL)
    return false;

  if (entry->IsStatic()) {
    HandleHeaderRepresentation(entry->name(), entry->value());

    HpackEntry* new_entry = header_table_.TryAddEntry(
        entry->name(), entry->value());
    if (new_entry) {
      header_table_.Toggle(new_entry);
      new_entry->set_state(kReferencedThisEncoding);
    }
  } else {
    entry->set_state(kNoState);
    if (header_table_.Toggle(entry)) {
      HandleHeaderRepresentation(entry->name(), entry->value());
      entry->set_state(kReferencedThisEncoding);
    }
  }
  return true;
}

bool HpackDecoder::DecodeNextLiteralHeader(HpackInputStream* input_stream,
                                           bool should_index) {
  StringPiece name;
  if (!DecodeNextName(input_stream, &name))
    return false;

  StringPiece value;
  if (!DecodeNextStringLiteral(input_stream, false, &value))
    return false;

  HandleHeaderRepresentation(name, value);

  if (!should_index)
    return true;

  HpackEntry* new_entry = header_table_.TryAddEntry(name, value);
  if (new_entry) {
    header_table_.Toggle(new_entry);
    new_entry->set_state(kReferencedThisEncoding);
  }
  return true;
}

bool HpackDecoder::DecodeNextName(
    HpackInputStream* input_stream, StringPiece* next_name) {
  uint32 index_or_zero = 0;
  if (!input_stream->DecodeNextUint32(&index_or_zero))
    return false;

  if (index_or_zero == 0)
    return DecodeNextStringLiteral(input_stream, true, next_name);

  const HpackEntry* entry = header_table_.GetByIndex(index_or_zero);
  if (entry == NULL) {
    return false;
  } else if (entry->IsStatic()) {
    *next_name = entry->name();
  } else {
    // |entry| could be evicted as part of this insertion. Preemptively copy.
    key_buffer_.assign(entry->name());
    *next_name = key_buffer_;
  }
  return true;
}

bool HpackDecoder::DecodeNextStringLiteral(HpackInputStream* input_stream,
                                           bool is_key,
                                           StringPiece* output) {
  if (input_stream->MatchPrefixAndConsume(kStringLiteralHuffmanEncoded)) {
    string* buffer = is_key ? &key_buffer_ : &value_buffer_;
    bool result = input_stream->DecodeNextHuffmanString(huffman_table_, buffer);
    *output = StringPiece(*buffer);
    return result;
  } else if (input_stream->MatchPrefixAndConsume(
      kStringLiteralIdentityEncoded)) {
    return input_stream->DecodeNextIdentityString(output);
  } else {
    return false;
  }
}

}  // namespace net
