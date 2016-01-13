// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/time/time.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/test_data_util.h"
#include "media/filters/h264_parser.h"
#include "media/formats/mp2t/es_parser_h264.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
class VideoDecoderConfig;

namespace mp2t {

namespace {

struct Packet {
  // Offset in the stream.
  size_t offset;

  // Size of the packet.
  size_t size;

  // Timestamp of the packet.
  base::TimeDelta pts;
};

// Compute the size of each packet assuming packets are given in stream order
// and the last packet covers the end of the stream.
void ComputePacketSize(std::vector<Packet>& packets, size_t stream_size) {
  for (size_t k = 0; k < packets.size() - 1; k++) {
    DCHECK_GE(packets[k + 1].offset, packets[k].offset);
    packets[k].size = packets[k + 1].offset - packets[k].offset;
  }
  packets[packets.size() - 1].size =
      stream_size - packets[packets.size() - 1].offset;
}

// Get the offset of the start of each access unit.
// This function assumes there is only one slice per access unit.
// This is a very simplified access unit segmenter that is good
// enough for unit tests.
std::vector<Packet> GetAccessUnits(const uint8* stream, size_t stream_size) {
  std::vector<Packet> access_units;
  bool start_access_unit = true;

  // In a first pass, retrieve the offsets of all access units.
  size_t offset = 0;
  while (true) {
    // Find the next start code.
    off_t relative_offset = 0;
    off_t start_code_size = 0;
    bool success = H264Parser::FindStartCode(
        &stream[offset], stream_size - offset,
        &relative_offset, &start_code_size);
    if (!success)
      break;
    offset += relative_offset;

    if (start_access_unit) {
      Packet cur_access_unit;
      cur_access_unit.offset = offset;
      access_units.push_back(cur_access_unit);
      start_access_unit = false;
    }

    // Get the NALU type.
    offset += start_code_size;
    if (offset >= stream_size)
      break;
    int nal_unit_type = stream[offset] & 0x1f;

    // We assume there is only one slice per access unit.
    if (nal_unit_type == H264NALU::kIDRSlice ||
        nal_unit_type == H264NALU::kNonIDRSlice) {
      start_access_unit = true;
    }
  }

  ComputePacketSize(access_units, stream_size);
  return access_units;
}

// Append an AUD NALU at the beginning of each access unit
// needed for streams which do not already have AUD NALUs.
void AppendAUD(
    const uint8* stream, size_t stream_size,
    const std::vector<Packet>& access_units,
    std::vector<uint8>& stream_with_aud,
    std::vector<Packet>& access_units_with_aud) {
  uint8 aud[] = { 0x00, 0x00, 0x01, 0x09 };
  stream_with_aud.resize(stream_size + access_units.size() * sizeof(aud));
  access_units_with_aud.resize(access_units.size());

  size_t offset = 0;
  for (size_t k = 0; k < access_units.size(); k++) {
    access_units_with_aud[k].offset = offset;
    access_units_with_aud[k].size = access_units[k].size + sizeof(aud);

    memcpy(&stream_with_aud[offset], aud, sizeof(aud));
    offset += sizeof(aud);

    memcpy(&stream_with_aud[offset],
           &stream[access_units[k].offset], access_units[k].size);
    offset += access_units[k].size;
  }
}

}  // namespace

class EsParserH264Test : public testing::Test {
 public:
  EsParserH264Test() : buffer_count_(0) {
  }
  virtual ~EsParserH264Test() {}

 protected:
  void LoadStream(const char* filename);
  void GetPesTimestamps(std::vector<Packet>& pes_packets);
  void ProcessPesPackets(const std::vector<Packet>& pes_packets,
                         bool force_timing);

  // Stream with AUD NALUs.
  std::vector<uint8> stream_;

  // Access units of the stream with AUD NALUs.
  std::vector<Packet> access_units_;

  // Number of buffers generated while parsing the H264 stream.
  size_t buffer_count_;

 private:
  void EmitBuffer(scoped_refptr<StreamParserBuffer> buffer);

  void NewVideoConfig(const VideoDecoderConfig& config) {
  }

