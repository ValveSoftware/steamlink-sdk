// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi_video_encode_accelerator.h"

#include <string.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/bind_to_current_loop.h"
#include "media/gpu/h264_dpb.h"
#include "media/gpu/shared_memory_region.h"
#include "third_party/libva/va/va_enc_h264.h"

#define DVLOGF(level) DVLOG(level) << __FUNCTION__ << "(): "

#define NOTIFY_ERROR(error, msg)                          \
  do {                                                    \
    SetState(kError);                                     \
    LOG(ERROR) << msg;                                    \
    LOG(ERROR) << "Calling NotifyError(" << error << ")"; \
    NotifyError(error);                                   \
  } while (0)

namespace media {

namespace {
// Need 2 surfaces for each frame: one for input data and one for
// reconstructed picture, which is later used for reference.
const size_t kMinSurfacesToEncode = 2;

// Subjectively chosen.
const size_t kNumInputBuffers = 4;
const size_t kMaxNumReferenceFrames = 4;

// We need up to kMaxNumReferenceFrames surfaces for reference, plus one
// for input and one for encode (which will be added to the set of reference
// frames for subsequent frames). Actual execution of HW encode is done
// in parallel, and we want to process more frames in the meantime.
// To have kNumInputBuffers in flight, we need a full set of reference +
// encode surfaces (i.e. kMaxNumReferenceFrames + kMinSurfacesToEncode), and
// (kNumInputBuffers - 1) of kMinSurfacesToEncode for the remaining frames
// in flight.
const size_t kNumSurfaces = kMaxNumReferenceFrames + kMinSurfacesToEncode +
                            kMinSurfacesToEncode * (kNumInputBuffers - 1);

// An IDR every 2048 frames, an I frame every 256 and no B frames.
// We choose IDR period to equal MaxFrameNum so it must be a power of 2.
const int kIDRPeriod = 2048;
const int kIPeriod = 256;
const int kIPPeriod = 1;

const int kDefaultFramerate = 30;

// HRD parameters (ch. E.2.2 in spec).
const int kBitRateScale = 0;  // bit_rate_scale for SPS HRD parameters.
const int kCPBSizeScale = 0;  // cpb_size_scale for SPS HRD parameters.

const int kDefaultQP = 26;
// All Intel codecs can do at least 4.1.
const int kDefaultLevelIDC = 41;
const int kChromaFormatIDC = 1;  // 4:2:0

// Arbitrarily chosen bitrate window size for rate control, in ms.
const int kCPBWindowSizeMs = 1500;

// UMA errors that the VaapiVideoEncodeAccelerator class reports.
enum VAVEAEncoderFailure {
  VAAPI_ERROR = 0,
  VAVEA_ENCODER_FAILURES_MAX,
};
}

// Round |value| up to |alignment|, which must be a power of 2.
static inline size_t RoundUpToPowerOf2(size_t value, size_t alignment) {
  // Check that |alignment| is a power of 2.
  DCHECK((alignment + (alignment - 1)) == (alignment | (alignment - 1)));
  return ((value + (alignment - 1)) & ~(alignment - 1));
}

static void ReportToUMA(VAVEAEncoderFailure failure) {
  UMA_HISTOGRAM_ENUMERATION("Media.VAVEA.EncoderFailure", failure,
                            VAVEA_ENCODER_FAILURES_MAX + 1);
}

struct VaapiVideoEncodeAccelerator::InputFrameRef {
  InputFrameRef(const scoped_refptr<VideoFrame>& frame, bool force_keyframe)
      : frame(frame), force_keyframe(force_keyframe) {}
  const scoped_refptr<VideoFrame> frame;
  const bool force_keyframe;
};

struct VaapiVideoEncodeAccelerator::BitstreamBufferRef {
  BitstreamBufferRef(int32_t id, std::unique_ptr<SharedMemoryRegion> shm)
      : id(id), shm(std::move(shm)) {}
  const int32_t id;
  const std::unique_ptr<SharedMemoryRegion> shm;
};

VideoEncodeAccelerator::SupportedProfiles
VaapiVideoEncodeAccelerator::GetSupportedProfiles() {
  return VaapiWrapper::GetSupportedEncodeProfiles();
}

static unsigned int Log2OfPowerOf2(unsigned int x) {
  CHECK_GT(x, 0u);
  DCHECK_EQ(x & (x - 1), 0u);

  int log = 0;
  while (x > 1) {
    x >>= 1;
    ++log;
  }
  return log;
}

VaapiVideoEncodeAccelerator::VaapiVideoEncodeAccelerator()
    : profile_(VIDEO_CODEC_PROFILE_UNKNOWN),
      mb_width_(0),
      mb_height_(0),
      output_buffer_byte_size_(0),
      state_(kUninitialized),
      frame_num_(0),
      idr_pic_id_(0),
      bitrate_(0),
      framerate_(0),
      cpb_size_(0),
      encoding_parameters_changed_(false),
      encoder_thread_("VAVEAEncoderThread"),
      child_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_this_ptr_factory_(this) {
  DVLOGF(4);
  weak_this_ = weak_this_ptr_factory_.GetWeakPtr();

  max_ref_idx_l0_size_ = kMaxNumReferenceFrames;
  qp_ = kDefaultQP;
  idr_period_ = kIDRPeriod;
  i_period_ = kIPeriod;
  ip_period_ = kIPPeriod;
}

VaapiVideoEncodeAccelerator::~VaapiVideoEncodeAccelerator() {
  DVLOGF(4);
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  DCHECK(!encoder_thread_.IsRunning());
}

bool VaapiVideoEncodeAccelerator::Initialize(
    VideoPixelFormat format,
    const gfx::Size& input_visible_size,
    VideoCodecProfile output_profile,
    uint32_t initial_bitrate,
    Client* client) {
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  DCHECK(!encoder_thread_.IsRunning());
  DCHECK_EQ(state_, kUninitialized);

  DVLOGF(1) << "Initializing VAVEA, input_format: "
            << VideoPixelFormatToString(format)
            << ", input_visible_size: " << input_visible_size.ToString()
            << ", output_profile: " << output_profile
            << ", initial_bitrate: " << initial_bitrate;

  client_ptr_factory_.reset(new base::WeakPtrFactory<Client>(client));
  client_ = client_ptr_factory_->GetWeakPtr();

  const SupportedProfiles& profiles = GetSupportedProfiles();
  auto profile = find_if(profiles.begin(), profiles.end(),
                         [output_profile](const SupportedProfile& profile) {
                           return profile.profile == output_profile;
                         });
  if (profile == profiles.end()) {
    DVLOGF(1) << "Unsupported output profile " << output_profile;
    return false;
  }
  if (input_visible_size.width() > profile->max_resolution.width() ||
      input_visible_size.height() > profile->max_resolution.height()) {
    DVLOGF(1) << "Input size too big: " << input_visible_size.ToString()
              << ", max supported size: " << profile->max_resolution.ToString();
    return false;
  }

  if (format != PIXEL_FORMAT_I420) {
    DVLOGF(1) << "Unsupported input format: "
              << VideoPixelFormatToString(format);
    return false;
  }

  profile_ = output_profile;
  visible_size_ = input_visible_size;
  // 4:2:0 format has to be 2-aligned.
  DCHECK_EQ(visible_size_.width() % 2, 0);
  DCHECK_EQ(visible_size_.height() % 2, 0);
  coded_size_ = gfx::Size(RoundUpToPowerOf2(visible_size_.width(), 16),
                          RoundUpToPowerOf2(visible_size_.height(), 16));
  mb_width_ = coded_size_.width() / 16;
  mb_height_ = coded_size_.height() / 16;
  output_buffer_byte_size_ = coded_size_.GetArea();

  UpdateRates(initial_bitrate, kDefaultFramerate);

  vaapi_wrapper_ =
      VaapiWrapper::CreateForVideoCodec(VaapiWrapper::kEncode, output_profile,
                                        base::Bind(&ReportToUMA, VAAPI_ERROR));
  if (!vaapi_wrapper_.get()) {
    DVLOGF(1) << "Failed initializing VAAPI for profile " << output_profile;
    return false;
  }

  if (!encoder_thread_.Start()) {
    LOG(ERROR) << "Failed to start encoder thread";
    return false;
  }
  encoder_thread_task_runner_ = encoder_thread_.task_runner();

  // Finish the remaining initialization on the encoder thread.
  encoder_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VaapiVideoEncodeAccelerator::InitializeTask,
                            base::Unretained(this)));

  return true;
}

