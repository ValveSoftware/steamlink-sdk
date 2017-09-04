// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains an implementation of a VP9 bitstream parser.
//
// VERBOSE level:
//  1 something wrong in bitstream
//  2 parsing steps
//  3 parsed values (selected)

#include "media/filters/vp9_parser.h"

#include <algorithm>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/numerics/safe_conversions.h"
#include "media/filters/vp9_compressed_header_parser.h"
#include "media/filters/vp9_uncompressed_header_parser.h"

namespace media {

bool Vp9FrameHeader::IsKeyframe() const {
  // When show_existing_frame is true, the frame header does not precede an
  // actual frame to be decoded, so frame_type does not apply (and is not read
  // from the stream).
  return !show_existing_frame && frame_type == KEYFRAME;
}

bool Vp9FrameHeader::IsIntra() const {
  return !show_existing_frame && (frame_type == KEYFRAME || intra_only);
}

Vp9Parser::FrameInfo::FrameInfo(const uint8_t* ptr, off_t size)
    : ptr(ptr), size(size) {}

bool Vp9FrameContext::IsValid() const {
  // probs should be in [1, 255] range.
  static_assert(sizeof(Vp9Prob) == 1,
                "following checks assuming Vp9Prob is single byte");
  if (memchr(tx_probs_8x8, 0, sizeof(tx_probs_8x8)))
    return false;
  if (memchr(tx_probs_16x16, 0, sizeof(tx_probs_16x16)))
    return false;
  if (memchr(tx_probs_32x32, 0, sizeof(tx_probs_32x32)))
    return false;

  for (auto& a : coef_probs) {
    for (auto& ai : a) {
      for (auto& aj : ai) {
        for (auto& ak : aj) {
          int max_l = (ak == aj[0]) ? 3 : 6;
          for (int l = 0; l < max_l; l++) {
            for (auto& x : ak[l]) {
              if (x == 0)
                return false;
            }
          }
        }
      }
    }
  }
  if (memchr(skip_prob, 0, sizeof(skip_prob)))
    return false;
  if (memchr(inter_mode_probs, 0, sizeof(inter_mode_probs)))
    return false;
  if (memchr(interp_filter_probs, 0, sizeof(interp_filter_probs)))
    return false;
  if (memchr(is_inter_prob, 0, sizeof(is_inter_prob)))
    return false;
  if (memchr(comp_mode_prob, 0, sizeof(comp_mode_prob)))
    return false;
  if (memchr(single_ref_prob, 0, sizeof(single_ref_prob)))
    return false;
  if (memchr(comp_ref_prob, 0, sizeof(comp_ref_prob)))
    return false;
  if (memchr(y_mode_probs, 0, sizeof(y_mode_probs)))
    return false;
  if (memchr(uv_mode_probs, 0, sizeof(uv_mode_probs)))
    return false;
  if (memchr(partition_probs, 0, sizeof(partition_probs)))
    return false;
  if (memchr(mv_joint_probs, 0, sizeof(mv_joint_probs)))
    return false;
  if (memchr(mv_sign_prob, 0, sizeof(mv_sign_prob)))
    return false;
  if (memchr(mv_class_probs, 0, sizeof(mv_class_probs)))
    return false;
  if (memchr(mv_class0_bit_prob, 0, sizeof(mv_class0_bit_prob)))
    return false;
  if (memchr(mv_bits_prob, 0, sizeof(mv_bits_prob)))
    return false;
  if (memchr(mv_class0_fr_probs, 0, sizeof(mv_class0_fr_probs)))
    return false;
  if (memchr(mv_fr_probs, 0, sizeof(mv_fr_probs)))
    return false;
  if (memchr(mv_class0_hp_prob, 0, sizeof(mv_class0_hp_prob)))
    return false;
  if (memchr(mv_hp_prob, 0, sizeof(mv_hp_prob)))
    return false;

  return true;
}

Vp9Parser::Context::Vp9FrameContextManager::Vp9FrameContextManager()
    : weak_ptr_factory_(this) {}

Vp9Parser::Context::Vp9FrameContextManager::~Vp9FrameContextManager() {}

const Vp9FrameContext&
Vp9Parser::Context::Vp9FrameContextManager::frame_context() const {
  DCHECK(initialized_);
  DCHECK(!needs_client_update_);
  return frame_context_;
}

void Vp9Parser::Context::Vp9FrameContextManager::Reset() {
  initialized_ = false;
  needs_client_update_ = false;
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void Vp9Parser::Context::Vp9FrameContextManager::SetNeedsClientUpdate() {
  DCHECK(!needs_client_update_);
  initialized_ = true;
  needs_client_update_ = true;
}

Vp9Parser::ContextRefreshCallback
Vp9Parser::Context::Vp9FrameContextManager::GetUpdateCb() {
  if (needs_client_update_)
    return base::Bind(&Vp9FrameContextManager::UpdateFromClient,
                      weak_ptr_factory_.GetWeakPtr());
  else
    return Vp9Parser::ContextRefreshCallback();
}

void Vp9Parser::Context::Vp9FrameContextManager::Update(
    const Vp9FrameContext& frame_context) {
  // DCHECK because we can trust values from our parser.
  DCHECK(frame_context.IsValid());
  initialized_ = true;
  frame_context_ = frame_context;

  // For frame context we are updating, it may be still awaiting previous
  // ContextRefreshCallback. Because we overwrite the value of context here and
  // previous ContextRefreshCallback no longer matters, invalidate the weak ptr
  // to prevent previous ContextRefreshCallback run.
  // With this optimization, we may be able to parse more frames while previous
  // are still decoding.
  weak_ptr_factory_.InvalidateWeakPtrs();
  needs_client_update_ = false;
}

void Vp9Parser::Context::Vp9FrameContextManager::UpdateFromClient(
    const Vp9FrameContext& frame_context) {
  DVLOG(2) << "Got external frame_context update";
  DCHECK(needs_client_update_);
  if (!frame_context.IsValid()) {
    DLOG(ERROR) << "Invalid prob value in frame_context";
    return;
  }
  needs_client_update_ = false;
  initialized_ = true;
  frame_context_ = frame_context;
}

void Vp9Parser::Context::Reset() {
  memset(&segmentation_, 0, sizeof(segmentation_));
  memset(&loop_filter_, 0, sizeof(loop_filter_));
  memset(&ref_slots_, 0, sizeof(ref_slots_));
  for (auto& manager : frame_context_managers_)
    manager.Reset();
}

void Vp9Parser::Context::MarkFrameContextForUpdate(size_t frame_context_idx) {
  DCHECK_LT(frame_context_idx, arraysize(frame_context_managers_));
  frame_context_managers_[frame_context_idx].SetNeedsClientUpdate();
}

void Vp9Parser::Context::UpdateFrameContext(
    size_t frame_context_idx,
    const Vp9FrameContext& frame_context) {
  DCHECK_LT(frame_context_idx, arraysize(frame_context_managers_));
  frame_context_managers_[frame_context_idx].Update(frame_context);
}

const Vp9Parser::ReferenceSlot& Vp9Parser::Context::GetRefSlot(
    size_t ref_type) const {
  DCHECK_LT(ref_type, arraysize(ref_slots_));
  return ref_slots_[ref_type];
}

void Vp9Parser::Context::UpdateRefSlot(
    size_t ref_type,
    const Vp9Parser::ReferenceSlot& ref_slot) {
  DCHECK_LT(ref_type, arraysize(ref_slots_));
  ref_slots_[ref_type] = ref_slot;
}

Vp9Parser::Vp9Parser(bool parsing_compressed_header)
    : parsing_compressed_header_(parsing_compressed_header) {
  Reset();
}

Vp9Parser::~Vp9Parser() {}

void Vp9Parser::SetStream(const uint8_t* stream, off_t stream_size) {
  DCHECK(stream);
  stream_ = stream;
  bytes_left_ = stream_size;
  frames_.clear();
}

void Vp9Parser::Reset() {
  stream_ = nullptr;
  bytes_left_ = 0;
  frames_.clear();
  curr_frame_info_.Reset();

  context_.Reset();
}

Vp9Parser::Result Vp9Parser::ParseNextFrame(Vp9FrameHeader* fhdr) {
  DCHECK(fhdr);
  DVLOG(2) << "ParseNextFrame";

  // If |curr_frame_info_| is valid, uncompressed header was parsed into
  // |curr_frame_header_| and we are awaiting context update to proceed with
  // compressed header parsing.
  if (!curr_frame_info_.IsValid()) {
    if (frames_.empty()) {
      // No frames to be decoded, if there is no more stream, request more.
      if (!stream_)
        return kEOStream;

      // New stream to be parsed, parse it and fill frames_.
      frames_ = ParseSuperframe();
      if (frames_.empty()) {
        DVLOG(1) << "Failed parsing superframes";
        return kInvalidStream;
      }
    }

    curr_frame_info_ = frames_.front();
    frames_.pop_front();

    memset(&curr_frame_header_, 0, sizeof(curr_frame_header_));

    Vp9UncompressedHeaderParser uncompressed_parser(&context_);
    if (!uncompressed_parser.Parse(curr_frame_info_.ptr, curr_frame_info_.size,
                                   &curr_frame_header_))
      return kInvalidStream;

    if (curr_frame_header_.header_size_in_bytes == 0) {
      // Verify padding bits are zero.
      for (off_t i = curr_frame_header_.uncompressed_header_size;
           i < curr_frame_info_.size; i++) {
        if (curr_frame_info_.ptr[i] != 0) {
          DVLOG(1) << "Padding bits are not zeros.";
          return kInvalidStream;
        }
      }
      *fhdr = curr_frame_header_;
      curr_frame_info_.Reset();
      return kOk;
    }
    if (curr_frame_header_.uncompressed_header_size +
            curr_frame_header_.header_size_in_bytes >
        base::checked_cast<size_t>(curr_frame_info_.size)) {
      DVLOG(1) << "header_size_in_bytes="
               << curr_frame_header_.header_size_in_bytes
               << " is larger than bytes left in buffer: "
               << curr_frame_info_.size -
                      curr_frame_header_.uncompressed_header_size;
      return kInvalidStream;
    }
  }

  if (parsing_compressed_header_) {
    size_t frame_context_idx = curr_frame_header_.frame_context_idx;
    const Context::Vp9FrameContextManager& context_to_load =
        context_.frame_context_managers_[frame_context_idx];
    if (!context_to_load.initialized()) {
      // 8.2 Frame order constraints
      // must load an initialized set of probabilities.
      DVLOG(1) << "loading uninitialized frame context, index="
               << frame_context_idx;
      return kInvalidStream;
    }
    if (context_to_load.needs_client_update()) {
      DVLOG(3) << "waiting frame_context_idx=" << frame_context_idx
               << " to update";
      return kAwaitingRefresh;
    }
    curr_frame_header_.initial_frame_context =
        curr_frame_header_.frame_context = context_to_load.frame_context();

    Vp9CompressedHeaderParser compressed_parser;
    if (!compressed_parser.Parse(
            curr_frame_info_.ptr + curr_frame_header_.uncompressed_header_size,
            curr_frame_header_.header_size_in_bytes, &curr_frame_header_)) {
      return kInvalidStream;
    }

    if (curr_frame_header_.refresh_frame_context) {
      // In frame parallel mode, we can refresh the context without decoding
      // tile data.
      if (curr_frame_header_.frame_parallel_decoding_mode) {
        context_.UpdateFrameContext(frame_context_idx,
                                    curr_frame_header_.frame_context);
      } else {
        context_.MarkFrameContextForUpdate(frame_context_idx);
      }
    }
  }

  SetupSegmentationDequant();
  SetupLoopFilter();
  UpdateSlots();

  *fhdr = curr_frame_header_;
  curr_frame_info_.Reset();
  return kOk;
}

Vp9Parser::ContextRefreshCallback Vp9Parser::GetContextRefreshCb(
    size_t frame_context_idx) {
  DCHECK_LT(frame_context_idx, arraysize(context_.frame_context_managers_));
  auto& frame_context_manager =
      context_.frame_context_managers_[frame_context_idx];

  return frame_context_manager.GetUpdateCb();
}

// Annex B Superframes
std::deque<Vp9Parser::FrameInfo> Vp9Parser::ParseSuperframe() {
  const uint8_t* stream = stream_;
  off_t bytes_left = bytes_left_;

  // Make sure we don't parse stream_ more than once.
  stream_ = nullptr;
  bytes_left_ = 0;

  if (bytes_left < 1)
    return std::deque<FrameInfo>();

  // If this is a superframe, the last byte in the stream will contain the
  // superframe marker. If not, the whole buffer contains a single frame.
  uint8_t marker = *(stream + bytes_left - 1);
  if ((marker & 0xe0) != 0xc0) {
    return {FrameInfo(stream, bytes_left)};
  }

  DVLOG(1) << "Parsing a superframe";

  // The bytes immediately before the superframe marker constitute superframe
  // index, which stores information about sizes of each frame in it.
  // Calculate its size and set index_ptr to the beginning of it.
  size_t num_frames = (marker & 0x7) + 1;
  size_t mag = ((marker >> 3) & 0x3) + 1;
  off_t index_size = 2 + mag * num_frames;

  if (bytes_left < index_size)
    return std::deque<FrameInfo>();

  const uint8_t* index_ptr = stream + bytes_left - index_size;
  if (marker != *index_ptr)
    return std::deque<FrameInfo>();

  ++index_ptr;
  bytes_left -= index_size;

  // Parse frame information contained in the index and add a pointer to and
  // size of each frame to frames.
  std::deque<FrameInfo> frames;
  for (size_t i = 0; i < num_frames; ++i) {
    uint32_t size = 0;
    for (size_t j = 0; j < mag; ++j) {
      size |= *index_ptr << (j * 8);
      ++index_ptr;
    }

    if (base::checked_cast<off_t>(size) > bytes_left) {
      DVLOG(1) << "Not enough data in the buffer for frame " << i;
      return std::deque<FrameInfo>();
    }

    frames.push_back(FrameInfo(stream, size));
    stream += size;
    bytes_left -= size;

    DVLOG(1) << "Frame " << i << ", size: " << size;
  }

  return frames;
}

// 8.6.1
const size_t QINDEX_RANGE = 256;
const int16_t kDcQLookup[QINDEX_RANGE] = {
  4,       8,    8,    9,   10,   11,   12,   12,
  13,     14,   15,   16,   17,   18,   19,   19,
  20,     21,   22,   23,   24,   25,   26,   26,
  27,     28,   29,   30,   31,   32,   32,   33,
  34,     35,   36,   37,   38,   38,   39,   40,
  41,     42,   43,   43,   44,   45,   46,   47,
  48,     48,   49,   50,   51,   52,   53,   53,
  54,     55,   56,   57,   57,   58,   59,   60,
  61,     62,   62,   63,   64,   65,   66,   66,
  67,     68,   69,   70,   70,   71,   72,   73,
  74,     74,   75,   76,   77,   78,   78,   79,
  80,     81,   81,   82,   83,   84,   85,   85,
  87,     88,   90,   92,   93,   95,   96,   98,
  99,    101,  102,  104,  105,  107,  108,  110,
  111,   113,  114,  116,  117,  118,  120,  121,
  123,   125,  127,  129,  131,  134,  136,  138,
  140,   142,  144,  146,  148,  150,  152,  154,
  156,   158,  161,  164,  166,  169,  172,  174,
  177,   180,  182,  185,  187,  190,  192,  195,
  199,   202,  205,  208,  211,  214,  217,  220,
  223,   226,  230,  233,  237,  240,  243,  247,
  250,   253,  257,  261,  265,  269,  272,  276,
  280,   284,  288,  292,  296,  300,  304,  309,
  313,   317,  322,  326,  330,  335,  340,  344,
  349,   354,  359,  364,  369,  374,  379,  384,
  389,   395,  400,  406,  411,  417,  423,  429,
  435,   441,  447,  454,  461,  467,  475,  482,
  489,   497,  505,  513,  522,  530,  539,  549,
  559,   569,  579,  590,  602,  614,  626,  640,
  654,   668,  684,  700,  717,  736,  755,  775,
  796,   819,  843,  869,  896,  925,  955,  988,
  1022, 1058, 1098, 1139, 1184, 1232, 1282, 1336,
};

const int16_t kAcQLookup[QINDEX_RANGE] = {
  4,       8,    9,   10,   11,   12,   13,   14,
  15,     16,   17,   18,   19,   20,   21,   22,
  23,     24,   25,   26,   27,   28,   29,   30,
  31,     32,   33,   34,   35,   36,   37,   38,
  39,     40,   41,   42,   43,   44,   45,   46,
  47,     48,   49,   50,   51,   52,   53,   54,
  55,     56,   57,   58,   59,   60,   61,   62,
  63,     64,   65,   66,   67,   68,   69,   70,
  71,     72,   73,   74,   75,   76,   77,   78,
  79,     80,   81,   82,   83,   84,   85,   86,
  87,     88,   89,   90,   91,   92,   93,   94,
  95,     96,   97,   98,   99,  100,  101,  102,
  104,   106,  108,  110,  112,  114,  116,  118,
  120,   122,  124,  126,  128,  130,  132,  134,
  136,   138,  140,  142,  144,  146,  148,  150,
  152,   155,  158,  161,  164,  167,  170,  173,
  176,   179,  182,  185,  188,  191,  194,  197,
  200,   203,  207,  211,  215,  219,  223,  227,
  231,   235,  239,  243,  247,  251,  255,  260,
  265,   270,  275,  280,  285,  290,  295,  300,
  305,   311,  317,  323,  329,  335,  341,  347,
  353,   359,  366,  373,  380,  387,  394,  401,
  408,   416,  424,  432,  440,  448,  456,  465,
  474,   483,  492,  501,  510,  520,  530,  540,
  550,   560,  571,  582,  593,  604,  615,  627,
  639,   651,  663,  676,  689,  702,  715,  729,
  743,   757,  771,  786,  801,  816,  832,  848,
  864,   881,  898,  915,  933,  951,  969,  988,
  1007, 1026, 1046, 1066, 1087, 1108, 1129, 1151,
  1173, 1196, 1219, 1243, 1267, 1292, 1317, 1343,
  1369, 1396, 1423, 1451, 1479, 1508, 1537, 1567,
  1597, 1628, 1660, 1692, 1725, 1759, 1793, 1828,
};

static_assert(arraysize(kDcQLookup) == arraysize(kAcQLookup),
              "quantizer lookup arrays of incorrect size");

static size_t ClampQ(size_t q) {
  return std::min(std::max(static_cast<size_t>(0), q),
                  arraysize(kDcQLookup) - 1);
}

// 8.6.1 Dequantization functions
size_t Vp9Parser::GetQIndex(const Vp9QuantizationParams& quant,
                            size_t segid) const {
  const Vp9SegmentationParams& segmentation = context_.segmentation();

  if (segmentation.FeatureEnabled(segid,
                                  Vp9SegmentationParams::SEG_LVL_ALT_Q)) {
    int16_t feature_data =
        segmentation.FeatureData(segid, Vp9SegmentationParams::SEG_LVL_ALT_Q);
    size_t q_index = segmentation.abs_or_delta_update
                         ? feature_data
                         : quant.base_q_idx + feature_data;
    return ClampQ(q_index);
  }

  return quant.base_q_idx;
}

// 8.6.1 Dequantization functions
void Vp9Parser::SetupSegmentationDequant() {
  const Vp9QuantizationParams& quant = curr_frame_header_.quant_params;
  Vp9SegmentationParams& segmentation = context_.segmentation_;

  DLOG_IF(ERROR, curr_frame_header_.bit_depth > 8)
      << "bit_depth > 8 is not supported "
         "yet, kDcQLookup and kAcQLookup "
         "need extended";
  if (segmentation.enabled) {
    for (size_t i = 0; i < Vp9SegmentationParams::kNumSegments; ++i) {
      const size_t q_index = GetQIndex(quant, i);
      segmentation.y_dequant[i][0] =
          kDcQLookup[ClampQ(q_index + quant.delta_q_y_dc)];
      segmentation.y_dequant[i][1] = kAcQLookup[ClampQ(q_index)];
      segmentation.uv_dequant[i][0] =
          kDcQLookup[ClampQ(q_index + quant.delta_q_uv_dc)];
      segmentation.uv_dequant[i][1] =
          kAcQLookup[ClampQ(q_index + quant.delta_q_uv_ac)];
    }
  } else {
    const size_t q_index = quant.base_q_idx;
    segmentation.y_dequant[0][0] =
        kDcQLookup[ClampQ(q_index + quant.delta_q_y_dc)];
    segmentation.y_dequant[0][1] = kAcQLookup[ClampQ(q_index)];
    segmentation.uv_dequant[0][0] =
        kDcQLookup[ClampQ(q_index + quant.delta_q_uv_dc)];
    segmentation.uv_dequant[0][1] =
        kAcQLookup[ClampQ(q_index + quant.delta_q_uv_ac)];
  }
}

static int ClampLf(int lf) {
  const int kMaxLoopFilterLevel = 63;
  return std::min(std::max(0, lf), kMaxLoopFilterLevel);
}

// 8.8.1 Loop filter frame init process
void Vp9Parser::SetupLoopFilter() {
  Vp9LoopFilterParams& loop_filter = context_.loop_filter_;
  if (!loop_filter.level)
    return;

  int scale = loop_filter.level < 32 ? 1 : 2;

  for (size_t i = 0; i < Vp9SegmentationParams::kNumSegments; ++i) {
    int level = loop_filter.level;
    const Vp9SegmentationParams& segmentation = context_.segmentation();

    if (segmentation.FeatureEnabled(i, Vp9SegmentationParams::SEG_LVL_ALT_LF)) {
      int feature_data =
          segmentation.FeatureData(i, Vp9SegmentationParams::SEG_LVL_ALT_LF);
      level = ClampLf(segmentation.abs_or_delta_update ? feature_data
                                                       : level + feature_data);
    }

    if (!loop_filter.delta_enabled) {
      memset(loop_filter.lvl[i], level, sizeof(loop_filter.lvl[i]));
    } else {
      loop_filter.lvl[i][Vp9RefType::VP9_FRAME_INTRA][0] = ClampLf(
          level + loop_filter.ref_deltas[Vp9RefType::VP9_FRAME_INTRA] * scale);
      loop_filter.lvl[i][Vp9RefType::VP9_FRAME_INTRA][1] = 0;

      for (size_t type = Vp9RefType::VP9_FRAME_LAST;
           type < Vp9RefType::VP9_FRAME_MAX; ++type) {
        for (size_t mode = 0; mode < Vp9LoopFilterParams::kNumModeDeltas;
             ++mode) {
          loop_filter.lvl[i][type][mode] =
              ClampLf(level + loop_filter.ref_deltas[type] * scale +
                      loop_filter.mode_deltas[mode] * scale);
        }
      }
    }
  }
}

void Vp9Parser::UpdateSlots() {
  // 8.10 Reference frame update process
  for (size_t i = 0; i < kVp9NumRefFrames; i++) {
    if (curr_frame_header_.RefreshFlag(i)) {
      ReferenceSlot ref_slot;
      ref_slot.initialized = true;

      ref_slot.frame_width = curr_frame_header_.frame_width;
      ref_slot.frame_height = curr_frame_header_.frame_height;
      ref_slot.subsampling_x = curr_frame_header_.subsampling_x;
      ref_slot.subsampling_y = curr_frame_header_.subsampling_y;
      ref_slot.bit_depth = curr_frame_header_.bit_depth;

      ref_slot.profile = curr_frame_header_.profile;
      ref_slot.color_space = curr_frame_header_.color_space;
      context_.UpdateRefSlot(i, ref_slot);
    }
  }
}

}  // namespace media
