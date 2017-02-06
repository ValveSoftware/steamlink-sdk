// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "vpxpes_parser.h"

#include <cstdint>
#include <cstdio>
#include <limits>
#include <vector>

#include "common/file_util.h"
#include "common/libwebm_util.h"

namespace libwebm {

VpxPesParser::BcmvHeader::BcmvHeader(std::uint32_t len) : length(len) {
  id[0] = 'B';
  id[1] = 'C';
  id[2] = 'M';
  id[3] = 'V';
}

bool VpxPesParser::BcmvHeader::operator==(const BcmvHeader& other) const {
  return (other.length == length && other.id[0] == id[0] &&
          other.id[1] == id[1] && other.id[2] == id[2] && other.id[3] == id[3]);
}

bool VpxPesParser::BcmvHeader::Valid() const {
  return (length > 0 && length <= std::numeric_limits<std::uint16_t>::max() &&
          id[0] == 'B' && id[1] == 'C' && id[2] == 'M' && id[3] == 'V');
}

bool VpxPesParser::Open(const std::string& pes_file) {
  pes_file_size_ = static_cast<size_t>(libwebm::GetFileSize(pes_file));
  if (pes_file_size_ <= 0)
    return false;
  pes_file_data_.reserve(static_cast<size_t>(pes_file_size_));
  libwebm::FilePtr file = libwebm::FilePtr(std::fopen(pes_file.c_str(), "rb"),
                                           libwebm::FILEDeleter());

  int byte;
  while ((byte = fgetc(file.get())) != EOF)
    pes_file_data_.push_back(static_cast<std::uint8_t>(byte));

  if (!feof(file.get()) || ferror(file.get()) ||
      pes_file_size_ != pes_file_data_.size()) {
    return false;
  }

  read_pos_ = 0;
  parse_state_ = kParsePesHeader;
  return true;
}

bool VpxPesParser::VerifyPacketStartCode() const {
  if (read_pos_ + 2 > pes_file_data_.size())
    return false;

  // PES packets all start with the byte sequence 0x0 0x0 0x1.
  if (pes_file_data_[read_pos_] != 0 || pes_file_data_[read_pos_ + 1] != 0 ||
      pes_file_data_[read_pos_ + 2] != 1) {
    return false;
  }

  return true;
}

bool VpxPesParser::ReadStreamId(std::uint8_t* stream_id) const {
  if (!stream_id || BytesAvailable() < 4)
    return false;

  *stream_id = pes_file_data_[read_pos_ + 3];
  return true;
}

bool VpxPesParser::ReadPacketLength(std::uint16_t* packet_length) const {
  if (!packet_length || BytesAvailable() < 6)
    return false;

  // Read and byte swap 16 bit big endian length.
  *packet_length =
      (pes_file_data_[read_pos_ + 4] << 8) | pes_file_data_[read_pos_ + 5];

  return true;
}

bool VpxPesParser::ParsePesHeader(PesHeader* header) {
  if (!header || parse_state_ != kParsePesHeader)
    return false;

  if (!VerifyPacketStartCode())
    return false;

  std::size_t pos = read_pos_;
  for (auto& a : header->start_code) {
    a = pes_file_data_[pos++];
  }

  // PES Video stream IDs start at E0.
  if (!ReadStreamId(&header->stream_id))
    return false;

  if (header->stream_id < kMinVideoStreamId ||
      header->stream_id > kMaxVideoStreamId)
    return false;

  if (!ReadPacketLength(&header->packet_length))
    return false;

  read_pos_ += kPesHeaderSize;
  parse_state_ = kParsePesOptionalHeader;
  return true;
}

// TODO(tomfinegan): Make these masks constants.
bool VpxPesParser::ParsePesOptionalHeader(PesOptionalHeader* header) {
  if (!header || parse_state_ != kParsePesOptionalHeader)
    return false;

  size_t offset = read_pos_;
  if (offset >= pes_file_size_)
    return false;

  header->marker = (pes_file_data_[offset] & 0x80) >> 6;
  header->scrambling = (pes_file_data_[offset] & 0x30) >> 4;
  header->priority = (pes_file_data_[offset] & 0x8) >> 3;
  header->data_alignment = (pes_file_data_[offset] & 0xc) >> 2;
  header->copyright = (pes_file_data_[offset] & 0x2) >> 1;
  header->original = pes_file_data_[offset] & 0x1;

  offset++;
  header->has_pts = (pes_file_data_[offset] & 0x80) >> 7;
  header->has_dts = (pes_file_data_[offset] & 0x40) >> 6;
  header->unused_fields = pes_file_data_[offset] & 0x3f;

  offset++;
  header->remaining_size = pes_file_data_[offset];
  if (header->remaining_size != kWebm2PesOptHeaderRemainingSize)
    return false;

  size_t bytes_left = header->remaining_size;

  if (header->has_pts) {
    // Read PTS markers. Format:
    // PTS: 5 bytes
    //   4 bits (flag: PTS present, but no DTS): 0x2 ('0010')
    //   36 bits (90khz PTS):
    //     top 3 bits
    //     marker ('1')
    //     middle 15 bits
    //     marker ('1')
    //     bottom 15 bits
    //     marker ('1')
    // TODO(tomfinegan): read/store the timestamp.
    offset++;
    header->pts_dts_flag = (pes_file_data_[offset] & 0x20) >> 4;
    // Check the marker bits.
    if ((pes_file_data_[offset] & 1) != 1 ||
        (pes_file_data_[offset + 2] & 1) != 1 ||
        (pes_file_data_[offset + 2] & 1) != 1) {
      return false;
    }
    offset += 5;
    bytes_left -= 5;
  }

  // Validate stuffing byte(s).
  for (size_t i = 0; i < bytes_left; ++i) {
    if (pes_file_data_[offset + i] != 0xff)
      return false;
  }

  read_pos_ += kPesOptionalHeaderSize;
  parse_state_ = kParseBcmvHeader;

  return true;
}

// Parses and validates a BCMV header.
bool VpxPesParser::ParseBcmvHeader(BcmvHeader* header) {
  if (!header || parse_state_ != kParseBcmvHeader)
    return false;

  std::size_t offset = read_pos_;
  header->id[0] = pes_file_data_[offset++];
  header->id[1] = pes_file_data_[offset++];
  header->id[2] = pes_file_data_[offset++];
  header->id[3] = pes_file_data_[offset++];

  header->length |= pes_file_data_[offset++] << 24;
  header->length |= pes_file_data_[offset++] << 16;
  header->length |= pes_file_data_[offset++] << 8;
  header->length |= pes_file_data_[offset++];

  // Length stored in the BCMV header is followed by 2 bytes of 0 padding.
  if (pes_file_data_[offset++] != 0 || pes_file_data_[offset++] != 0)
    return false;

  if (!header->Valid())
    return false;

  // TODO(tomfinegan): Verify data instead of jumping to the next packet.
  read_pos_ += kBcmvHeaderSize + header->length;
  parse_state_ = kParsePesHeader;
  return true;
}

int VpxPesParser::BytesAvailable() const {
  return static_cast<int>(pes_file_data_.size() - read_pos_);
}

bool VpxPesParser::ParseNextPacket(PesHeader* header, VpxFrame* frame) {
  if (!header || !frame) {
    return false;
  }
  if (!ParsePesHeader(header)) {
    return false;
  }
  if (!ParsePesOptionalHeader(&header->opt_header)) {
    return false;
  }
  if (!ParseBcmvHeader(&header->bcmv_header)) {
    return false;
  }

  // TODO(tomfinegan): Process data payload/store in frame. This requires
  // changes to Webm2Pes:
  // 1. BCMV header must contain entire frame length, and pes header
  //    payload size will be what's in the current packet-- parsing code needs
  //    needs to know how to reassemble frames.
  // 2. Random access bit must be set in Adaptation Field for key frames.
  return true;
}

}  // namespace libwebm