void VaapiVideoEncodeAccelerator::InitializeTask() {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kUninitialized);
  DVLOGF(4);

  va_surface_release_cb_ = BindToCurrentLoop(
      base::Bind(&VaapiVideoEncodeAccelerator::RecycleVASurfaceID,
                 base::Unretained(this)));

  if (!vaapi_wrapper_->CreateSurfaces(VA_RT_FORMAT_YUV420, coded_size_,
                                      kNumSurfaces,
                                      &available_va_surface_ids_)) {
    NOTIFY_ERROR(kPlatformFailureError, "Failed creating VASurfaces");
    return;
  }

  UpdateSPS();
  GeneratePackedSPS();

  UpdatePPS();
  GeneratePackedPPS();

  child_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&Client::RequireBitstreamBuffers, client_, kNumInputBuffers,
                 coded_size_, output_buffer_byte_size_));

  SetState(kEncoding);
}

void VaapiVideoEncodeAccelerator::RecycleVASurfaceID(
    VASurfaceID va_surface_id) {
  DVLOGF(4) << "va_surface_id: " << va_surface_id;
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  available_va_surface_ids_.push_back(va_surface_id);
  EncodeFrameTask();
}

void VaapiVideoEncodeAccelerator::BeginFrame(bool force_keyframe) {
  current_pic_ = new H264Picture();

  // If the current picture is an IDR picture, frame_num shall be equal to 0.
  if (force_keyframe)
    frame_num_ = 0;

  current_pic_->frame_num = frame_num_++;
  frame_num_ %= idr_period_;

  if (current_pic_->frame_num == 0) {
    current_pic_->idr = true;
    // H264 spec mandates idr_pic_id to differ between two consecutive IDRs.
    idr_pic_id_ ^= 1;
    ref_pic_list0_.clear();
  }

  if (current_pic_->frame_num % i_period_ == 0)
    current_pic_->type = H264SliceHeader::kISlice;
  else
    current_pic_->type = H264SliceHeader::kPSlice;

  if (current_pic_->type != H264SliceHeader::kBSlice)
    current_pic_->ref = true;

  current_pic_->pic_order_cnt = current_pic_->frame_num * 2;
  current_pic_->top_field_order_cnt = current_pic_->pic_order_cnt;
  current_pic_->pic_order_cnt_lsb = current_pic_->pic_order_cnt;

  current_encode_job_->keyframe = current_pic_->idr;

  DVLOGF(4) << "Starting a new frame, type: " << current_pic_->type
            << (force_keyframe ? " (forced keyframe)" : "")
            << " frame_num: " << current_pic_->frame_num
            << " POC: " << current_pic_->pic_order_cnt;
}