  DISALLOW_COPY_AND_ASSIGN(EsParserH264Test);
};

void EsParserH264Test::LoadStream(const char* filename) {
  base::FilePath file_path = GetTestDataFilePath(filename);

  base::MemoryMappedFile stream_without_aud;
  ASSERT_TRUE(stream_without_aud.Initialize(file_path))
      << "Couldn't open stream file: " << file_path.MaybeAsASCII();

  // The input file does not have AUDs.
  std::vector<Packet> access_units_without_aud = GetAccessUnits(
      stream_without_aud.data(), stream_without_aud.length());
  ASSERT_GT(access_units_without_aud.size(), 0u);
  AppendAUD(stream_without_aud.data(), stream_without_aud.length(),
            access_units_without_aud,
            stream_, access_units_);

  // Generate some timestamps based on a 25fps stream.
  for (size_t k = 0; k < access_units_.size(); k++)
    access_units_[k].pts = base::TimeDelta::FromMilliseconds(k * 40u);
}

void EsParserH264Test::GetPesTimestamps(std::vector<Packet>& pes_packets) {
  // Default: set to a negative timestamp to be able to differentiate from
  // real timestamps.
  // Note: we don't use kNoTimestamp() here since this one has already
  // a special meaning in EsParserH264. The negative timestamps should be
  // ultimately discarded by the H264 parser since not relevant.
  for (size_t k = 0; k < pes_packets.size(); k++) {
    pes_packets[k].pts = base::TimeDelta::FromMilliseconds(-1);
  }

  // Set a valid timestamp for PES packets which include the start
  // of an H264 access unit.
  size_t pes_idx = 0;
  for (size_t k = 0; k < access_units_.size(); k++) {
    for (; pes_idx < pes_packets.size(); pes_idx++) {
      size_t pes_start = pes_packets[pes_idx].offset;
      size_t pes_end = pes_packets[pes_idx].offset + pes_packets[pes_idx].size;
      if (pes_start <= access_units_[k].offset &&
          pes_end > access_units_[k].offset) {
        pes_packets[pes_idx].pts = access_units_[k].pts;
        break;
      }
    }
  }
}

void EsParserH264Test::ProcessPesPackets(
    const std::vector<Packet>& pes_packets,
    bool force_timing) {
  EsParserH264 es_parser(
      base::Bind(&EsParserH264Test::NewVideoConfig, base::Unretained(this)),
      base::Bind(&EsParserH264Test::EmitBuffer, base::Unretained(this)));

  for (size_t k = 0; k < pes_packets.size(); k++) {
    size_t cur_pes_offset = pes_packets[k].offset;
    size_t cur_pes_size = pes_packets[k].size;

    base::TimeDelta pts = kNoTimestamp();
    base::TimeDelta dts = kNoTimestamp();
    if (pes_packets[k].pts >= base::TimeDelta() || force_timing)
      pts = pes_packets[k].pts;

    ASSERT_TRUE(
        es_parser.Parse(&stream_[cur_pes_offset], cur_pes_size, pts, dts));
  }
  es_parser.Flush();
}

void EsParserH264Test::EmitBuffer(scoped_refptr<StreamParserBuffer> buffer) {
  ASSERT_LT(buffer_count_, access_units_.size());
  EXPECT_EQ(buffer->timestamp(), access_units_[buffer_count_].pts);
  buffer_count_++;
}

TEST_F(EsParserH264Test, OneAccessUnitPerPes) {
  LoadStream("bear.h264");

  // One to one equivalence between PES packets and access units.
  std::vector<Packet> pes_packets(access_units_);
  GetPesTimestamps(pes_packets);

  // Process each PES packet.
  ProcessPesPackets(pes_packets, false);
  EXPECT_EQ(buffer_count_, access_units_.size());
}

TEST_F(EsParserH264Test, NonAlignedPesPacket) {
  LoadStream("bear.h264");

  // Generate the PES packets.
  std::vector<Packet> pes_packets;
  Packet cur_pes_packet;
  cur_pes_packet.offset = 0;
  for (size_t k = 0; k < access_units_.size(); k++) {
    pes_packets.push_back(cur_pes_packet);

    // The current PES packet includes the remaining bytes of the previous
    // access unit and some bytes of the current access unit
    // (487 bytes in this unit test but no more than the current access unit
    // size).
    cur_pes_packet.offset = access_units_[k].offset +
        std::min<size_t>(487u, access_units_[k].size);
  }
  ComputePacketSize(pes_packets, stream_.size());
  GetPesTimestamps(pes_packets);

  // Process each PES packet.
  ProcessPesPackets(pes_packets, false);
  EXPECT_EQ(buffer_count_, access_units_.size());
}

TEST_F(EsParserH264Test, SeveralPesPerAccessUnit) {
  LoadStream("bear.h264");

  // Get the minimum size of an access unit.
  size_t min_access_unit_size = stream_.size();
  for (size_t k = 0; k < access_units_.size(); k++) {
    if (min_access_unit_size >= access_units_[k].size)
      min_access_unit_size = access_units_[k].size;
  }

  // Use a small PES packet size or the minimum access unit size
  // if it is even smaller.
  size_t pes_size = 512;
  if (min_access_unit_size < pes_size)
    pes_size = min_access_unit_size;

  std::vector<Packet> pes_packets;
  Packet cur_pes_packet;
  cur_pes_packet.offset = 0;
  while (cur_pes_packet.offset < stream_.size()) {
    pes_packets.push_back(cur_pes_packet);
    cur_pes_packet.offset += pes_size;
  }
  ComputePacketSize(pes_packets, stream_.size());
  GetPesTimestamps(pes_packets);

  // Process each PES packet.
  ProcessPesPackets(pes_packets, false);
  EXPECT_EQ(buffer_count_, access_units_.size());

  // Process PES packets forcing timings for each PES packet.
  buffer_count_ = 0;
  ProcessPesPackets(pes_packets, true);
  EXPECT_EQ(buffer_count_, access_units_.size());
}

}  // namespace mp2t
}  // namespace media

