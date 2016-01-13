// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <limits>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "content/common/gpu/media/vaapi_h264_decoder.h"

namespace content {

// Decode surface, used for decoding and reference. input_id comes from client
// and is associated with the surface that was produced as the result
// of decoding a bitstream buffer with that id.
class VaapiH264Decoder::DecodeSurface {
 public:
  DecodeSurface(int poc,
                int32 input_id,
                const scoped_refptr<VASurface>& va_surface);
  DecodeSurface(int poc, const scoped_refptr<DecodeSurface>& dec_surface);
  ~DecodeSurface();

  int poc() {
    return poc_;
  }

  scoped_refptr<VASurface> va_surface() {
    return va_surface_;
  }

  int32 input_id() {
    return input_id_;
  }

 private:
  int poc_;
  int32 input_id_;
  scoped_refptr<VASurface> va_surface_;
};

VaapiH264Decoder::DecodeSurface::DecodeSurface(
    int poc,
    int32 input_id,
    const scoped_refptr<VASurface>& va_surface)
    : poc_(poc),
      input_id_(input_id),
      va_surface_(va_surface) {
  DCHECK(va_surface_.get());
}

VaapiH264Decoder::DecodeSurface::~DecodeSurface() {
}

VaapiH264Decoder::VaapiH264Decoder(
    VaapiWrapper* vaapi_wrapper,
    const OutputPicCB& output_pic_cb,
    const ReportErrorToUmaCB& report_error_to_uma_cb)
    : max_pic_order_cnt_lsb_(0),
      max_frame_num_(0),
      max_pic_num_(0),
      max_long_term_frame_idx_(0),
      max_num_reorder_frames_(0),
      curr_sps_id_(-1),
      curr_pps_id_(-1),
      vaapi_wrapper_(vaapi_wrapper),
      output_pic_cb_(output_pic_cb),
      report_error_to_uma_cb_(report_error_to_uma_cb) {
  Reset();
  state_ = kNeedStreamMetadata;
}

VaapiH264Decoder::~VaapiH264Decoder() {
}

void VaapiH264Decoder::Reset() {
  curr_pic_.reset();

  curr_input_id_ = -1;
  frame_num_ = 0;
  prev_frame_num_ = -1;
  prev_frame_num_offset_ = -1;

  prev_ref_has_memmgmnt5_ = false;
  prev_ref_top_field_order_cnt_ = -1;
  prev_ref_pic_order_cnt_msb_ = -1;
  prev_ref_pic_order_cnt_lsb_ = -1;
  prev_ref_field_ = H264Picture::FIELD_NONE;

  vaapi_wrapper_->DestroyPendingBuffers();

  ref_pic_list0_.clear();
  ref_pic_list1_.clear();

  for (DecSurfacesInUse::iterator it = decode_surfaces_in_use_.begin();
       it != decode_surfaces_in_use_.end(); ) {
    int poc = it->second->poc();
    // Must be incremented before UnassignSurfaceFromPoC as this call
    // invalidates |it|.
    ++it;
    UnassignSurfaceFromPoC(poc);
  }
  DCHECK(decode_surfaces_in_use_.empty());

  dpb_.Clear();
  parser_.Reset();
  last_output_poc_ = std::numeric_limits<int>::min();

  // If we are in kDecoding, we can resume without processing an SPS.
  if (state_ == kDecoding)
    state_ = kAfterReset;
}

void VaapiH264Decoder::ReuseSurface(
    const scoped_refptr<VASurface>& va_surface) {
  available_va_surfaces_.push_back(va_surface);
}

// Fill |va_pic| with default/neutral values.
static void InitVAPicture(VAPictureH264* va_pic) {
  memset(va_pic, 0, sizeof(*va_pic));
  va_pic->picture_id = VA_INVALID_ID;
  va_pic->flags = VA_PICTURE_H264_INVALID;
}

void VaapiH264Decoder::FillVAPicture(VAPictureH264 *va_pic, H264Picture* pic) {
  DCHECK(pic);

  DecodeSurface* dec_surface = DecodeSurfaceByPoC(pic->pic_order_cnt);
  if (!dec_surface) {
    // Cannot provide a ref picture, will corrupt output, but may be able
    // to recover.
    InitVAPicture(va_pic);
    return;
  }

  va_pic->picture_id = dec_surface->va_surface()->id();
  va_pic->frame_idx = pic->frame_num;
  va_pic->flags = 0;

  switch (pic->field) {
    case H264Picture::FIELD_NONE:
      break;
    case H264Picture::FIELD_TOP:
      va_pic->flags |= VA_PICTURE_H264_TOP_FIELD;
      break;
    case H264Picture::FIELD_BOTTOM:
      va_pic->flags |= VA_PICTURE_H264_BOTTOM_FIELD;
      break;
  }

  if (pic->ref) {
    va_pic->flags |= pic->long_term ? VA_PICTURE_H264_LONG_TERM_REFERENCE
                                    : VA_PICTURE_H264_SHORT_TERM_REFERENCE;
  }

  va_pic->TopFieldOrderCnt = pic->top_field_order_cnt;
  va_pic->BottomFieldOrderCnt = pic->bottom_field_order_cnt;
}

int VaapiH264Decoder::FillVARefFramesFromDPB(VAPictureH264 *va_pics,
                                             int num_pics) {
  H264DPB::Pictures::reverse_iterator rit;
  int i;

  // Return reference frames in reverse order of insertion.
  // Libva does not document this, but other implementations (e.g. mplayer)
  // do it this way as well.
  for (rit = dpb_.rbegin(), i = 0; rit != dpb_.rend() && i < num_pics; ++rit) {
    if ((*rit)->ref)
      FillVAPicture(&va_pics[i++], *rit);
  }

  return i;
}

VaapiH264Decoder::DecodeSurface* VaapiH264Decoder::DecodeSurfaceByPoC(int poc) {
  DecSurfacesInUse::iterator iter = decode_surfaces_in_use_.find(poc);
  if (iter == decode_surfaces_in_use_.end()) {
    DVLOG(1) << "Could not find surface assigned to POC: " << poc;
    return NULL;
  }

  return iter->second.get();
}

bool VaapiH264Decoder::AssignSurfaceToPoC(int32 input_id, int poc) {
  if (available_va_surfaces_.empty()) {
    DVLOG(1) << "No VA Surfaces available";
    return false;
  }

  linked_ptr<DecodeSurface> dec_surface(new DecodeSurface(
      poc, input_id, available_va_surfaces_.back()));
  available_va_surfaces_.pop_back();

  DVLOG(4) << "POC " << poc
           << " will use surface " << dec_surface->va_surface()->id();

  bool inserted = decode_surfaces_in_use_.insert(
      std::make_pair(poc, dec_surface)).second;
  DCHECK(inserted);

  return true;
}

void VaapiH264Decoder::UnassignSurfaceFromPoC(int poc) {
  DecSurfacesInUse::iterator it = decode_surfaces_in_use_.find(poc);
  if (it == decode_surfaces_in_use_.end()) {
    DVLOG(1) << "Asked to unassign an unassigned POC " << poc;
    return;
  }

  DVLOG(4) << "POC " << poc << " no longer using VA surface "
           << it->second->va_surface()->id();

  decode_surfaces_in_use_.erase(it);
}

bool VaapiH264Decoder::SendPPS() {
  const media::H264PPS* pps = parser_.GetPPS(curr_pps_id_);
  DCHECK(pps);

  const media::H264SPS* sps = parser_.GetSPS(pps->seq_parameter_set_id);
  DCHECK(sps);

  DCHECK(curr_pic_.get());

  VAPictureParameterBufferH264 pic_param;
  memset(&pic_param, 0, sizeof(VAPictureParameterBufferH264));

#define FROM_SPS_TO_PP(a) pic_param.a = sps->a;
#define FROM_SPS_TO_PP2(a, b) pic_param.b = sps->a;
  FROM_SPS_TO_PP2(pic_width_in_mbs_minus1, picture_width_in_mbs_minus1);
  // This assumes non-interlaced video
  FROM_SPS_TO_PP2(pic_height_in_map_units_minus1,
                  picture_height_in_mbs_minus1);
  FROM_SPS_TO_PP(bit_depth_luma_minus8);
  FROM_SPS_TO_PP(bit_depth_chroma_minus8);
#undef FROM_SPS_TO_PP
#undef FROM_SPS_TO_PP2

#define FROM_SPS_TO_PP_SF(a) pic_param.seq_fields.bits.a = sps->a;
#define FROM_SPS_TO_PP_SF2(a, b) pic_param.seq_fields.bits.b = sps->a;
  FROM_SPS_TO_PP_SF(chroma_format_idc);
  FROM_SPS_TO_PP_SF2(separate_colour_plane_flag,
                     residual_colour_transform_flag);
  FROM_SPS_TO_PP_SF(gaps_in_frame_num_value_allowed_flag);
  FROM_SPS_TO_PP_SF(frame_mbs_only_flag);
  FROM_SPS_TO_PP_SF(mb_adaptive_frame_field_flag);
  FROM_SPS_TO_PP_SF(direct_8x8_inference_flag);
  pic_param.seq_fields.bits.MinLumaBiPredSize8x8 = (sps->level_idc >= 31);
  FROM_SPS_TO_PP_SF(log2_max_frame_num_minus4);
  FROM_SPS_TO_PP_SF(pic_order_cnt_type);
  FROM_SPS_TO_PP_SF(log2_max_pic_order_cnt_lsb_minus4);
  FROM_SPS_TO_PP_SF(delta_pic_order_always_zero_flag);
#undef FROM_SPS_TO_PP_SF
#undef FROM_SPS_TO_PP_SF2

#define FROM_PPS_TO_PP(a) pic_param.a = pps->a;
  FROM_PPS_TO_PP(num_slice_groups_minus1);
  pic_param.slice_group_map_type = 0;
  pic_param.slice_group_change_rate_minus1 = 0;
  FROM_PPS_TO_PP(pic_init_qp_minus26);
  FROM_PPS_TO_PP(pic_init_qs_minus26);
  FROM_PPS_TO_PP(chroma_qp_index_offset);
  FROM_PPS_TO_PP(second_chroma_qp_index_offset);
#undef FROM_PPS_TO_PP

#define FROM_PPS_TO_PP_PF(a) pic_param.pic_fields.bits.a = pps->a;
#define FROM_PPS_TO_PP_PF2(a, b) pic_param.pic_fields.bits.b = pps->a;
  FROM_PPS_TO_PP_PF(entropy_coding_mode_flag);
  FROM_PPS_TO_PP_PF(weighted_pred_flag);
  FROM_PPS_TO_PP_PF(weighted_bipred_idc);
  FROM_PPS_TO_PP_PF(transform_8x8_mode_flag);

  pic_param.pic_fields.bits.field_pic_flag = 0;
  FROM_PPS_TO_PP_PF(constrained_intra_pred_flag);
  FROM_PPS_TO_PP_PF2(bottom_field_pic_order_in_frame_present_flag,
                pic_order_present_flag);
  FROM_PPS_TO_PP_PF(deblocking_filter_control_present_flag);
  FROM_PPS_TO_PP_PF(redundant_pic_cnt_present_flag);
  pic_param.pic_fields.bits.reference_pic_flag = curr_pic_->ref;
#undef FROM_PPS_TO_PP_PF
#undef FROM_PPS_TO_PP_PF2

  pic_param.frame_num = curr_pic_->frame_num;

  InitVAPicture(&pic_param.CurrPic);
  FillVAPicture(&pic_param.CurrPic, curr_pic_.get());

  // Init reference pictures' array.
  for (int i = 0; i < 16; ++i)
    InitVAPicture(&pic_param.ReferenceFrames[i]);

  // And fill it with picture info from DPB.
  FillVARefFramesFromDPB(pic_param.ReferenceFrames,
                         arraysize(pic_param.ReferenceFrames));

  pic_param.num_ref_frames = sps->max_num_ref_frames;

  return vaapi_wrapper_->SubmitBuffer(VAPictureParameterBufferType,
                                      sizeof(VAPictureParameterBufferH264),
                                      &pic_param);
}

bool VaapiH264Decoder::SendIQMatrix() {
  const media::H264PPS* pps = parser_.GetPPS(curr_pps_id_);
  DCHECK(pps);

  VAIQMatrixBufferH264 iq_matrix_buf;
  memset(&iq_matrix_buf, 0, sizeof(VAIQMatrixBufferH264));

  if (pps->pic_scaling_matrix_present_flag) {
    for (int i = 0; i < 6; ++i) {
      for (int j = 0; j < 16; ++j)
        iq_matrix_buf.ScalingList4x4[i][j] = pps->scaling_list4x4[i][j];
    }

    for (int i = 0; i < 2; ++i) {
      for (int j = 0; j < 64; ++j)
        iq_matrix_buf.ScalingList8x8[i][j] = pps->scaling_list8x8[i][j];
    }
  } else {
    const media::H264SPS* sps = parser_.GetSPS(pps->seq_parameter_set_id);
    DCHECK(sps);
    for (int i = 0; i < 6; ++i) {
      for (int j = 0; j < 16; ++j)
        iq_matrix_buf.ScalingList4x4[i][j] = sps->scaling_list4x4[i][j];
    }

    for (int i = 0; i < 2; ++i) {
      for (int j = 0; j < 64; ++j)
        iq_matrix_buf.ScalingList8x8[i][j] = sps->scaling_list8x8[i][j];
    }
  }

  return vaapi_wrapper_->SubmitBuffer(VAIQMatrixBufferType,
                                      sizeof(VAIQMatrixBufferH264),
                                      &iq_matrix_buf);
}

bool VaapiH264Decoder::SendVASliceParam(media::H264SliceHeader* slice_hdr) {
  const media::H264PPS* pps = parser_.GetPPS(slice_hdr->pic_parameter_set_id);
  DCHECK(pps);

  const media::H264SPS* sps = parser_.GetSPS(pps->seq_parameter_set_id);
  DCHECK(sps);

  VASliceParameterBufferH264 slice_param;
  memset(&slice_param, 0, sizeof(VASliceParameterBufferH264));

  slice_param.slice_data_size = slice_hdr->nalu_size;
  slice_param.slice_data_offset = 0;
  slice_param.slice_data_flag = VA_SLICE_DATA_FLAG_ALL;
  slice_param.slice_data_bit_offset = slice_hdr->header_bit_size;

#define SHDRToSP(a) slice_param.a = slice_hdr->a;
  SHDRToSP(first_mb_in_slice);
  slice_param.slice_type = slice_hdr->slice_type % 5;
  SHDRToSP(direct_spatial_mv_pred_flag);

  // TODO posciak: make sure parser sets those even when override flags
  // in slice header is off.
  SHDRToSP(num_ref_idx_l0_active_minus1);
  SHDRToSP(num_ref_idx_l1_active_minus1);
  SHDRToSP(cabac_init_idc);
  SHDRToSP(slice_qp_delta);
  SHDRToSP(disable_deblocking_filter_idc);
  SHDRToSP(slice_alpha_c0_offset_div2);
  SHDRToSP(slice_beta_offset_div2);

  if (((slice_hdr->IsPSlice() || slice_hdr->IsSPSlice()) &&
       pps->weighted_pred_flag) ||
      (slice_hdr->IsBSlice() && pps->weighted_bipred_idc == 1)) {
    SHDRToSP(luma_log2_weight_denom);
    SHDRToSP(chroma_log2_weight_denom);

    SHDRToSP(luma_weight_l0_flag);
    SHDRToSP(luma_weight_l1_flag);

    SHDRToSP(chroma_weight_l0_flag);
    SHDRToSP(chroma_weight_l1_flag);

    for (int i = 0; i <= slice_param.num_ref_idx_l0_active_minus1; ++i) {
      slice_param.luma_weight_l0[i] =
          slice_hdr->pred_weight_table_l0.luma_weight[i];
      slice_param.luma_offset_l0[i] =
          slice_hdr->pred_weight_table_l0.luma_offset[i];

      for (int j = 0; j < 2; ++j) {
        slice_param.chroma_weight_l0[i][j] =
            slice_hdr->pred_weight_table_l0.chroma_weight[i][j];
        slice_param.chroma_offset_l0[i][j] =
            slice_hdr->pred_weight_table_l0.chroma_offset[i][j];
      }
    }

    if (slice_hdr->IsBSlice()) {
      for (int i = 0; i <= slice_param.num_ref_idx_l1_active_minus1; ++i) {
        slice_param.luma_weight_l1[i] =
            slice_hdr->pred_weight_table_l1.luma_weight[i];
        slice_param.luma_offset_l1[i] =
            slice_hdr->pred_weight_table_l1.luma_offset[i];

        for (int j = 0; j < 2; ++j) {
          slice_param.chroma_weight_l1[i][j] =
              slice_hdr->pred_weight_table_l1.chroma_weight[i][j];
          slice_param.chroma_offset_l1[i][j] =
              slice_hdr->pred_weight_table_l1.chroma_offset[i][j];
        }
      }
    }
  }

  for (int i = 0; i < 32; ++i) {
    InitVAPicture(&slice_param.RefPicList0[i]);
    InitVAPicture(&slice_param.RefPicList1[i]);
  }

  int i;
  H264Picture::PtrVector::iterator it;
  for (it = ref_pic_list0_.begin(), i = 0; it != ref_pic_list0_.end() && *it;
       ++it, ++i)
    FillVAPicture(&slice_param.RefPicList0[i], *it);
  for (it = ref_pic_list1_.begin(), i = 0; it != ref_pic_list1_.end() && *it;
       ++it, ++i)
    FillVAPicture(&slice_param.RefPicList1[i], *it);

  return vaapi_wrapper_->SubmitBuffer(VASliceParameterBufferType,
                                      sizeof(VASliceParameterBufferH264),
                                      &slice_param);
}

bool VaapiH264Decoder::SendSliceData(const uint8* ptr, size_t size) {
  // Can't help it, blame libva...
  void* non_const_ptr = const_cast<uint8*>(ptr);
  return vaapi_wrapper_->SubmitBuffer(VASliceDataBufferType, size,
                                      non_const_ptr);
}

bool VaapiH264Decoder::PrepareRefPicLists(media::H264SliceHeader* slice_hdr) {
  ref_pic_list0_.clear();
  ref_pic_list1_.clear();

  // Fill reference picture lists for B and S/SP slices.
  if (slice_hdr->IsPSlice() || slice_hdr->IsSPSlice()) {
    ConstructReferencePicListsP(slice_hdr);
    return ModifyReferencePicList(slice_hdr, 0);
  }

  if (slice_hdr->IsBSlice()) {
    ConstructReferencePicListsB(slice_hdr);
    return ModifyReferencePicList(slice_hdr, 0) &&
        ModifyReferencePicList(slice_hdr, 1);
  }

  return true;
}

bool VaapiH264Decoder::QueueSlice(media::H264SliceHeader* slice_hdr) {
  DCHECK(curr_pic_.get());

  if (!PrepareRefPicLists(slice_hdr))
    return false;

  if (!SendVASliceParam(slice_hdr))
    return false;

  if (!SendSliceData(slice_hdr->nalu_data, slice_hdr->nalu_size))
    return false;

  return true;
}

// TODO(posciak) start using vaMapBuffer instead of vaCreateBuffer wherever
// possible.
bool VaapiH264Decoder::DecodePicture() {
  DCHECK(curr_pic_.get());

  DVLOG(4) << "Decoding POC " << curr_pic_->pic_order_cnt;
  DecodeSurface* dec_surface = DecodeSurfaceByPoC(curr_pic_->pic_order_cnt);
  if (!dec_surface) {
    DVLOG(1) << "Asked to decode an invalid POC " << curr_pic_->pic_order_cnt;
    return false;
  }

  if (!vaapi_wrapper_->DecodeAndDestroyPendingBuffers(
      dec_surface->va_surface()->id())) {
    DVLOG(1) << "Failed decoding picture";
    return false;
  }

  return true;
}

bool VaapiH264Decoder::InitCurrPicture(media::H264SliceHeader* slice_hdr) {
  DCHECK(curr_pic_.get());

  memset(curr_pic_.get(), 0, sizeof(H264Picture));

  curr_pic_->idr = slice_hdr->idr_pic_flag;

  if (slice_hdr->field_pic_flag) {
    curr_pic_->field = slice_hdr->bottom_field_flag ? H264Picture::FIELD_BOTTOM
                                                    : H264Picture::FIELD_TOP;
  } else {
    curr_pic_->field = H264Picture::FIELD_NONE;
  }

  curr_pic_->ref = slice_hdr->nal_ref_idc != 0;
  // This assumes non-interlaced stream.
  curr_pic_->frame_num = curr_pic_->pic_num = slice_hdr->frame_num;

  if (!CalculatePicOrderCounts(slice_hdr))
    return false;

  // Try to get an empty surface to decode this picture to.
  if (!AssignSurfaceToPoC(curr_input_id_, curr_pic_->pic_order_cnt)) {
    DVLOG(1) << "Failed getting a free surface for a picture";
    return false;
  }

  curr_pic_->long_term_reference_flag = slice_hdr->long_term_reference_flag;
  curr_pic_->adaptive_ref_pic_marking_mode_flag =
      slice_hdr->adaptive_ref_pic_marking_mode_flag;

  // If the slice header indicates we will have to perform reference marking
  // process after this picture is decoded, store required data for that
  // purpose.
  if (slice_hdr->adaptive_ref_pic_marking_mode_flag) {
    COMPILE_ASSERT(sizeof(curr_pic_->ref_pic_marking) ==
                   sizeof(slice_hdr->ref_pic_marking),
                   ref_pic_marking_array_sizes_do_not_match);
    memcpy(curr_pic_->ref_pic_marking, slice_hdr->ref_pic_marking,
           sizeof(curr_pic_->ref_pic_marking));
  }

  return true;
}

bool VaapiH264Decoder::CalculatePicOrderCounts(
    media::H264SliceHeader* slice_hdr) {
  DCHECK_NE(curr_sps_id_, -1);
  const media::H264SPS* sps = parser_.GetSPS(curr_sps_id_);

  int pic_order_cnt_lsb = slice_hdr->pic_order_cnt_lsb;
  curr_pic_->pic_order_cnt_lsb = pic_order_cnt_lsb;

  switch (sps->pic_order_cnt_type) {
    case 0:
      // See spec 8.2.1.1.
      int prev_pic_order_cnt_msb, prev_pic_order_cnt_lsb;
      if (slice_hdr->idr_pic_flag) {
        prev_pic_order_cnt_msb = prev_pic_order_cnt_lsb = 0;
      } else {
        if (prev_ref_has_memmgmnt5_) {
          if (prev_ref_field_ != H264Picture::FIELD_BOTTOM) {
            prev_pic_order_cnt_msb = 0;
            prev_pic_order_cnt_lsb = prev_ref_top_field_order_cnt_;
          } else {
            prev_pic_order_cnt_msb = 0;
            prev_pic_order_cnt_lsb = 0;
          }
        } else {
          prev_pic_order_cnt_msb = prev_ref_pic_order_cnt_msb_;
          prev_pic_order_cnt_lsb = prev_ref_pic_order_cnt_lsb_;
        }
      }

      DCHECK_NE(max_pic_order_cnt_lsb_, 0);
      if ((pic_order_cnt_lsb < prev_pic_order_cnt_lsb) &&
          (prev_pic_order_cnt_lsb - pic_order_cnt_lsb >=
           max_pic_order_cnt_lsb_ / 2)) {
        curr_pic_->pic_order_cnt_msb = prev_pic_order_cnt_msb +
          max_pic_order_cnt_lsb_;
      } else if ((pic_order_cnt_lsb > prev_pic_order_cnt_lsb) &&
          (pic_order_cnt_lsb - prev_pic_order_cnt_lsb >
           max_pic_order_cnt_lsb_ / 2)) {
        curr_pic_->pic_order_cnt_msb = prev_pic_order_cnt_msb -
          max_pic_order_cnt_lsb_;
      } else {
        curr_pic_->pic_order_cnt_msb = prev_pic_order_cnt_msb;
      }

      if (curr_pic_->field != H264Picture::FIELD_BOTTOM) {
        curr_pic_->top_field_order_cnt = curr_pic_->pic_order_cnt_msb +
          pic_order_cnt_lsb;
      }

      if (curr_pic_->field != H264Picture::FIELD_TOP) {
        // TODO posciak: perhaps replace with pic->field?
        if (!slice_hdr->field_pic_flag) {
          curr_pic_->bottom_field_order_cnt = curr_pic_->top_field_order_cnt +
            slice_hdr->delta_pic_order_cnt_bottom;
        } else {
          curr_pic_->bottom_field_order_cnt = curr_pic_->pic_order_cnt_msb +
            pic_order_cnt_lsb;
        }
      }
      break;

    case 1: {
      // See spec 8.2.1.2.
      if (prev_has_memmgmnt5_)
        prev_frame_num_offset_ = 0;

      if (slice_hdr->idr_pic_flag)
        curr_pic_->frame_num_offset = 0;
      else if (prev_frame_num_ > slice_hdr->frame_num)
        curr_pic_->frame_num_offset = prev_frame_num_offset_ + max_frame_num_;
      else
        curr_pic_->frame_num_offset = prev_frame_num_offset_;

      int abs_frame_num = 0;
      if (sps->num_ref_frames_in_pic_order_cnt_cycle != 0)
        abs_frame_num = curr_pic_->frame_num_offset + slice_hdr->frame_num;
      else
        abs_frame_num = 0;

      if (slice_hdr->nal_ref_idc == 0 && abs_frame_num > 0)
        --abs_frame_num;

      int expected_pic_order_cnt = 0;
      if (abs_frame_num > 0) {
        if (sps->num_ref_frames_in_pic_order_cnt_cycle == 0) {
          DVLOG(1) << "Invalid num_ref_frames_in_pic_order_cnt_cycle "
                   << "in stream";
          return false;
        }

        int pic_order_cnt_cycle_cnt = (abs_frame_num - 1) /
            sps->num_ref_frames_in_pic_order_cnt_cycle;
        int frame_num_in_pic_order_cnt_cycle = (abs_frame_num - 1) %
            sps->num_ref_frames_in_pic_order_cnt_cycle;

        expected_pic_order_cnt = pic_order_cnt_cycle_cnt *
            sps->expected_delta_per_pic_order_cnt_cycle;
        // frame_num_in_pic_order_cnt_cycle is verified < 255 in parser
        for (int i = 0; i <= frame_num_in_pic_order_cnt_cycle; ++i)
          expected_pic_order_cnt += sps->offset_for_ref_frame[i];
      }

      if (!slice_hdr->nal_ref_idc)
        expected_pic_order_cnt += sps->offset_for_non_ref_pic;

      if (!slice_hdr->field_pic_flag) {
        curr_pic_->top_field_order_cnt = expected_pic_order_cnt +
            slice_hdr->delta_pic_order_cnt[0];
        curr_pic_->bottom_field_order_cnt = curr_pic_->top_field_order_cnt +
            sps->offset_for_top_to_bottom_field +
            slice_hdr->delta_pic_order_cnt[1];
      } else if (!slice_hdr->bottom_field_flag) {
        curr_pic_->top_field_order_cnt = expected_pic_order_cnt +
            slice_hdr->delta_pic_order_cnt[0];
      } else {
        curr_pic_->bottom_field_order_cnt = expected_pic_order_cnt +
            sps->offset_for_top_to_bottom_field +
            slice_hdr->delta_pic_order_cnt[0];
      }
      break;
    }

    case 2:
      // See spec 8.2.1.3.
      if (prev_has_memmgmnt5_)
        prev_frame_num_offset_ = 0;

      if (slice_hdr->idr_pic_flag)
        curr_pic_->frame_num_offset = 0;
      else if (prev_frame_num_ > slice_hdr->frame_num)
        curr_pic_->frame_num_offset = prev_frame_num_offset_ + max_frame_num_;
      else
        curr_pic_->frame_num_offset = prev_frame_num_offset_;

      int temp_pic_order_cnt;
      if (slice_hdr->idr_pic_flag) {
        temp_pic_order_cnt = 0;
      } else if (!slice_hdr->nal_ref_idc) {
        temp_pic_order_cnt =
            2 * (curr_pic_->frame_num_offset + slice_hdr->frame_num) - 1;
      } else {
        temp_pic_order_cnt = 2 * (curr_pic_->frame_num_offset +
            slice_hdr->frame_num);
      }

      if (!slice_hdr->field_pic_flag) {
        curr_pic_->top_field_order_cnt = temp_pic_order_cnt;
        curr_pic_->bottom_field_order_cnt = temp_pic_order_cnt;
      } else if (slice_hdr->bottom_field_flag) {
        curr_pic_->bottom_field_order_cnt = temp_pic_order_cnt;
      } else {
        curr_pic_->top_field_order_cnt = temp_pic_order_cnt;
      }
      break;

    default:
      DVLOG(1) << "Invalid pic_order_cnt_type: " << sps->pic_order_cnt_type;
      return false;
  }

  switch (curr_pic_->field) {
    case H264Picture::FIELD_NONE:
      curr_pic_->pic_order_cnt = std::min(curr_pic_->top_field_order_cnt,
                                          curr_pic_->bottom_field_order_cnt);
      break;
    case H264Picture::FIELD_TOP:
      curr_pic_->pic_order_cnt = curr_pic_->top_field_order_cnt;
      break;
    case H264Picture::FIELD_BOTTOM:
      curr_pic_->pic_order_cnt = curr_pic_->bottom_field_order_cnt;
      break;
  }

  return true;
}

void VaapiH264Decoder::UpdatePicNums() {
  for (H264DPB::Pictures::iterator it = dpb_.begin(); it != dpb_.end(); ++it) {
    H264Picture* pic = *it;
    DCHECK(pic);
    if (!pic->ref)
      continue;

    // Below assumes non-interlaced stream.
    DCHECK_EQ(pic->field, H264Picture::FIELD_NONE);
    if (pic->long_term) {
      pic->long_term_pic_num = pic->long_term_frame_idx;
    } else {
      if (pic->frame_num > frame_num_)
        pic->frame_num_wrap = pic->frame_num - max_frame_num_;
      else
        pic->frame_num_wrap = pic->frame_num;

      pic->pic_num = pic->frame_num_wrap;
    }
  }
}

struct PicNumDescCompare {
  bool operator()(const H264Picture* a, const H264Picture* b) const {
    return a->pic_num > b->pic_num;
  }
};

struct LongTermPicNumAscCompare {
  bool operator()(const H264Picture* a, const H264Picture* b) const {
    return a->long_term_pic_num < b->long_term_pic_num;
  }
};

void VaapiH264Decoder::ConstructReferencePicListsP(
    media::H264SliceHeader* slice_hdr) {
  // RefPicList0 (8.2.4.2.1) [[1] [2]], where:
  // [1] shortterm ref pics sorted by descending pic_num,
  // [2] longterm ref pics by ascending long_term_pic_num.
  DCHECK(ref_pic_list0_.empty() && ref_pic_list1_.empty());
  // First get the short ref pics...
  dpb_.GetShortTermRefPicsAppending(ref_pic_list0_);
  size_t num_short_refs = ref_pic_list0_.size();

  // and sort them to get [1].
  std::sort(ref_pic_list0_.begin(), ref_pic_list0_.end(), PicNumDescCompare());

  // Now get long term pics and sort them by long_term_pic_num to get [2].
  dpb_.GetLongTermRefPicsAppending(ref_pic_list0_);
  std::sort(ref_pic_list0_.begin() + num_short_refs, ref_pic_list0_.end(),
            LongTermPicNumAscCompare());

  // Cut off if we have more than requested in slice header.
  ref_pic_list0_.resize(slice_hdr->num_ref_idx_l0_active_minus1 + 1);
}

struct POCAscCompare {
  bool operator()(const H264Picture* a, const H264Picture* b) const {
    return a->pic_order_cnt < b->pic_order_cnt;
  }
};

struct POCDescCompare {
  bool operator()(const H264Picture* a, const H264Picture* b) const {
    return a->pic_order_cnt > b->pic_order_cnt;
  }
};

void VaapiH264Decoder::ConstructReferencePicListsB(
    media::H264SliceHeader* slice_hdr) {
  // RefPicList0 (8.2.4.2.3) [[1] [2] [3]], where:
  // [1] shortterm ref pics with POC < curr_pic's POC sorted by descending POC,
  // [2] shortterm ref pics with POC > curr_pic's POC by ascending POC,
  // [3] longterm ref pics by ascending long_term_pic_num.
  DCHECK(ref_pic_list0_.empty() && ref_pic_list1_.empty());
  dpb_.GetShortTermRefPicsAppending(ref_pic_list0_);
  size_t num_short_refs = ref_pic_list0_.size();

  // First sort ascending, this will put [1] in right place and finish [2].
  std::sort(ref_pic_list0_.begin(), ref_pic_list0_.end(), POCAscCompare());

  // Find first with POC > curr_pic's POC to get first element in [2]...
  H264Picture::PtrVector::iterator iter;
  iter = std::upper_bound(ref_pic_list0_.begin(), ref_pic_list0_.end(),
                          curr_pic_.get(), POCAscCompare());

  // and sort [1] descending, thus finishing sequence [1] [2].
  std::sort(ref_pic_list0_.begin(), iter, POCDescCompare());

  // Now add [3] and sort by ascending long_term_pic_num.
  dpb_.GetLongTermRefPicsAppending(ref_pic_list0_);
  std::sort(ref_pic_list0_.begin() + num_short_refs, ref_pic_list0_.end(),
            LongTermPicNumAscCompare());

  // RefPicList1 (8.2.4.2.4) [[1] [2] [3]], where:
  // [1] shortterm ref pics with POC > curr_pic's POC sorted by ascending POC,
  // [2] shortterm ref pics with POC < curr_pic's POC by descending POC,
  // [3] longterm ref pics by ascending long_term_pic_num.

  dpb_.GetShortTermRefPicsAppending(ref_pic_list1_);
  num_short_refs = ref_pic_list1_.size();

  // First sort by descending POC.
  std::sort(ref_pic_list1_.begin(), ref_pic_list1_.end(), POCDescCompare());

  // Find first with POC < curr_pic's POC to get first element in [2]...
  iter = std::upper_bound(ref_pic_list1_.begin(), ref_pic_list1_.end(),
                          curr_pic_.get(), POCDescCompare());

  // and sort [1] ascending.
  std::sort(ref_pic_list1_.begin(), iter, POCAscCompare());

  // Now add [3] and sort by ascending long_term_pic_num
  dpb_.GetShortTermRefPicsAppending(ref_pic_list1_);
  std::sort(ref_pic_list1_.begin() + num_short_refs, ref_pic_list1_.end(),
            LongTermPicNumAscCompare());

  // If lists identical, swap first two entries in RefPicList1 (spec 8.2.4.2.3)
  if (ref_pic_list1_.size() > 1 &&
      std::equal(ref_pic_list0_.begin(), ref_pic_list0_.end(),
                 ref_pic_list1_.begin()))
    std::swap(ref_pic_list1_[0], ref_pic_list1_[1]);

  // Per 8.2.4.2 it's possible for num_ref_idx_lX_active_minus1 to indicate
  // there should be more ref pics on list than we constructed.
  // Those superfluous ones should be treated as non-reference.
  ref_pic_list0_.resize(slice_hdr->num_ref_idx_l0_active_minus1 + 1);
  ref_pic_list1_.resize(slice_hdr->num_ref_idx_l1_active_minus1 + 1);
}

// See 8.2.4
int VaapiH264Decoder::PicNumF(H264Picture *pic) {
  if (!pic)
      return -1;

  if (!pic->long_term)
      return pic->pic_num;
  else
      return max_pic_num_;
}

// See 8.2.4
int VaapiH264Decoder::LongTermPicNumF(H264Picture *pic) {
  if (pic->ref && pic->long_term)
    return pic->long_term_pic_num;
  else
    return 2 * (max_long_term_frame_idx_ + 1);
}

// Shift elements on the |v| starting from |from| to |to|, inclusive,
// one position to the right and insert pic at |from|.
static void ShiftRightAndInsert(H264Picture::PtrVector *v,
                                int from,
                                int to,
                                H264Picture* pic) {
  // Security checks, do not disable in Debug mode.
  CHECK(from <= to);
  CHECK(to <= std::numeric_limits<int>::max() - 2);
  // Additional checks. Debug mode ok.
  DCHECK(v);
  DCHECK(pic);
  DCHECK((to + 1 == static_cast<int>(v->size())) ||
         (to + 2 == static_cast<int>(v->size())));

  v->resize(to + 2);

  for (int i = to + 1; i > from; --i)
    (*v)[i] = (*v)[i - 1];

  (*v)[from] = pic;
}

bool VaapiH264Decoder::ModifyReferencePicList(media::H264SliceHeader* slice_hdr,
                                              int list) {
  int num_ref_idx_lX_active_minus1;
  H264Picture::PtrVector* ref_pic_listx;
  media::H264ModificationOfPicNum* list_mod;

  // This can process either ref_pic_list0 or ref_pic_list1, depending on
  // the list argument. Set up pointers to proper list to be processed here.
  if (list == 0) {
    if (!slice_hdr->ref_pic_list_modification_flag_l0)
      return true;

    list_mod = slice_hdr->ref_list_l0_modifications;
    num_ref_idx_lX_active_minus1 = ref_pic_list0_.size() - 1;

    ref_pic_listx = &ref_pic_list0_;
  } else {
    if (!slice_hdr->ref_pic_list_modification_flag_l1)
      return true;

    list_mod = slice_hdr->ref_list_l1_modifications;
    num_ref_idx_lX_active_minus1 = ref_pic_list1_.size() - 1;

    ref_pic_listx = &ref_pic_list1_;
  }

  DCHECK_GE(num_ref_idx_lX_active_minus1, 0);

  // Spec 8.2.4.3:
  // Reorder pictures on the list in a way specified in the stream.
  int pic_num_lx_pred = curr_pic_->pic_num;
  int ref_idx_lx = 0;
  int pic_num_lx_no_wrap;
  int pic_num_lx;
  bool done = false;
  H264Picture* pic;
  for (int i = 0; i < media::H264SliceHeader::kRefListModSize && !done; ++i) {
    switch (list_mod->modification_of_pic_nums_idc) {
      case 0:
      case 1:
        // Modify short reference picture position.
        if (list_mod->modification_of_pic_nums_idc == 0) {
          // Subtract given value from predicted PicNum.
          pic_num_lx_no_wrap = pic_num_lx_pred -
              (static_cast<int>(list_mod->abs_diff_pic_num_minus1) + 1);
          // Wrap around max_pic_num_ if it becomes < 0 as result
          // of subtraction.
          if (pic_num_lx_no_wrap < 0)
            pic_num_lx_no_wrap += max_pic_num_;
        } else {
          // Add given value to predicted PicNum.
          pic_num_lx_no_wrap = pic_num_lx_pred +
              (static_cast<int>(list_mod->abs_diff_pic_num_minus1) + 1);
          // Wrap around max_pic_num_ if it becomes >= max_pic_num_ as result
          // of the addition.
          if (pic_num_lx_no_wrap >= max_pic_num_)
            pic_num_lx_no_wrap -= max_pic_num_;
        }

        // For use in next iteration.
        pic_num_lx_pred = pic_num_lx_no_wrap;

        if (pic_num_lx_no_wrap > curr_pic_->pic_num)
          pic_num_lx = pic_num_lx_no_wrap - max_pic_num_;
        else
          pic_num_lx = pic_num_lx_no_wrap;

        DCHECK_LT(num_ref_idx_lX_active_minus1 + 1,
                  media::H264SliceHeader::kRefListModSize);
        pic = dpb_.GetShortRefPicByPicNum(pic_num_lx);
        if (!pic) {
          DVLOG(1) << "Malformed stream, no pic num " << pic_num_lx;
          return false;
        }
        ShiftRightAndInsert(ref_pic_listx, ref_idx_lx,
                            num_ref_idx_lX_active_minus1, pic);
        ref_idx_lx++;

        for (int src = ref_idx_lx, dst = ref_idx_lx;
             src <= num_ref_idx_lX_active_minus1 + 1; ++src) {
          if (PicNumF((*ref_pic_listx)[src]) != pic_num_lx)
            (*ref_pic_listx)[dst++] = (*ref_pic_listx)[src];
        }
        break;

      case 2:
        // Modify long term reference picture position.
        DCHECK_LT(num_ref_idx_lX_active_minus1 + 1,
                  media::H264SliceHeader::kRefListModSize);
        pic = dpb_.GetLongRefPicByLongTermPicNum(list_mod->long_term_pic_num);
        if (!pic) {
          DVLOG(1) << "Malformed stream, no pic num "
                   << list_mod->long_term_pic_num;
          return false;
        }
        ShiftRightAndInsert(ref_pic_listx, ref_idx_lx,
                            num_ref_idx_lX_active_minus1, pic);
        ref_idx_lx++;

        for (int src = ref_idx_lx, dst = ref_idx_lx;
             src <= num_ref_idx_lX_active_minus1 + 1; ++src) {
          if (LongTermPicNumF((*ref_pic_listx)[src])
              != static_cast<int>(list_mod->long_term_pic_num))
            (*ref_pic_listx)[dst++] = (*ref_pic_listx)[src];
        }
        break;

      case 3:
        // End of modification list.
        done = true;
        break;

      default:
        // May be recoverable.
        DVLOG(1) << "Invalid modification_of_pic_nums_idc="
                 << list_mod->modification_of_pic_nums_idc
                 << " in position " << i;
        break;
    }

    ++list_mod;
  }

  // Per NOTE 2 in 8.2.4.3.2, the ref_pic_listx size in the above loop is
  // temporarily made one element longer than the required final list.
  // Resize the list back to its required size.
  ref_pic_listx->resize(num_ref_idx_lX_active_minus1 + 1);

  return true;
}

bool VaapiH264Decoder::OutputPic(H264Picture* pic) {
  DCHECK(!pic->outputted);
  pic->outputted = true;
  last_output_poc_ = pic->pic_order_cnt;

  DecodeSurface* dec_surface = DecodeSurfaceByPoC(pic->pic_order_cnt);
  if (!dec_surface)
    return false;

  DCHECK_GE(dec_surface->input_id(), 0);
  DVLOG(4) << "Posting output task for POC: " << pic->pic_order_cnt
           << " input_id: " << dec_surface->input_id();
  output_pic_cb_.Run(dec_surface->input_id(), dec_surface->va_surface());

  return true;
}

void VaapiH264Decoder::ClearDPB() {
  // Clear DPB contents, marking the pictures as unused first.
  for (H264DPB::Pictures::iterator it = dpb_.begin(); it != dpb_.end(); ++it)
    UnassignSurfaceFromPoC((*it)->pic_order_cnt);

  dpb_.Clear();
  last_output_poc_ = std::numeric_limits<int>::min();
}

bool VaapiH264Decoder::OutputAllRemainingPics() {
  // Output all pictures that are waiting to be outputted.
  FinishPrevFrameIfPresent();
  H264Picture::PtrVector to_output;
  dpb_.GetNotOutputtedPicsAppending(to_output);
  // Sort them by ascending POC to output in order.
  std::sort(to_output.begin(), to_output.end(), POCAscCompare());

  H264Picture::PtrVector::iterator it;
  for (it = to_output.begin(); it != to_output.end(); ++it) {
    if (!OutputPic(*it)) {
      DVLOG(1) << "Failed to output pic POC: " << (*it)->pic_order_cnt;
      return false;
    }
  }

  return true;
}

bool VaapiH264Decoder::Flush() {
  DVLOG(2) << "Decoder flush";

  if (!OutputAllRemainingPics())
    return false;

  ClearDPB();

  DCHECK(decode_surfaces_in_use_.empty());
  return true;
}

bool VaapiH264Decoder::StartNewFrame(media::H264SliceHeader* slice_hdr) {
  // TODO posciak: add handling of max_num_ref_frames per spec.

  // If the new frame is an IDR, output what's left to output and clear DPB
  if (slice_hdr->idr_pic_flag) {
    // (unless we are explicitly instructed not to do so).
    if (!slice_hdr->no_output_of_prior_pics_flag) {
      // Output DPB contents.
      if (!Flush())
        return false;
    }
    dpb_.Clear();
    last_output_poc_ = std::numeric_limits<int>::min();
  }

  // curr_pic_ should have either been added to DPB or discarded when finishing
  // the last frame. DPB is responsible for releasing that memory once it's
  // not needed anymore.
  DCHECK(!curr_pic_.get());
  curr_pic_.reset(new H264Picture);
  CHECK(curr_pic_.get());

  if (!InitCurrPicture(slice_hdr))
    return false;

  DCHECK_GT(max_frame_num_, 0);

  UpdatePicNums();

  // Send parameter buffers before each new picture, before the first slice.
  if (!SendPPS())
    return false;

  if (!SendIQMatrix())
    return false;

  if (!QueueSlice(slice_hdr))
    return false;

  return true;
}

bool VaapiH264Decoder::HandleMemoryManagementOps() {
  // 8.2.5.4
  for (unsigned int i = 0; i < arraysize(curr_pic_->ref_pic_marking); ++i) {
    // Code below does not support interlaced stream (per-field pictures).
    media::H264DecRefPicMarking* ref_pic_marking =
        &curr_pic_->ref_pic_marking[i];
    H264Picture* to_mark;
    int pic_num_x;

    switch (ref_pic_marking->memory_mgmnt_control_operation) {
      case 0:
        // Normal end of operations' specification.
        return true;

      case 1:
        // Mark a short term reference picture as unused so it can be removed
        // if outputted.
        pic_num_x = curr_pic_->pic_num -
            (ref_pic_marking->difference_of_pic_nums_minus1 + 1);
        to_mark = dpb_.GetShortRefPicByPicNum(pic_num_x);
        if (to_mark) {
          to_mark->ref = false;
        } else {
          DVLOG(1) << "Invalid short ref pic num to unmark";
          return false;
        }
        break;

      case 2:
        // Mark a long term reference picture as unused so it can be removed
        // if outputted.
        to_mark = dpb_.GetLongRefPicByLongTermPicNum(
            ref_pic_marking->long_term_pic_num);
        if (to_mark) {
          to_mark->ref = false;
        } else {
          DVLOG(1) << "Invalid long term ref pic num to unmark";
          return false;
        }
        break;

      case 3:
        // Mark a short term reference picture as long term reference.
        pic_num_x = curr_pic_->pic_num -
            (ref_pic_marking->difference_of_pic_nums_minus1 + 1);
        to_mark = dpb_.GetShortRefPicByPicNum(pic_num_x);
        if (to_mark) {
          DCHECK(to_mark->ref && !to_mark->long_term);
          to_mark->long_term = true;
          to_mark->long_term_frame_idx = ref_pic_marking->long_term_frame_idx;
        } else {
          DVLOG(1) << "Invalid short term ref pic num to mark as long ref";
          return false;
        }
        break;

      case 4: {
        // Unmark all reference pictures with long_term_frame_idx over new max.
        max_long_term_frame_idx_
            = ref_pic_marking->max_long_term_frame_idx_plus1 - 1;
        H264Picture::PtrVector long_terms;
        dpb_.GetLongTermRefPicsAppending(long_terms);
        for (size_t i = 0; i < long_terms.size(); ++i) {
          H264Picture* pic = long_terms[i];
          DCHECK(pic->ref && pic->long_term);
          // Ok to cast, max_long_term_frame_idx is much smaller than 16bit.
          if (pic->long_term_frame_idx >
              static_cast<int>(max_long_term_frame_idx_))
            pic->ref = false;
        }
        break;
      }

      case 5:
        // Unmark all reference pictures.
        dpb_.MarkAllUnusedForRef();
        max_long_term_frame_idx_ = -1;
        curr_pic_->mem_mgmt_5 = true;
        break;

      case 6: {
        // Replace long term reference pictures with current picture.
        // First unmark if any existing with this long_term_frame_idx...
        H264Picture::PtrVector long_terms;
        dpb_.GetLongTermRefPicsAppending(long_terms);
        for (size_t i = 0; i < long_terms.size(); ++i) {
          H264Picture* pic = long_terms[i];
          DCHECK(pic->ref && pic->long_term);
          // Ok to cast, long_term_frame_idx is much smaller than 16bit.
          if (pic->long_term_frame_idx ==
              static_cast<int>(ref_pic_marking->long_term_frame_idx))
            pic->ref = false;
        }

        // and mark the current one instead.
        curr_pic_->ref = true;
        curr_pic_->long_term = true;
        curr_pic_->long_term_frame_idx = ref_pic_marking->long_term_frame_idx;
        break;
      }

      default:
        // Would indicate a bug in parser.
        NOTREACHED();
    }
  }

  return true;
}

// This method ensures that DPB does not overflow, either by removing
// reference pictures as specified in the stream, or using a sliding window
// procedure to remove the oldest one.
// It also performs marking and unmarking pictures as reference.
// See spac 8.2.5.1.
void VaapiH264Decoder::ReferencePictureMarking() {
  if (curr_pic_->idr) {
    // If current picture is an IDR, all reference pictures are unmarked.
    dpb_.MarkAllUnusedForRef();

    if (curr_pic_->long_term_reference_flag) {
      curr_pic_->long_term = true;
      curr_pic_->long_term_frame_idx = 0;
      max_long_term_frame_idx_ = 0;
    } else {
      curr_pic_->long_term = false;
      max_long_term_frame_idx_ = -1;
    }
  } else {
    if (!curr_pic_->adaptive_ref_pic_marking_mode_flag) {
      // If non-IDR, and the stream does not indicate what we should do to
      // ensure DPB doesn't overflow, discard oldest picture.
      // See spec 8.2.5.3.
      if (curr_pic_->field == H264Picture::FIELD_NONE) {
        DCHECK_LE(dpb_.CountRefPics(),
            std::max<int>(parser_.GetSPS(curr_sps_id_)->max_num_ref_frames,
                          1));
        if (dpb_.CountRefPics() ==
            std::max<int>(parser_.GetSPS(curr_sps_id_)->max_num_ref_frames,
                          1)) {
          // Max number of reference pics reached,
          // need to remove one of the short term ones.
          // Find smallest frame_num_wrap short reference picture and mark
          // it as unused.
          H264Picture* to_unmark = dpb_.GetLowestFrameNumWrapShortRefPic();
          if (to_unmark == NULL) {
            DVLOG(1) << "Couldn't find a short ref picture to unmark";
            return;
          }
          to_unmark->ref = false;
        }
      } else {
        // Shouldn't get here.
        DVLOG(1) << "Interlaced video not supported.";
        report_error_to_uma_cb_.Run(INTERLACED_STREAM);
      }
    } else {
      // Stream has instructions how to discard pictures from DPB and how
      // to mark/unmark existing reference pictures. Do it.
      // Spec 8.2.5.4.
      if (curr_pic_->field == H264Picture::FIELD_NONE) {
        HandleMemoryManagementOps();
      } else {
        // Shouldn't get here.
        DVLOG(1) << "Interlaced video not supported.";
        report_error_to_uma_cb_.Run(INTERLACED_STREAM);
      }
    }
  }
}

bool VaapiH264Decoder::FinishPicture() {
  DCHECK(curr_pic_.get());

  // Finish processing previous picture.
  // Start by storing previous reference picture data for later use,
  // if picture being finished is a reference picture.
  if (curr_pic_->ref) {
    ReferencePictureMarking();
    prev_ref_has_memmgmnt5_ = curr_pic_->mem_mgmt_5;
    prev_ref_top_field_order_cnt_ = curr_pic_->top_field_order_cnt;
    prev_ref_pic_order_cnt_msb_ = curr_pic_->pic_order_cnt_msb;
    prev_ref_pic_order_cnt_lsb_ = curr_pic_->pic_order_cnt_lsb;
    prev_ref_field_ = curr_pic_->field;
  }
  prev_has_memmgmnt5_ = curr_pic_->mem_mgmt_5;
  prev_frame_num_offset_ = curr_pic_->frame_num_offset;

  // Remove unused (for reference or later output) pictures from DPB, marking
  // them as such.
  for (H264DPB::Pictures::iterator it = dpb_.begin(); it != dpb_.end(); ++it) {
    if ((*it)->outputted && !(*it)->ref)
      UnassignSurfaceFromPoC((*it)->pic_order_cnt);
  }
  dpb_.DeleteUnused();

  DVLOG(4) << "Finishing picture, entries in DPB: " << dpb_.size();

  // Whatever happens below, curr_pic_ will stop managing the pointer to the
  // picture after this function returns. The ownership will either be
  // transferred to DPB, if the image is still needed (for output and/or
  // reference), or the memory will be released if we manage to output it here
  // without having to store it for future reference.
  scoped_ptr<H264Picture> pic(curr_pic_.release());

  // Get all pictures that haven't been outputted yet.
  H264Picture::PtrVector not_outputted;
  // TODO(posciak): pass as pointer, not reference (violates coding style).
  dpb_.GetNotOutputtedPicsAppending(not_outputted);
  // Include the one we've just decoded.
  not_outputted.push_back(pic.get());

  // Sort in output order.
  std::sort(not_outputted.begin(), not_outputted.end(), POCAscCompare());

  // Try to output as many pictures as we can. A picture can be output,
  // if the number of decoded and not yet outputted pictures that would remain
  // in DPB afterwards would at least be equal to max_num_reorder_frames.
  // If the outputted picture is not a reference picture, it doesn't have
  // to remain in the DPB and can be removed.
  H264Picture::PtrVector::iterator output_candidate = not_outputted.begin();
  size_t num_remaining = not_outputted.size();
  while (num_remaining > max_num_reorder_frames_) {
    int poc = (*output_candidate)->pic_order_cnt;
    DCHECK_GE(poc, last_output_poc_);
    if (!OutputPic(*output_candidate))
      return false;

    if (!(*output_candidate)->ref) {
      // Current picture hasn't been inserted into DPB yet, so don't remove it
      // if we managed to output it immediately.
      if (*output_candidate != pic)
        dpb_.DeleteByPOC(poc);
      // Mark as unused.
      UnassignSurfaceFromPoC(poc);
    }

    ++output_candidate;
    --num_remaining;
  }

  // If we haven't managed to output the picture that we just decoded, or if
  // it's a reference picture, we have to store it in DPB.
  if (!pic->outputted || pic->ref) {
    if (dpb_.IsFull()) {
      // If we haven't managed to output anything to free up space in DPB
      // to store this picture, it's an error in the stream.
      DVLOG(1) << "Could not free up space in DPB!";
      return false;
    }

    dpb_.StorePic(pic.release());
  }

  return true;
}

static int LevelToMaxDpbMbs(int level) {
  // See table A-1 in spec.
  switch (level) {
    case 10: return 396;
    case 11: return 900;
    case 12: //  fallthrough
    case 13: //  fallthrough
    case 20: return 2376;
    case 21: return 4752;
    case 22: //  fallthrough
    case 30: return 8100;
    case 31: return 18000;
    case 32: return 20480;
    case 40: //  fallthrough
    case 41: return 32768;
    case 42: return 34816;
    case 50: return 110400;
    case 51: //  fallthrough
    case 52: return 184320;
    default:
      DVLOG(1) << "Invalid codec level (" << level << ")";
      return 0;
  }
}

bool VaapiH264Decoder::UpdateMaxNumReorderFrames(const media::H264SPS* sps) {
  if (sps->vui_parameters_present_flag && sps->bitstream_restriction_flag) {
    max_num_reorder_frames_ =
        base::checked_cast<size_t>(sps->max_num_reorder_frames);
    if (max_num_reorder_frames_ > dpb_.max_num_pics()) {
      DVLOG(1)
          << "max_num_reorder_frames present, but larger than MaxDpbFrames ("
          << max_num_reorder_frames_ << " > " << dpb_.max_num_pics() << ")";
      max_num_reorder_frames_ = 0;
      return false;
    }
    return true;
  }

  // max_num_reorder_frames not present, infer from profile/constraints
  // (see VUI semantics in spec).
  if (sps->constraint_set3_flag) {
    switch (sps->profile_idc) {
      case 44:
      case 86:
      case 100:
      case 110:
      case 122:
      case 244:
        max_num_reorder_frames_ = 0;
        break;
      default:
        max_num_reorder_frames_ = dpb_.max_num_pics();
        break;
    }
  } else {
    max_num_reorder_frames_ = dpb_.max_num_pics();
  }

  return true;
}

bool VaapiH264Decoder::ProcessSPS(int sps_id, bool* need_new_buffers) {
  const media::H264SPS* sps = parser_.GetSPS(sps_id);
  DCHECK(sps);
  DVLOG(4) << "Processing SPS";

  *need_new_buffers = false;

  if (sps->frame_mbs_only_flag == 0) {
    DVLOG(1) << "frame_mbs_only_flag != 1 not supported";
    report_error_to_uma_cb_.Run(FRAME_MBS_ONLY_FLAG_NOT_ONE);
    return false;
  }

  if (sps->gaps_in_frame_num_value_allowed_flag) {
    DVLOG(1) << "Gaps in frame numbers not supported";
    report_error_to_uma_cb_.Run(GAPS_IN_FRAME_NUM);
    return false;
  }

  curr_sps_id_ = sps->seq_parameter_set_id;

  // Calculate picture height/width in macroblocks and pixels
  // (spec 7.4.2.1.1, 7.4.3).
  int width_mb = sps->pic_width_in_mbs_minus1 + 1;
  int height_mb = (2 - sps->frame_mbs_only_flag) *
      (sps->pic_height_in_map_units_minus1 + 1);

  gfx::Size new_pic_size(16 * width_mb, 16 * height_mb);
  if (new_pic_size.IsEmpty()) {
    DVLOG(1) << "Invalid picture size: " << new_pic_size.ToString();
    return false;
  }

  if (!pic_size_.IsEmpty() && new_pic_size == pic_size_) {
    // Already have surfaces and this SPS keeps the same resolution,
    // no need to request a new set.
    return true;
  }

  pic_size_ = new_pic_size;
  DVLOG(1) << "New picture size: " << pic_size_.ToString();

  max_pic_order_cnt_lsb_ = 1 << (sps->log2_max_pic_order_cnt_lsb_minus4 + 4);
  max_frame_num_ = 1 << (sps->log2_max_frame_num_minus4 + 4);

  int level = sps->level_idc;
  int max_dpb_mbs = LevelToMaxDpbMbs(level);
  if (max_dpb_mbs == 0)
    return false;

  size_t max_dpb_size = std::min(max_dpb_mbs / (width_mb * height_mb),
                                 static_cast<int>(H264DPB::kDPBMaxSize));
  DVLOG(1) << "Codec level: " << level << ", DPB size: " << max_dpb_size;
  if (max_dpb_size == 0) {
    DVLOG(1) << "Invalid DPB Size";
    return false;
  }

  dpb_.set_max_num_pics(max_dpb_size);

  if (!UpdateMaxNumReorderFrames(sps))
    return false;
  DVLOG(1) << "max_num_reorder_frames: " << max_num_reorder_frames_;

  *need_new_buffers = true;
  return true;
}

bool VaapiH264Decoder::ProcessPPS(int pps_id) {
  const media::H264PPS* pps = parser_.GetPPS(pps_id);
  DCHECK(pps);

  curr_pps_id_ = pps->pic_parameter_set_id;

  return true;
}

bool VaapiH264Decoder::FinishPrevFrameIfPresent() {
  // If we already have a frame waiting to be decoded, decode it and finish.
  if (curr_pic_ != NULL) {
    if (!DecodePicture())
      return false;
    return FinishPicture();
  }

  return true;
}

bool VaapiH264Decoder::ProcessSlice(media::H264SliceHeader* slice_hdr) {
  prev_frame_num_ = frame_num_;
  frame_num_ = slice_hdr->frame_num;

  if (prev_frame_num_ > 0 && prev_frame_num_ < frame_num_ - 1) {
    DVLOG(1) << "Gap in frame_num!";
    report_error_to_uma_cb_.Run(GAPS_IN_FRAME_NUM);
    return false;
  }

  if (slice_hdr->field_pic_flag == 0)
    max_pic_num_ = max_frame_num_;
  else
    max_pic_num_ = 2 * max_frame_num_;

  // TODO posciak: switch to new picture detection per 7.4.1.2.4.
  if (curr_pic_ != NULL && slice_hdr->first_mb_in_slice != 0) {
    // This is just some more slice data of the current picture, so
    // just queue it and return.
    QueueSlice(slice_hdr);
    return true;
  } else {
    // A new frame, so first finish the previous one before processing it...
    if (!FinishPrevFrameIfPresent())
      return false;

    // and then start a new one.
    return StartNewFrame(slice_hdr);
  }
}

#define SET_ERROR_AND_RETURN()             \
  do {                                     \
    DVLOG(1) << "Error during decode";     \
    state_ = kError;                       \
    return VaapiH264Decoder::kDecodeError; \
  } while (0)

void VaapiH264Decoder::SetStream(const uint8* ptr,
                                 size_t size,
                                 int32 input_id) {
  DCHECK(ptr);
  DCHECK(size);

  // Got new input stream data from the client.
  DVLOG(4) << "New input stream id: " << input_id << " at: " << (void*) ptr
           << " size:  " << size;
  parser_.SetStream(ptr, size);
  curr_input_id_ = input_id;
}

VaapiH264Decoder::DecResult VaapiH264Decoder::Decode() {
  media::H264Parser::Result par_res;
  media::H264NALU nalu;
  DCHECK_NE(state_, kError);

  while (1) {
    // If we've already decoded some of the stream (after reset, i.e. we are
    // not in kNeedStreamMetadata state), we may be able to go back into
    // decoding state not only starting at/resuming from an SPS, but also from
    // other resume points, such as IDRs. In the latter case we need an output
    // surface, because we will end up decoding that IDR in the process.
    // Otherwise we just look for an SPS and don't produce any output frames.
    if (state_ != kNeedStreamMetadata && available_va_surfaces_.empty()) {
      DVLOG(4) << "No output surfaces available";
      return kRanOutOfSurfaces;
    }

    par_res = parser_.AdvanceToNextNALU(&nalu);
    if (par_res == media::H264Parser::kEOStream)
      return kRanOutOfStreamData;
    else if (par_res != media::H264Parser::kOk)
      SET_ERROR_AND_RETURN();

    DVLOG(4) << "NALU found: " << static_cast<int>(nalu.nal_unit_type);

    switch (nalu.nal_unit_type) {
      case media::H264NALU::kNonIDRSlice:
        // We can't resume from a non-IDR slice.
        if (state_ != kDecoding)
          break;
        // else fallthrough
      case media::H264NALU::kIDRSlice: {
        // TODO(posciak): the IDR may require an SPS that we don't have
        // available. For now we'd fail if that happens, but ideally we'd like
        // to keep going until the next SPS in the stream.
        if (state_ == kNeedStreamMetadata) {
          // We need an SPS, skip this IDR and keep looking.
          break;
        }

        // If after reset, we should be able to recover from an IDR.
        media::H264SliceHeader slice_hdr;

        par_res = parser_.ParseSliceHeader(nalu, &slice_hdr);
        if (par_res != media::H264Parser::kOk)
          SET_ERROR_AND_RETURN();

        if (!ProcessSlice(&slice_hdr))
          SET_ERROR_AND_RETURN();

        state_ = kDecoding;
        break;
      }

      case media::H264NALU::kSPS: {
        int sps_id;

        if (!FinishPrevFrameIfPresent())
          SET_ERROR_AND_RETURN();

        par_res = parser_.ParseSPS(&sps_id);
        if (par_res != media::H264Parser::kOk)
          SET_ERROR_AND_RETURN();

        bool need_new_buffers = false;
        if (!ProcessSPS(sps_id, &need_new_buffers))
          SET_ERROR_AND_RETURN();

        state_ = kDecoding;

        if (need_new_buffers) {
          if (!Flush())
            return kDecodeError;

          available_va_surfaces_.clear();
          return kAllocateNewSurfaces;
        }
        break;
      }

      case media::H264NALU::kPPS: {
        if (state_ != kDecoding)
          break;

        int pps_id;

        if (!FinishPrevFrameIfPresent())
          SET_ERROR_AND_RETURN();

        par_res = parser_.ParsePPS(&pps_id);
        if (par_res != media::H264Parser::kOk)
          SET_ERROR_AND_RETURN();

        if (!ProcessPPS(pps_id))
          SET_ERROR_AND_RETURN();
        break;
      }

      default:
        DVLOG(4) << "Skipping NALU type: " << nalu.nal_unit_type;
        break;
    }
  }
}

size_t VaapiH264Decoder::GetRequiredNumOfPictures() {
  return dpb_.max_num_pics() + kPicsInPipeline;
}

}  // namespace content