void VaapiVideoEncodeAccelerator::EndFrame() {
  DCHECK(current_pic_);
  // Store the picture on the list of reference pictures and keep the list
  // below maximum size, dropping oldest references.
  if (current_pic_->ref)
    ref_pic_list0_.push_front(current_encode_job_->recon_surface);
  size_t max_num_ref_frames =
      base::checked_cast<size_t>(current_sps_.max_num_ref_frames);
  while (ref_pic_list0_.size() > max_num_ref_frames)
    ref_pic_list0_.pop_back();

  submitted_encode_jobs_.push(make_linked_ptr(current_encode_job_.release()));
}

static void InitVAPicture(VAPictureH264* va_pic) {
  memset(va_pic, 0, sizeof(*va_pic));
  va_pic->picture_id = VA_INVALID_ID;
  va_pic->flags = VA_PICTURE_H264_INVALID;
}

bool VaapiVideoEncodeAccelerator::SubmitFrameParameters() {
  DCHECK(current_pic_);
  VAEncSequenceParameterBufferH264 seq_param;
  memset(&seq_param, 0, sizeof(seq_param));

#define SPS_TO_SP(a) seq_param.a = current_sps_.a;
  SPS_TO_SP(seq_parameter_set_id);
  SPS_TO_SP(level_idc);

  seq_param.intra_period = i_period_;
  seq_param.intra_idr_period = idr_period_;
  seq_param.ip_period = ip_period_;
  seq_param.bits_per_second = bitrate_;

  SPS_TO_SP(max_num_ref_frames);
  seq_param.picture_width_in_mbs = mb_width_;
  seq_param.picture_height_in_mbs = mb_height_;

#define SPS_TO_SP_FS(a) seq_param.seq_fields.bits.a = current_sps_.a;
  SPS_TO_SP_FS(chroma_format_idc);
  SPS_TO_SP_FS(frame_mbs_only_flag);
  SPS_TO_SP_FS(log2_max_frame_num_minus4);
  SPS_TO_SP_FS(pic_order_cnt_type);
  SPS_TO_SP_FS(log2_max_pic_order_cnt_lsb_minus4);
#undef SPS_TO_SP_FS

  SPS_TO_SP(bit_depth_luma_minus8);
  SPS_TO_SP(bit_depth_chroma_minus8);

  SPS_TO_SP(frame_cropping_flag);
  if (current_sps_.frame_cropping_flag) {
    SPS_TO_SP(frame_crop_left_offset);
    SPS_TO_SP(frame_crop_right_offset);
    SPS_TO_SP(frame_crop_top_offset);
    SPS_TO_SP(frame_crop_bottom_offset);
  }

  SPS_TO_SP(vui_parameters_present_flag);
#define SPS_TO_SP_VF(a) seq_param.vui_fields.bits.a = current_sps_.a;
  SPS_TO_SP_VF(timing_info_present_flag);
#undef SPS_TO_SP_VF
  SPS_TO_SP(num_units_in_tick);
  SPS_TO_SP(time_scale);
#undef SPS_TO_SP

  if (!vaapi_wrapper_->SubmitBuffer(VAEncSequenceParameterBufferType,
                                    sizeof(seq_param), &seq_param))
    return false;

  VAEncPictureParameterBufferH264 pic_param;
  memset(&pic_param, 0, sizeof(pic_param));

  pic_param.CurrPic.picture_id = current_encode_job_->recon_surface->id();
  pic_param.CurrPic.TopFieldOrderCnt = current_pic_->top_field_order_cnt;
  pic_param.CurrPic.BottomFieldOrderCnt = current_pic_->bottom_field_order_cnt;
  pic_param.CurrPic.flags = 0;

  for (size_t i = 0; i < arraysize(pic_param.ReferenceFrames); ++i)
    InitVAPicture(&pic_param.ReferenceFrames[i]);

  DCHECK_LE(ref_pic_list0_.size(), arraysize(pic_param.ReferenceFrames));
  RefPicList::const_iterator iter = ref_pic_list0_.begin();
  for (size_t i = 0;
       i < arraysize(pic_param.ReferenceFrames) && iter != ref_pic_list0_.end();
       ++iter, ++i) {
    pic_param.ReferenceFrames[i].picture_id = (*iter)->id();
    pic_param.ReferenceFrames[i].flags = 0;
  }

  pic_param.coded_buf = current_encode_job_->coded_buffer;
  pic_param.pic_parameter_set_id = current_pps_.pic_parameter_set_id;
  pic_param.seq_parameter_set_id = current_pps_.seq_parameter_set_id;
  pic_param.frame_num = current_pic_->frame_num;
  pic_param.pic_init_qp = qp_;
  pic_param.num_ref_idx_l0_active_minus1 = max_ref_idx_l0_size_ - 1;
  pic_param.pic_fields.bits.idr_pic_flag = current_pic_->idr;
  pic_param.pic_fields.bits.reference_pic_flag = current_pic_->ref;
#define PPS_TO_PP_PF(a) pic_param.pic_fields.bits.a = current_pps_.a;
  PPS_TO_PP_PF(entropy_coding_mode_flag);
  PPS_TO_PP_PF(transform_8x8_mode_flag);
  PPS_TO_PP_PF(deblocking_filter_control_present_flag);
#undef PPS_TO_PP_PF

  if (!vaapi_wrapper_->SubmitBuffer(VAEncPictureParameterBufferType,
                                    sizeof(pic_param), &pic_param))
    return false;

  VAEncSliceParameterBufferH264 slice_param;
  memset(&slice_param, 0, sizeof(slice_param));

  slice_param.num_macroblocks = mb_width_ * mb_height_;
  slice_param.macroblock_info = VA_INVALID_ID;
  slice_param.slice_type = current_pic_->type;
  slice_param.pic_parameter_set_id = current_pps_.pic_parameter_set_id;
  slice_param.idr_pic_id = idr_pic_id_;
  slice_param.pic_order_cnt_lsb = current_pic_->pic_order_cnt_lsb;
  slice_param.num_ref_idx_active_override_flag = true;

  for (size_t i = 0; i < arraysize(slice_param.RefPicList0); ++i)
    InitVAPicture(&slice_param.RefPicList0[i]);

  for (size_t i = 0; i < arraysize(slice_param.RefPicList1); ++i)
    InitVAPicture(&slice_param.RefPicList1[i]);

  DCHECK_LE(ref_pic_list0_.size(), arraysize(slice_param.RefPicList0));
  iter = ref_pic_list0_.begin();
  for (size_t i = 0;
       i < arraysize(slice_param.RefPicList0) && iter != ref_pic_list0_.end();
       ++iter, ++i) {
    InitVAPicture(&slice_param.RefPicList0[i]);
    slice_param.RefPicList0[i].picture_id = (*iter)->id();
    slice_param.RefPicList0[i].flags = 0;
  }

  if (!vaapi_wrapper_->SubmitBuffer(VAEncSliceParameterBufferType,
                                    sizeof(slice_param), &slice_param))
    return false;

  VAEncMiscParameterRateControl rate_control_param;
  memset(&rate_control_param, 0, sizeof(rate_control_param));
  rate_control_param.bits_per_second = bitrate_;
  rate_control_param.target_percentage = 90;
  rate_control_param.window_size = kCPBWindowSizeMs;
  rate_control_param.initial_qp = qp_;
  rate_control_param.rc_flags.bits.disable_frame_skip = true;

  if (!vaapi_wrapper_->SubmitVAEncMiscParamBuffer(
          VAEncMiscParameterTypeRateControl, sizeof(rate_control_param),
          &rate_control_param))
    return false;

  VAEncMiscParameterFrameRate framerate_param;
  memset(&framerate_param, 0, sizeof(framerate_param));
  framerate_param.framerate = framerate_;
  if (!vaapi_wrapper_->SubmitVAEncMiscParamBuffer(
          VAEncMiscParameterTypeFrameRate, sizeof(framerate_param),
          &framerate_param))
    return false;

  VAEncMiscParameterHRD hrd_param;
  memset(&hrd_param, 0, sizeof(hrd_param));
  hrd_param.buffer_size = cpb_size_;
  hrd_param.initial_buffer_fullness = cpb_size_ / 2;
  if (!vaapi_wrapper_->SubmitVAEncMiscParamBuffer(
          VAEncMiscParameterTypeHRD, sizeof(hrd_param), &hrd_param))
    return false;

  return true;
}

