// Copyright (c) 2015 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "m2ts/webm2pes.h"

#include <cstdio>
#include <cstring>
#include <new>
#include <vector>

#include "common/libwebm_util.h"

namespace {

bool GetPacketPayloadRanges(const libwebm::PesHeader& header,
                            const libwebm::Ranges& frame_ranges,
                            libwebm::Ranges* packet_payload_ranges) {
  // TODO(tomfinegan): The length field in PES is actually number of bytes that
  // follow the length field, and does not include the 6 byte fixed portion of
  // the header (4 byte start code + 2 bytes for the length). We can fit in 6
  // more bytes if we really want to, and avoid packetization when size is very
  // close to UINT16_MAX.
  if (packet_payload_ranges == nullptr) {
    std::fprintf(stderr, "Webm2Pes: nullptr getting payload ranges.\n");
    return false;
  }

  const std::size_t kMaxPacketPayloadSize = UINT16_MAX - header.size();

  for (const libwebm::Range& frame_range : frame_ranges) {
    if (frame_range.length + header.size() > kMaxPacketPayloadSize) {
      // make packet ranges until range.length is exhausted
      const std::size_t kBytesToPacketize = frame_range.length;
      std::size_t packet_payload_length = 0;
      for (std::size_t pos = 0; pos < kBytesToPacketize;
           pos += packet_payload_length) {
        packet_payload_length =
            (frame_range.length - pos < kMaxPacketPayloadSize) ?
                frame_range.length - pos :
                kMaxPacketPayloadSize;
        packet_payload_ranges->push_back(
            libwebm::Range(frame_range.offset + pos, packet_payload_length));
      }
    } else {
      // copy range into |packet_ranges|
      packet_payload_ranges->push_back(
          libwebm::Range(frame_range.offset, frame_range.length));
    }
  }
  return true;
}

}  // namespace

