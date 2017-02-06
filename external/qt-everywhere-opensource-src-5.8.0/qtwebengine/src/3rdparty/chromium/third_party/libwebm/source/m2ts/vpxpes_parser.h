// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef LIBWEBM_M2TS_VPXPES_PARSER_H_
#define LIBWEBM_M2TS_VPXPES_PARSER_H_

#include <cstdint>
#include <string>
#include <vector>

namespace libwebm {

// Parser for VPx PES. Requires that the _entire_ PES stream can be stored in
// a std::vector<std::uint8_t> and read into memory when Open() is called.
// TODO(tomfinegan): Support incremental parse.
class VpxPesParser {
 public:
  typedef std::vector<std::uint8_t> PesFileData;

  enum ParseState {
    kParsePesHeader,
    kParsePesOptionalHeader,
    kParseBcmvHeader,
  };

  struct VpxFrame {
    enum Codec {
      VP8,
      VP9,
    };

    Codec codec = VP9;
    bool keyframe = false;

    // Frame data.
    std::vector<std::uint8_t> data;

    // Raw PES PTS.
    std::int64_t pts = 0;
  };

  struct PesOptionalHeader {
    int marker = 0;
    int scrambling = 0;
    int priority = 0;
    int data_alignment = 0;
    int copyright = 0;
    int original = 0;
    int has_pts = 0;
    int has_dts = 0;
    int unused_fields = 0;
    int remaining_size = 0;
    int pts_dts_flag = 0;
    std::uint64_t pts = 0;
    int stuffing_byte = 0;
  };

  struct BcmvHeader {
    BcmvHeader() = default;
    ~BcmvHeader() = default;
    BcmvHeader(const BcmvHeader&) = delete;
    BcmvHeader(BcmvHeader&&) = delete;

    // Convenience ctor for quick validation of expected values via operator==
    // after parsing input.
    explicit BcmvHeader(std::uint32_t len);

    bool operator==(const BcmvHeader& other) const;

    bool Valid() const;

    char id[4] = {0};
    std::uint32_t length = 0;
  };

  struct PesHeader {
    std::uint8_t start_code[4] = {0};
    std::uint16_t packet_length = 0;
    std::uint8_t stream_id = 0;
    PesOptionalHeader opt_header;
    BcmvHeader bcmv_header;
  };

  // Constants for validating known values from input data.
  const std::uint8_t kMinVideoStreamId = 0xE0;
  const std::uint8_t kMaxVideoStreamId = 0xEF;
  const int kPesHeaderSize = 6;
  const int kPesOptionalHeaderStartOffset = kPesHeaderSize;
  const int kPesOptionalHeaderSize = 9;
  const int kPesOptionalHeaderMarkerValue = 0x2;
  const int kWebm2PesOptHeaderRemainingSize = 6;
  const int kBcmvHeaderSize = 10;

  VpxPesParser() = default;
  ~VpxPesParser() = default;

  // Opens file specified by |pes_file_path| and reads its contents. Returns
  // true after successful read of input file.
  bool Open(const std::string& pes_file_path);

  // Parses the next packet in the PES. PES header information is stored in
  // |header|, and the frame payload is stored in |frame|. Returns true when
  // packet is parsed successfully.
  bool ParseNextPacket(PesHeader* header, VpxFrame* frame);

  // PES Header parsing utility functions.
  // PES Header structure:
  // Start code         Stream ID   Packet length (16 bits)
  // /                  /      ____/
  // |                  |     /
  // Byte0 Byte1  Byte2 Byte3 Byte4 Byte5
  //     0     0      1     X           Y
  bool VerifyPacketStartCode() const;
  bool ReadStreamId(std::uint8_t* stream_id) const;
  bool ReadPacketLength(std::uint16_t* packet_length) const;

  std::uint64_t pes_file_size() const { return pes_file_size_; }
  const PesFileData& pes_file_data() const { return pes_file_data_; }

  // Returns number of unparsed bytes remaining.
  int BytesAvailable() const;

 private:
  // Parses and verifies the static 6 byte portion that begins every PES packet.
  bool ParsePesHeader(PesHeader* header);

  // Parses a PES optional header, the optional header following the static
  // header that begins the VPX PES packet.
  // https://en.wikipedia.org/wiki/Packetized_elementary_stream
  bool ParsePesOptionalHeader(PesOptionalHeader* header);

  // Parses and validates the BCMV header. This immediately follows the optional
  // header.
  bool ParseBcmvHeader(BcmvHeader* header);

  std::size_t pes_file_size_ = 0;
  PesFileData pes_file_data_;
  std::size_t read_pos_ = 0;
  ParseState parse_state_ = kParsePesHeader;
};

}  // namespace libwebm

#endif  // LIBWEBM_M2TS_VPXPES_PARSER_H_