bool VaapiVideoEncodeAccelerator::SubmitHeadersIfNeeded() {
  DCHECK(current_pic_);
  if (current_pic_->type != H264SliceHeader::kISlice)
    return true;

  // Submit PPS.
  VAEncPackedHeaderParameterBuffer par_buffer;
  memset(&par_buffer, 0, sizeof(par_buffer));
  par_buffer.type = VAEncPackedHeaderSequence;
  par_buffer.bit_length = packed_sps_.BytesInBuffer() * 8;

  if (!vaapi_wrapper_->SubmitBuffer(VAEncPackedHeaderParameterBufferType,
                                    sizeof(par_buffer), &par_buffer))
    return false;

  if (!vaapi_wrapper_->SubmitBuffer(VAEncPackedHeaderDataBufferType,
                                    packed_sps_.BytesInBuffer(),
                                    packed_sps_.data()))
    return false;

  // Submit PPS.
  memset(&par_buffer, 0, sizeof(par_buffer));
  par_buffer.type = VAEncPackedHeaderPicture;
  par_buffer.bit_length = packed_pps_.BytesInBuffer() * 8;

  if (!vaapi_wrapper_->SubmitBuffer(VAEncPackedHeaderParameterBufferType,
                                    sizeof(par_buffer), &par_buffer))
    return false;

  if (!vaapi_wrapper_->SubmitBuffer(VAEncPackedHeaderDataBufferType,
                                    packed_pps_.BytesInBuffer(),
                                    packed_pps_.data()))
    return false;

  return true;
}

bool VaapiVideoEncodeAccelerator::ExecuteEncode() {
  DCHECK(current_pic_);
  DVLOGF(3) << "Encoding frame_num: " << current_pic_->frame_num;
  return vaapi_wrapper_->ExecuteAndDestroyPendingBuffers(
      current_encode_job_->input_surface->id());
}

bool VaapiVideoEncodeAccelerator::UploadFrame(
    const scoped_refptr<VideoFrame>& frame) {
  return vaapi_wrapper_->UploadVideoFrameToSurface(
      frame, current_encode_job_->input_surface->id());
}

void VaapiVideoEncodeAccelerator::TryToReturnBitstreamBuffer() {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  if (state_ != kEncoding)
    return;

  if (submitted_encode_jobs_.empty() || available_bitstream_buffers_.empty())
    return;

  linked_ptr<BitstreamBufferRef> buffer = available_bitstream_buffers_.front();
  available_bitstream_buffers_.pop();

  uint8_t* target_data = reinterpret_cast<uint8_t*>(buffer->shm->memory());

  linked_ptr<EncodeJob> encode_job = submitted_encode_jobs_.front();
  submitted_encode_jobs_.pop();

  size_t data_size = 0;
  if (!vaapi_wrapper_->DownloadAndDestroyCodedBuffer(
          encode_job->coded_buffer, encode_job->input_surface->id(),
          target_data, buffer->shm->size(), &data_size)) {
    NOTIFY_ERROR(kPlatformFailureError, "Failed downloading coded buffer");
    return;
  }

  DVLOGF(3) << "Returning bitstream buffer "
            << (encode_job->keyframe ? "(keyframe)" : "")
            << " id: " << buffer->id << " size: " << data_size;

  child_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&Client::BitstreamBufferReady, client_, buffer->id, data_size,
                 encode_job->keyframe, encode_job->timestamp));
}