namespace libwebm {

//
// PesOptionalHeader methods.
//

void PesOptionalHeader::SetPtsBits(std::int64_t pts_90khz) {
  std::uint64_t* pts_bits = &pts.bits;
  *pts_bits = 0;

  // PTS is broken up and stored in 40 bits as shown:
  //
  //  PES PTS Only flag
  // /                  Marker              Marker              Marker
  // |                 /                   /                   /
  // |                 |                   |                   |
  // 7654  321         0  765432107654321  0  765432107654321  0
  // 0010  PTS 32-30   1  PTS 29-15        1  PTS 14-0         1
  const std::uint32_t pts1 = (pts_90khz >> 30) & 0x7;
  const std::uint32_t pts2 = (pts_90khz >> 15) & 0x7FFF;
  const std::uint32_t pts3 = pts_90khz & 0x7FFF;

  std::uint8_t buffer[5] = {0};
  // PTS only flag.
  buffer[0] |= 1 << 5;
  // Top 3 bits of PTS and 1 bit marker.
  buffer[0] |= pts1 << 1;
  // Marker.
  buffer[0] |= 1;

  // Next 15 bits of pts and 1 bit marker.
  // Top 8 bits of second PTS chunk.
  buffer[1] |= (pts2 >> 7) & 0xff;
  // bottom 7 bits of second PTS chunk.
  buffer[2] |= (pts2 << 1);
  // Marker.
  buffer[2] |= 1;

  // Last 15 bits of pts and 1 bit marker.
  // Top 8 bits of second PTS chunk.
  buffer[3] |= (pts3 >> 7) & 0xff;
  // bottom 7 bits of second PTS chunk.
  buffer[4] |= (pts3 << 1);
  // Marker.
  buffer[4] |= 1;

  // Write bits into PesHeaderField.
  std::memcpy(reinterpret_cast<std::uint8_t*>(pts_bits), buffer, 5);
}

// Writes fields to |file| and returns true. Returns false when write or
// field value validation fails.
bool PesOptionalHeader::Write(bool write_pts, PacketDataBuffer* buffer) const {
  if (buffer == nullptr) {
    std::fprintf(stderr, "Webm2Pes: nullptr in opt header writer.\n");
    return false;
  }

  const int kHeaderSize = 9;
  std::uint8_t header[kHeaderSize] = {0};
  std::uint8_t* byte = header;

  if (marker.Check() != true || scrambling.Check() != true ||
      priority.Check() != true || data_alignment.Check() != true ||
      copyright.Check() != true || original.Check() != true ||
      has_pts.Check() != true || has_dts.Check() != true ||
      pts.Check() != true || stuffing_byte.Check() != true) {
    std::fprintf(stderr, "Webm2Pes: Invalid PES Optional Header field.\n");
    return false;
  }

  // TODO(tomfinegan): As noted in above, the PesHeaderFields should be an
  // array (or some data structure) that can be iterated over.

  // First byte of header, fields: marker, scrambling, priority, alignment,
  // copyright, original.
  *byte = 0;
  *byte |= marker.bits << marker.shift;
  *byte |= scrambling.bits << scrambling.shift;
  *byte |= priority.bits << priority.shift;
  *byte |= data_alignment.bits << data_alignment.shift;
  *byte |= copyright.bits << copyright.shift;
  *byte |= original.bits << original.shift;

  // Second byte of header, fields: has_pts, has_dts, unused fields.
  *++byte = 0;
  if (write_pts == true) {
    *byte |= has_pts.bits << has_pts.shift;
    *byte |= has_dts.bits << has_dts.shift;
  }

  // Third byte of header, fields: remaining size of header.
  *++byte = remaining_size.bits & 0xff;  // Field is 8 bits wide.

  int num_stuffing_bytes =
      (pts.num_bits + 7) / 8 + 1 /* always 1 stuffing byte */;
  if (write_pts == true) {
    // Set the PTS value and adjust stuffing byte count accordingly.
    *++byte = (pts.bits >> 32) & 0xff;
    *++byte = (pts.bits >> 24) & 0xff;
    *++byte = (pts.bits >> 16) & 0xff;
    *++byte = (pts.bits >> 8) & 0xff;
    *++byte = pts.bits & 0xff;
    num_stuffing_bytes = 1;
  }

  // Add the stuffing byte(s).
  for (int i = 0; i < num_stuffing_bytes; ++i)
    *++byte = stuffing_byte.bits & 0xff;

  for (int i = 0; i < kHeaderSize; ++i)
    buffer->push_back(header[i]);

  return true;
}

//
// BCMVHeader methods.
//

bool BCMVHeader::Write(PacketDataBuffer* buffer) const {
  if (buffer == nullptr) {
    std::fprintf(stderr, "Webm2Pes: nullptr for buffer in BCMV Write.\n");
    return false;
  }
  const int kBcmvSize = 4;
  for (int i = 0; i < kBcmvSize; ++i)
    buffer->push_back(bcmv[i]);

  const int kRemainingBytes = 6;
  const uint8_t bcmv_buffer[kRemainingBytes] = {
      static_cast<std::uint8_t>((length >> 24) & 0xff),
      static_cast<std::uint8_t>((length >> 16) & 0xff),
      static_cast<std::uint8_t>((length >> 8) & 0xff),
      static_cast<std::uint8_t>(length & 0xff),
      0,
      0 /* 2 bytes 0 padding */};
  for (int i = 0; i < kRemainingBytes; ++i)
    buffer->push_back(bcmv_buffer[i]);

  return true;
}

//
// PesHeader methods.
//

// Writes out the header to |buffer|. Calls PesOptionalHeader::Write() to write
// |optional_header| contents. Returns true when successful, false otherwise.
bool PesHeader::Write(bool write_pts, PacketDataBuffer* buffer) const {
  if (buffer == nullptr) {
    std::fprintf(stderr, "Webm2Pes: nullptr in header writer.\n");
    return false;
  }

  // Write |start_code|.
  const int kStartCodeLength = 4;
  for (int i = 0; i < kStartCodeLength; ++i)
    buffer->push_back(start_code[i]);

  // Write |packet_length| as big endian.
  std::uint8_t byte = (packet_length >> 8) & 0xff;
  buffer->push_back(byte);
  byte = packet_length & 0xff;
  buffer->push_back(byte);

  // Write the (not really) optional header.
  if (optional_header.Write(write_pts, buffer) != true) {
    std::fprintf(stderr, "Webm2Pes: PES optional header write failed.");
    return false;
  }
  return true;
}

//
// Webm2Pes methods.
//

bool Webm2Pes::ConvertToFile() {
  if (input_file_name_.empty() || output_file_name_.empty()) {
    std::fprintf(stderr, "Webm2Pes: input and/or output file name(s) empty.\n");
    return false;
  }

  output_file_ = FilePtr(fopen(output_file_name_.c_str(), "wb"), FILEDeleter());
  if (output_file_ == nullptr) {
    std::fprintf(stderr, "Webm2Pes: Cannot open %s for output.\n",
                 output_file_name_.c_str());
    return false;
  }

  if (InitWebmParser() != true) {
    std::fprintf(stderr, "Webm2Pes: Cannot initialize WebM parser.\n");
    return false;
  }

  // Walk clusters in segment.
  const mkvparser::Cluster* cluster = webm_parser_->GetFirst();
  while (cluster != nullptr && cluster->EOS() == false) {
    const mkvparser::BlockEntry* block_entry = nullptr;
    std::int64_t block_status = cluster->GetFirst(block_entry);
    if (block_status < 0) {
      std::fprintf(stderr, "Webm2Pes: Cannot parse first block in %s.\n",
                   input_file_name_.c_str());
      return false;
    }

    // Walk blocks in cluster.
    while (block_entry != nullptr && block_entry->EOS() == false) {
      const mkvparser::Block* block = block_entry->GetBlock();
      if (block->GetTrackNumber() == video_track_num_) {
        const int frame_count = block->GetFrameCount();

        // Walk frames in block.
        for (int frame_num = 0; frame_num < frame_count; ++frame_num) {
          const mkvparser::Block::Frame& frame = block->GetFrame(frame_num);

          // Write frame out as PES packet(s), storing them in |packet_data_|.
          const bool pes_status = WritePesPacket(
              frame, static_cast<double>(block->GetTime(cluster)));
          if (pes_status != true) {
            std::fprintf(stderr, "Webm2Pes: WritePesPacket failed.\n");
            return false;
          }

          // Write contents of |packet_data_| to |output_file_|.
          if (std::fwrite(&packet_data_[0], 1, packet_data_.size(),
                          output_file_.get()) != packet_data_.size()) {
            std::fprintf(stderr, "Webm2Pes: packet payload write failed.\n");
            return false;
          }
        }
      }
      block_status = cluster->GetNext(block_entry, block_entry);
      if (block_status < 0) {
        std::fprintf(stderr, "Webm2Pes: Cannot parse block in %s.\n",
                     input_file_name_.c_str());
        return false;
      }
    }

    cluster = webm_parser_->GetNext(cluster);
  }

  return true;
}

bool Webm2Pes::ConvertToPacketReceiver() {
  if (input_file_name_.empty() || packet_sink_ == nullptr) {
    std::fprintf(stderr, "Webm2Pes: input file name empty or null sink.\n");
    return false;
  }

  if (InitWebmParser() != true) {
    std::fprintf(stderr, "Webm2Pes: Cannot initialize WebM parser.\n");
    return false;
  }

  // Walk clusters in segment.
  const mkvparser::Cluster* cluster = webm_parser_->GetFirst();
  while (cluster != nullptr && cluster->EOS() == false) {
    const mkvparser::BlockEntry* block_entry = nullptr;
    std::int64_t block_status = cluster->GetFirst(block_entry);
    if (block_status < 0) {
      std::fprintf(stderr, "Webm2Pes: Cannot parse first block in %s.\n",
                   input_file_name_.c_str());
      return false;
    }

    // Walk blocks in cluster.
    while (block_entry != nullptr && block_entry->EOS() == false) {
      const mkvparser::Block* block = block_entry->GetBlock();
      if (block->GetTrackNumber() == video_track_num_) {
        const int frame_count = block->GetFrameCount();

        // Walk frames in block.
        for (int frame_num = 0; frame_num < frame_count; ++frame_num) {
          const mkvparser::Block::Frame& frame = block->GetFrame(frame_num);

          // Write frame out as PES packet(s).
          const bool pes_status = WritePesPacket(
              frame, static_cast<double>(block->GetTime(cluster)));
          if (pes_status != true) {
            std::fprintf(stderr, "Webm2Pes: WritePesPacket failed.\n");
            return false;
          }
          if (packet_sink_->ReceivePacket(packet_data_) != true) {
            std::fprintf(stderr, "Webm2Pes: ReceivePacket failed.\n");
            return false;
          }
        }
      }
      block_status = cluster->GetNext(block_entry, block_entry);
      if (block_status < 0) {
        std::fprintf(stderr, "Webm2Pes: Cannot parse block in %s.\n",
                     input_file_name_.c_str());
        return false;
      }
    }

    cluster = webm_parser_->GetNext(cluster);
  }

  std::fflush(output_file_.get());
  return true;
}

bool Webm2Pes::InitWebmParser() {
  if (webm_reader_.Open(input_file_name_.c_str()) != 0) {
    std::fprintf(stderr, "Webm2Pes: Cannot open %s as input.\n",
                 input_file_name_.c_str());
    return false;
  }

  using mkvparser::Segment;
  Segment* webm_parser = nullptr;
  if (Segment::CreateInstance(&webm_reader_, 0 /* pos */,
                              webm_parser /* Segment*& */) != 0) {
    std::fprintf(stderr, "Webm2Pes: Cannot create WebM parser.\n");
    return false;
  }
  webm_parser_.reset(webm_parser);

  if (webm_parser_->Load() != 0) {
    std::fprintf(stderr, "Webm2Pes: Cannot parse %s.\n",
                 input_file_name_.c_str());
    return false;
  }

  // Store timecode scale.
  timecode_scale_ = webm_parser_->GetInfo()->GetTimeCodeScale();

  // Make sure there's a video track.
  const mkvparser::Tracks* tracks = webm_parser_->GetTracks();
  if (tracks == nullptr) {
    std::fprintf(stderr, "Webm2Pes: %s has no tracks.\n",
                 input_file_name_.c_str());
    return false;
  }
  for (int track_index = 0;
       track_index < static_cast<int>(tracks->GetTracksCount());
       ++track_index) {
    const mkvparser::Track* track = tracks->GetTrackByIndex(track_index);
    if (track && track->GetType() == mkvparser::Track::kVideo) {
      if (std::string(track->GetCodecId()) == std::string("V_VP8"))
        codec_ = VP8;
      else if (std::string(track->GetCodecId()) == std::string("V_VP9"))
        codec_ = VP9;
      else {
        fprintf(stderr, "Webm2Pes: Codec must be VP8 or VP9.\n");
        return false;
      }
      video_track_num_ = track_index + 1;
      break;
    }
  }
  if (video_track_num_ < 1) {
    std::fprintf(stderr, "Webm2Pes: No video track found in %s.\n",
                 input_file_name_.c_str());
    return false;
  }
  return true;
}

bool Webm2Pes::WritePesPacket(const mkvparser::Block::Frame& vpx_frame,
                              double nanosecond_pts) {
  // Read the input frame.
  std::unique_ptr<uint8_t[]> frame_data(new (std::nothrow)
                                            uint8_t[vpx_frame.len]);
  if (frame_data.get() == nullptr) {
    std::fprintf(stderr, "Webm2Pes: Out of memory.\n");
    return false;
  }
  if (vpx_frame.Read(&webm_reader_, frame_data.get()) != 0) {
    std::fprintf(stderr, "Webm2Pes: Error reading VPx frame!\n");
    return false;
  }

  Ranges frame_ranges;
  if (codec_ == VP9) {
    bool has_superframe_index =
        ParseVP9SuperFrameIndex(frame_data.get(), vpx_frame.len, &frame_ranges);
    if (has_superframe_index == false) {
      frame_ranges.push_back(Range(0, vpx_frame.len));
    }
  } else {
    frame_ranges.push_back(Range(0, vpx_frame.len));
  }

  PesHeader header;
  Ranges packet_payload_ranges;
  if (GetPacketPayloadRanges(header, frame_ranges, &packet_payload_ranges) !=
      true) {
    std::fprintf(stderr, "Webm2Pes: Error preparing packet payload ranges!\n");
    return false;
  }

  ///
  /// TODO: DEBUG/REMOVE
  ///
  printf("-FRAME TOTAL LENGTH %ld--\n", vpx_frame.len);
  for (const Range& frame_range : frame_ranges) {
    printf("--frame range: off:%lu len:%lu\n", frame_range.offset,
           frame_range.length);
  }
  for (const Range& payload_range : packet_payload_ranges) {
    printf("---payload range: off:%lu len:%lu\n", payload_range.offset,
           payload_range.length);
  }

  const std::int64_t khz90_pts =
      NanosecondsTo90KhzTicks(static_cast<std::int64_t>(nanosecond_pts));
  header.optional_header.SetPtsBits(khz90_pts);

  packet_data_.clear();

  bool write_pts = true;
  for (const Range& packet_payload_range : packet_payload_ranges) {
    header.packet_length =
        (header.optional_header.size_in_bytes() + packet_payload_range.length) &
        0xffff;
    if (header.Write(write_pts, &packet_data_) != true) {
      std::fprintf(stderr, "Webm2Pes: packet header write failed.\n");
      return false;
    }
    write_pts = false;

    BCMVHeader bcmv_header(static_cast<uint32_t>(packet_payload_range.length));
    if (bcmv_header.Write(&packet_data_) != true) {
      std::fprintf(stderr, "Webm2Pes: BCMV write failed.\n");
      return false;
    }

    // Insert the payload at the end of |packet_data_|.
    const std::uint8_t* payload_start =
        frame_data.get() + packet_payload_range.offset;
    packet_data_.insert(packet_data_.end(), payload_start,
                        payload_start + packet_payload_range.length);
  }

  return true;
}
}  // namespace libwebm