void VaapiVideoEncodeAccelerator::Encode(const scoped_refptr<VideoFrame>& frame,
                                         bool force_keyframe) {
  DVLOGF(3) << "Frame timestamp: " << frame->timestamp().InMilliseconds()
            << " force_keyframe: " << force_keyframe;
  DCHECK(child_task_runner_->BelongsToCurrentThread());

  encoder_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VaapiVideoEncodeAccelerator::EncodeTask,
                            base::Unretained(this), frame, force_keyframe));
}

bool VaapiVideoEncodeAccelerator::PrepareNextJob(base::TimeDelta timestamp) {
  if (available_va_surface_ids_.size() < kMinSurfacesToEncode)
    return false;

  DCHECK(!current_encode_job_);
  current_encode_job_.reset(new EncodeJob());

  if (!vaapi_wrapper_->CreateCodedBuffer(output_buffer_byte_size_,
                                         &current_encode_job_->coded_buffer)) {
    NOTIFY_ERROR(kPlatformFailureError, "Failed creating coded buffer");
    return false;
  }

  current_encode_job_->timestamp = timestamp;

  current_encode_job_->input_surface = new VASurface(
      available_va_surface_ids_.back(), coded_size_,
      vaapi_wrapper_->va_surface_format(), va_surface_release_cb_);
  available_va_surface_ids_.pop_back();

  current_encode_job_->recon_surface = new VASurface(
      available_va_surface_ids_.back(), coded_size_,
      vaapi_wrapper_->va_surface_format(), va_surface_release_cb_);
  available_va_surface_ids_.pop_back();

  // Reference surfaces are needed until the job is done, but they get
  // removed from ref_pic_list0_ when it's full at the end of job submission.
  // Keep refs to them along with the job and only release after sync.
  current_encode_job_->reference_surfaces = ref_pic_list0_;

  return true;
}

void VaapiVideoEncodeAccelerator::EncodeTask(
    const scoped_refptr<VideoFrame>& frame,
    bool force_keyframe) {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());
  DCHECK_NE(state_, kUninitialized);

  encoder_input_queue_.push(
      make_linked_ptr(new InputFrameRef(frame, force_keyframe)));
  EncodeFrameTask();
}

void VaapiVideoEncodeAccelerator::EncodeFrameTask() {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());

  if (state_ != kEncoding || encoder_input_queue_.empty())
    return;

  if (!PrepareNextJob(encoder_input_queue_.front()->frame->timestamp())) {
    DVLOGF(4) << "Not ready for next frame yet";
    return;
  }

  linked_ptr<InputFrameRef> frame_ref = encoder_input_queue_.front();
  encoder_input_queue_.pop();

  if (!UploadFrame(frame_ref->frame)) {
    NOTIFY_ERROR(kPlatformFailureError, "Failed uploading source frame to HW.");
    return;
  }

  BeginFrame(frame_ref->force_keyframe || encoding_parameters_changed_);
  encoding_parameters_changed_ = false;

  if (!SubmitFrameParameters()) {
    NOTIFY_ERROR(kPlatformFailureError, "Failed submitting frame parameters.");
    return;
  }

  if (!SubmitHeadersIfNeeded()) {
    NOTIFY_ERROR(kPlatformFailureError, "Failed submitting frame headers.");
    return;
  }

  if (!ExecuteEncode()) {
    NOTIFY_ERROR(kPlatformFailureError, "Failed submitting encode job to HW.");
    return;
  }

  EndFrame();
  TryToReturnBitstreamBuffer();
}

void VaapiVideoEncodeAccelerator::UseOutputBitstreamBuffer(
    const BitstreamBuffer& buffer) {
  DVLOGF(4) << "id: " << buffer.id();
  DCHECK(child_task_runner_->BelongsToCurrentThread());

  if (buffer.size() < output_buffer_byte_size_) {
    NOTIFY_ERROR(kInvalidArgumentError, "Provided bitstream buffer too small");
    return;
  }

  std::unique_ptr<SharedMemoryRegion> shm(
      new SharedMemoryRegion(buffer, false));
  if (!shm->Map()) {
    NOTIFY_ERROR(kPlatformFailureError, "Failed mapping shared memory.");
    return;
  }

  std::unique_ptr<BitstreamBufferRef> buffer_ref(
      new BitstreamBufferRef(buffer.id(), std::move(shm)));

  encoder_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VaapiVideoEncodeAccelerator::UseOutputBitstreamBufferTask,
                 base::Unretained(this), base::Passed(&buffer_ref)));
}

void VaapiVideoEncodeAccelerator::UseOutputBitstreamBufferTask(
    std::unique_ptr<BitstreamBufferRef> buffer_ref) {
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());
  DCHECK_NE(state_, kUninitialized);

  available_bitstream_buffers_.push(make_linked_ptr(buffer_ref.release()));
  TryToReturnBitstreamBuffer();
}

void VaapiVideoEncodeAccelerator::RequestEncodingParametersChange(
    uint32_t bitrate,
    uint32_t framerate) {
  DVLOGF(2) << "bitrate: " << bitrate << " framerate: " << framerate;
  DCHECK(child_task_runner_->BelongsToCurrentThread());

  encoder_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &VaapiVideoEncodeAccelerator::RequestEncodingParametersChangeTask,
          base::Unretained(this), bitrate, framerate));
}

void VaapiVideoEncodeAccelerator::UpdateRates(uint32_t bitrate,
                                              uint32_t framerate) {
  if (encoder_thread_.IsRunning())
    DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());
  DCHECK_NE(bitrate, 0u);
  DCHECK_NE(framerate, 0u);
  bitrate_ = bitrate;
  framerate_ = framerate;
  cpb_size_ = bitrate_ * kCPBWindowSizeMs / 1000;
}

void VaapiVideoEncodeAccelerator::RequestEncodingParametersChangeTask(
    uint32_t bitrate,
    uint32_t framerate) {
  DVLOGF(2) << "bitrate: " << bitrate << " framerate: " << framerate;
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());
  DCHECK_NE(state_, kUninitialized);

  // This is a workaround to zero being temporarily, as part of the initial
  // setup, provided by the webrtc video encode and a zero bitrate and
  // framerate not being accepted by VAAPI
  // TODO: This code is common with v4l2_video_encode_accelerator.cc, perhaps
  // it could be pulled up to RTCVideoEncoder
  if (bitrate < 1)
    bitrate = 1;
  if (framerate < 1)
    framerate = 1;

  if (bitrate_ == bitrate && framerate_ == framerate)
    return;

  UpdateRates(bitrate, framerate);

  UpdateSPS();
  GeneratePackedSPS();

  // Submit new parameters along with next frame that will be processed.
  encoding_parameters_changed_ = true;
}

void VaapiVideoEncodeAccelerator::Destroy() {
  DCHECK(child_task_runner_->BelongsToCurrentThread());

  // Can't call client anymore after Destroy() returns.
  client_ptr_factory_.reset();
  weak_this_ptr_factory_.InvalidateWeakPtrs();

  // Early-exit encoder tasks if they are running and join the thread.
  if (encoder_thread_.IsRunning()) {
    encoder_thread_.message_loop()->PostTask(
        FROM_HERE, base::Bind(&VaapiVideoEncodeAccelerator::DestroyTask,
                              base::Unretained(this)));
    encoder_thread_.Stop();
  }

  delete this;
}

void VaapiVideoEncodeAccelerator::DestroyTask() {
  DVLOGF(2);
  DCHECK(encoder_thread_task_runner_->BelongsToCurrentThread());
  SetState(kError);
}

void VaapiVideoEncodeAccelerator::UpdateSPS() {
  memset(&current_sps_, 0, sizeof(H264SPS));

  // Spec A.2 and A.3.
  switch (profile_) {
    case H264PROFILE_BASELINE:
      // Due to crbug.com/345569, we don't distinguish between constrained
      // and non-constrained baseline profiles. Since many codecs can't do
      // non-constrained, and constrained is usually what we mean (and it's a
      // subset of non-constrained), default to it.
      current_sps_.profile_idc = H264SPS::kProfileIDCBaseline;
      current_sps_.constraint_set0_flag = true;
      break;
    case H264PROFILE_MAIN:
      current_sps_.profile_idc = H264SPS::kProfileIDCMain;
      current_sps_.constraint_set1_flag = true;
      break;
    case H264PROFILE_HIGH:
      current_sps_.profile_idc = H264SPS::kProfileIDCHigh;
      break;
    default:
      NOTIMPLEMENTED();
      return;
  }

  current_sps_.level_idc = kDefaultLevelIDC;
  current_sps_.seq_parameter_set_id = 0;
  current_sps_.chroma_format_idc = kChromaFormatIDC;

  DCHECK_GE(idr_period_, 1u << 4);
  current_sps_.log2_max_frame_num_minus4 = Log2OfPowerOf2(idr_period_) - 4;
  current_sps_.pic_order_cnt_type = 0;
  current_sps_.log2_max_pic_order_cnt_lsb_minus4 =
      Log2OfPowerOf2(idr_period_ * 2) - 4;
  current_sps_.max_num_ref_frames = max_ref_idx_l0_size_;

  current_sps_.frame_mbs_only_flag = true;

  DCHECK_GT(mb_width_, 0u);
  DCHECK_GT(mb_height_, 0u);
  current_sps_.pic_width_in_mbs_minus1 = mb_width_ - 1;
  DCHECK(current_sps_.frame_mbs_only_flag);
  current_sps_.pic_height_in_map_units_minus1 = mb_height_ - 1;

  if (visible_size_ != coded_size_) {
    // Visible size differs from coded size, fill crop information.
    current_sps_.frame_cropping_flag = true;
    DCHECK(!current_sps_.separate_colour_plane_flag);
    // Spec table 6-1. Only 4:2:0 for now.
    DCHECK_EQ(current_sps_.chroma_format_idc, 1);
    // Spec 7.4.2.1.1. Crop is in crop units, which is 2 pixels for 4:2:0.
    const unsigned int crop_unit_x = 2;
    const unsigned int crop_unit_y = 2 * (2 - current_sps_.frame_mbs_only_flag);
    current_sps_.frame_crop_left_offset = 0;
    current_sps_.frame_crop_right_offset =
        (coded_size_.width() - visible_size_.width()) / crop_unit_x;
    current_sps_.frame_crop_top_offset = 0;
    current_sps_.frame_crop_bottom_offset =
        (coded_size_.height() - visible_size_.height()) / crop_unit_y;
  }

  current_sps_.vui_parameters_present_flag = true;
  current_sps_.timing_info_present_flag = true;
  current_sps_.num_units_in_tick = 1;
  current_sps_.time_scale = framerate_ * 2;  // See equation D-2 in spec.
  current_sps_.fixed_frame_rate_flag = true;

  current_sps_.nal_hrd_parameters_present_flag = true;
  // H.264 spec ch. E.2.2.
  current_sps_.cpb_cnt_minus1 = 0;
  current_sps_.bit_rate_scale = kBitRateScale;
  current_sps_.cpb_size_scale = kCPBSizeScale;
  current_sps_.bit_rate_value_minus1[0] =
      (bitrate_ >> (kBitRateScale + H264SPS::kBitRateScaleConstantTerm)) - 1;
  current_sps_.cpb_size_value_minus1[0] =
      (cpb_size_ >> (kCPBSizeScale + H264SPS::kCPBSizeScaleConstantTerm)) - 1;
  current_sps_.cbr_flag[0] = true;
  current_sps_.initial_cpb_removal_delay_length_minus_1 =
      H264SPS::kDefaultInitialCPBRemovalDelayLength - 1;
  current_sps_.cpb_removal_delay_length_minus1 =
      H264SPS::kDefaultInitialCPBRemovalDelayLength - 1;
  current_sps_.dpb_output_delay_length_minus1 =
      H264SPS::kDefaultDPBOutputDelayLength - 1;
  current_sps_.time_offset_length = H264SPS::kDefaultTimeOffsetLength;
  current_sps_.low_delay_hrd_flag = false;
}

void VaapiVideoEncodeAccelerator::GeneratePackedSPS() {
  packed_sps_.Reset();

  packed_sps_.BeginNALU(H264NALU::kSPS, 3);

  packed_sps_.AppendBits(8, current_sps_.profile_idc);
  packed_sps_.AppendBool(current_sps_.constraint_set0_flag);
  packed_sps_.AppendBool(current_sps_.constraint_set1_flag);
  packed_sps_.AppendBool(current_sps_.constraint_set2_flag);
  packed_sps_.AppendBool(current_sps_.constraint_set3_flag);
  packed_sps_.AppendBool(current_sps_.constraint_set4_flag);
  packed_sps_.AppendBool(current_sps_.constraint_set5_flag);
  packed_sps_.AppendBits(2, 0);  // reserved_zero_2bits
  packed_sps_.AppendBits(8, current_sps_.level_idc);
  packed_sps_.AppendUE(current_sps_.seq_parameter_set_id);

  if (current_sps_.profile_idc == H264SPS::kProfileIDCHigh) {
    packed_sps_.AppendUE(current_sps_.chroma_format_idc);
    if (current_sps_.chroma_format_idc == 3)
      packed_sps_.AppendBool(current_sps_.separate_colour_plane_flag);
    packed_sps_.AppendUE(current_sps_.bit_depth_luma_minus8);
    packed_sps_.AppendUE(current_sps_.bit_depth_chroma_minus8);
    packed_sps_.AppendBool(current_sps_.qpprime_y_zero_transform_bypass_flag);
    packed_sps_.AppendBool(current_sps_.seq_scaling_matrix_present_flag);
    CHECK(!current_sps_.seq_scaling_matrix_present_flag);
  }

  packed_sps_.AppendUE(current_sps_.log2_max_frame_num_minus4);
  packed_sps_.AppendUE(current_sps_.pic_order_cnt_type);
  if (current_sps_.pic_order_cnt_type == 0)
    packed_sps_.AppendUE(current_sps_.log2_max_pic_order_cnt_lsb_minus4);
  else if (current_sps_.pic_order_cnt_type == 1) {
    CHECK(1);
  }

  packed_sps_.AppendUE(current_sps_.max_num_ref_frames);
  packed_sps_.AppendBool(current_sps_.gaps_in_frame_num_value_allowed_flag);
  packed_sps_.AppendUE(current_sps_.pic_width_in_mbs_minus1);
  packed_sps_.AppendUE(current_sps_.pic_height_in_map_units_minus1);

  packed_sps_.AppendBool(current_sps_.frame_mbs_only_flag);
  if (!current_sps_.frame_mbs_only_flag)
    packed_sps_.AppendBool(current_sps_.mb_adaptive_frame_field_flag);

  packed_sps_.AppendBool(current_sps_.direct_8x8_inference_flag);

  packed_sps_.AppendBool(current_sps_.frame_cropping_flag);
  if (current_sps_.frame_cropping_flag) {
    packed_sps_.AppendUE(current_sps_.frame_crop_left_offset);
    packed_sps_.AppendUE(current_sps_.frame_crop_right_offset);
    packed_sps_.AppendUE(current_sps_.frame_crop_top_offset);
    packed_sps_.AppendUE(current_sps_.frame_crop_bottom_offset);
  }

  packed_sps_.AppendBool(current_sps_.vui_parameters_present_flag);
  if (current_sps_.vui_parameters_present_flag) {
    packed_sps_.AppendBool(false);  // aspect_ratio_info_present_flag
    packed_sps_.AppendBool(false);  // overscan_info_present_flag
    packed_sps_.AppendBool(false);  // video_signal_type_present_flag
    packed_sps_.AppendBool(false);  // chroma_loc_info_present_flag

    packed_sps_.AppendBool(current_sps_.timing_info_present_flag);
    if (current_sps_.timing_info_present_flag) {
      packed_sps_.AppendBits(32, current_sps_.num_units_in_tick);
      packed_sps_.AppendBits(32, current_sps_.time_scale);
      packed_sps_.AppendBool(current_sps_.fixed_frame_rate_flag);
    }

    packed_sps_.AppendBool(current_sps_.nal_hrd_parameters_present_flag);
    if (current_sps_.nal_hrd_parameters_present_flag) {
      packed_sps_.AppendUE(current_sps_.cpb_cnt_minus1);
      packed_sps_.AppendBits(4, current_sps_.bit_rate_scale);
      packed_sps_.AppendBits(4, current_sps_.cpb_size_scale);
      CHECK_LT(base::checked_cast<size_t>(current_sps_.cpb_cnt_minus1),
               arraysize(current_sps_.bit_rate_value_minus1));
      for (int i = 0; i <= current_sps_.cpb_cnt_minus1; ++i) {
        packed_sps_.AppendUE(current_sps_.bit_rate_value_minus1[i]);
        packed_sps_.AppendUE(current_sps_.cpb_size_value_minus1[i]);
        packed_sps_.AppendBool(current_sps_.cbr_flag[i]);
      }
      packed_sps_.AppendBits(
          5, current_sps_.initial_cpb_removal_delay_length_minus_1);
      packed_sps_.AppendBits(5, current_sps_.cpb_removal_delay_length_minus1);
      packed_sps_.AppendBits(5, current_sps_.dpb_output_delay_length_minus1);
      packed_sps_.AppendBits(5, current_sps_.time_offset_length);
    }

    packed_sps_.AppendBool(false);  // vcl_hrd_parameters_flag
    if (current_sps_.nal_hrd_parameters_present_flag)
      packed_sps_.AppendBool(current_sps_.low_delay_hrd_flag);

    packed_sps_.AppendBool(false);  // pic_struct_present_flag
    packed_sps_.AppendBool(true);   // bitstream_restriction_flag

    packed_sps_.AppendBool(false);  // motion_vectors_over_pic_boundaries_flag
    packed_sps_.AppendUE(2);        // max_bytes_per_pic_denom
    packed_sps_.AppendUE(1);        // max_bits_per_mb_denom
    packed_sps_.AppendUE(16);       // log2_max_mv_length_horizontal
    packed_sps_.AppendUE(16);       // log2_max_mv_length_vertical

    // Explicitly set max_num_reorder_frames to 0 to allow the decoder to
    // output pictures early.
    packed_sps_.AppendUE(0);  // max_num_reorder_frames

    // The value of max_dec_frame_buffering shall be greater than or equal to
    // max_num_ref_frames.
    const unsigned int max_dec_frame_buffering =
        current_sps_.max_num_ref_frames;
    packed_sps_.AppendUE(max_dec_frame_buffering);
  }

  packed_sps_.FinishNALU();
}

void VaapiVideoEncodeAccelerator::UpdatePPS() {
  memset(&current_pps_, 0, sizeof(H264PPS));

  current_pps_.seq_parameter_set_id = current_sps_.seq_parameter_set_id;
  current_pps_.pic_parameter_set_id = 0;

  current_pps_.entropy_coding_mode_flag =
      current_sps_.profile_idc >= H264SPS::kProfileIDCMain;

  CHECK_GT(max_ref_idx_l0_size_, 0u);
  current_pps_.num_ref_idx_l0_default_active_minus1 = max_ref_idx_l0_size_ - 1;
  current_pps_.num_ref_idx_l1_default_active_minus1 = 0;
  DCHECK_LE(qp_, 51u);
  current_pps_.pic_init_qp_minus26 = qp_ - 26;
  current_pps_.deblocking_filter_control_present_flag = true;
  current_pps_.transform_8x8_mode_flag =
      (current_sps_.profile_idc == H264SPS::kProfileIDCHigh);
}

void VaapiVideoEncodeAccelerator::GeneratePackedPPS() {
  packed_pps_.Reset();

  packed_pps_.BeginNALU(H264NALU::kPPS, 3);

  packed_pps_.AppendUE(current_pps_.pic_parameter_set_id);
  packed_pps_.AppendUE(current_pps_.seq_parameter_set_id);
  packed_pps_.AppendBool(current_pps_.entropy_coding_mode_flag);
  packed_pps_.AppendBool(
      current_pps_.bottom_field_pic_order_in_frame_present_flag);
  CHECK_EQ(current_pps_.num_slice_groups_minus1, 0);
  packed_pps_.AppendUE(current_pps_.num_slice_groups_minus1);

  packed_pps_.AppendUE(current_pps_.num_ref_idx_l0_default_active_minus1);
  packed_pps_.AppendUE(current_pps_.num_ref_idx_l1_default_active_minus1);

  packed_pps_.AppendBool(current_pps_.weighted_pred_flag);
  packed_pps_.AppendBits(2, current_pps_.weighted_bipred_idc);

  packed_pps_.AppendSE(current_pps_.pic_init_qp_minus26);
  packed_pps_.AppendSE(current_pps_.pic_init_qs_minus26);
  packed_pps_.AppendSE(current_pps_.chroma_qp_index_offset);

  packed_pps_.AppendBool(current_pps_.deblocking_filter_control_present_flag);
  packed_pps_.AppendBool(current_pps_.constrained_intra_pred_flag);
  packed_pps_.AppendBool(current_pps_.redundant_pic_cnt_present_flag);

  packed_pps_.AppendBool(current_pps_.transform_8x8_mode_flag);
  packed_pps_.AppendBool(current_pps_.pic_scaling_matrix_present_flag);
  DCHECK(!current_pps_.pic_scaling_matrix_present_flag);
  packed_pps_.AppendSE(current_pps_.second_chroma_qp_index_offset);

  packed_pps_.FinishNALU();
}

void VaapiVideoEncodeAccelerator::SetState(State state) {
  // Only touch state on encoder thread, unless it's not running.
  if (encoder_thread_.IsRunning() &&
      !encoder_thread_task_runner_->BelongsToCurrentThread()) {
    encoder_thread_task_runner_->PostTask(
        FROM_HERE, base::Bind(&VaapiVideoEncodeAccelerator::SetState,
                              base::Unretained(this), state));
    return;
  }

  DVLOGF(1) << "setting state to: " << state;
  state_ = state;
}

void VaapiVideoEncodeAccelerator::NotifyError(Error error) {
  if (!child_task_runner_->BelongsToCurrentThread()) {
    child_task_runner_->PostTask(
        FROM_HERE, base::Bind(&VaapiVideoEncodeAccelerator::NotifyError,
                              weak_this_, error));
    return;
  }

  if (client_) {
    client_->NotifyError(error);
    client_ptr_factory_.reset();
  }
}

VaapiVideoEncodeAccelerator::EncodeJob::EncodeJob()
    : coded_buffer(VA_INVALID_ID), keyframe(false) {}

VaapiVideoEncodeAccelerator::EncodeJob::~EncodeJob() {}

}  // namespace media
