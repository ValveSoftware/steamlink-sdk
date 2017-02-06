// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vt_video_decode_accelerator_mac.h"

#include <CoreVideo/CoreVideo.h>
#include <OpenGL/CGLIOSurface.h>
#include <OpenGL/gl.h>
#include <stddef.h>

#include <algorithm>
#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "base/mac/mac_logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/sys_byteorder.h"
#include "base/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/version.h"
#include "media/base/limits.h"
#include "media/gpu/shared_memory_region.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image_io_surface.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/scoped_binders.h"

using media_gpu::kModuleVt;
using media_gpu::InitializeStubs;
using media_gpu::IsVtInitialized;
using media_gpu::StubPathMap;

#define NOTIFY_STATUS(name, status, session_failure) \
  do {                                               \
    OSSTATUS_DLOG(ERROR, status) << name;            \
    NotifyError(PLATFORM_FAILURE, session_failure);  \
  } while (0)

namespace media {

// Only H.264 with 4:2:0 chroma sampling is supported.
static const VideoCodecProfile kSupportedProfiles[] = {
    H264PROFILE_BASELINE, H264PROFILE_MAIN, H264PROFILE_EXTENDED,
    H264PROFILE_HIGH,
    // TODO(hubbe): Try to re-enable this again somehow. Currently it seems
    // that some codecs fail to check the profile during initialization and
    // then fail on the first frame decode, which currently results in a
    // pipeline failure.
    // H264PROFILE_HIGH10PROFILE,
    H264PROFILE_SCALABLEBASELINE, H264PROFILE_SCALABLEHIGH,
    H264PROFILE_STEREOHIGH, H264PROFILE_MULTIVIEWHIGH,
};

// Size to use for NALU length headers in AVC format (can be 1, 2, or 4).
static const int kNALUHeaderLength = 4;

// We request 5 picture buffers from the client, each of which has a texture ID
// that we can bind decoded frames to. We need enough to satisfy preroll, and
// enough to avoid unnecessary stalling, but no more than that. The resource
// requirements are low, as we don't need the textures to be backed by storage.
static const int kNumPictureBuffers = limits::kMaxVideoFrames + 1;

// Maximum number of frames to queue for reordering before we stop asking for
// more. (NotifyEndOfBitstreamBuffer() is called when frames are moved into the
// reorder queue.)
static const int kMaxReorderQueueSize = 16;

// Build an |image_config| dictionary for VideoToolbox initialization.
static base::ScopedCFTypeRef<CFMutableDictionaryRef> BuildImageConfig(
    CMVideoDimensions coded_dimensions) {
  base::ScopedCFTypeRef<CFMutableDictionaryRef> image_config;

  // Note that 4:2:0 textures cannot be used directly as RGBA in OpenGL, but are
  // lower power than 4:2:2 when composited directly by CoreAnimation.
  int32_t pixel_format = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
#define CFINT(i) CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &i)
  base::ScopedCFTypeRef<CFNumberRef> cf_pixel_format(CFINT(pixel_format));
  base::ScopedCFTypeRef<CFNumberRef> cf_width(CFINT(coded_dimensions.width));
  base::ScopedCFTypeRef<CFNumberRef> cf_height(CFINT(coded_dimensions.height));
#undef CFINT
  if (!cf_pixel_format.get() || !cf_width.get() || !cf_height.get())
    return image_config;

  image_config.reset(CFDictionaryCreateMutable(
      kCFAllocatorDefault,
      3,  // capacity
      &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
  if (!image_config.get())
    return image_config;

  CFDictionarySetValue(image_config, kCVPixelBufferPixelFormatTypeKey,
                       cf_pixel_format);
  CFDictionarySetValue(image_config, kCVPixelBufferWidthKey, cf_width);
  CFDictionarySetValue(image_config, kCVPixelBufferHeightKey, cf_height);

  return image_config;
}

// Create a VTDecompressionSession using the provided |pps| and |sps|. If
// |require_hardware| is true, the session must uses real hardware decoding
// (as opposed to software decoding inside of VideoToolbox) to be considered
// successful.
//
// TODO(sandersd): Merge with ConfigureDecoder(), as the code is very similar.
static bool CreateVideoToolboxSession(const uint8_t* sps,
                                      size_t sps_size,
                                      const uint8_t* pps,
                                      size_t pps_size,
                                      bool require_hardware) {
  const uint8_t* data_ptrs[] = {sps, pps};
  const size_t data_sizes[] = {sps_size, pps_size};

  base::ScopedCFTypeRef<CMFormatDescriptionRef> format;
  OSStatus status = CMVideoFormatDescriptionCreateFromH264ParameterSets(
      kCFAllocatorDefault,
      2,                  // parameter_set_count
      data_ptrs,          // &parameter_set_pointers
      data_sizes,         // &parameter_set_sizes
      kNALUHeaderLength,  // nal_unit_header_length
      format.InitializeInto());
  if (status) {
    OSSTATUS_DLOG(WARNING, status)
        << "Failed to create CMVideoFormatDescription";
    return false;
  }

  base::ScopedCFTypeRef<CFMutableDictionaryRef> decoder_config(
      CFDictionaryCreateMutable(kCFAllocatorDefault,
                                1,  // capacity
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  if (!decoder_config.get())
    return false;

  if (require_hardware) {
    CFDictionarySetValue(
        decoder_config,
        // kVTVideoDecoderSpecification_RequireHardwareAcceleratedVideoDecoder
        CFSTR("RequireHardwareAcceleratedVideoDecoder"), kCFBooleanTrue);
  }

  base::ScopedCFTypeRef<CFMutableDictionaryRef> image_config(
      BuildImageConfig(CMVideoFormatDescriptionGetDimensions(format)));
  if (!image_config.get())
    return false;

  VTDecompressionOutputCallbackRecord callback = {0};

  base::ScopedCFTypeRef<VTDecompressionSessionRef> session;
  status = VTDecompressionSessionCreate(
      kCFAllocatorDefault,
      format,          // video_format_description
      decoder_config,  // video_decoder_specification
      image_config,    // destination_image_buffer_attributes
      &callback,       // output_callback
      session.InitializeInto());
  if (status) {
    OSSTATUS_DLOG(WARNING, status) << "Failed to create VTDecompressionSession";
    return false;
  }

  return true;
}

// The purpose of this function is to preload the generic and hardware-specific
// libraries required by VideoToolbox before the GPU sandbox is enabled.
// VideoToolbox normally loads the hardware-specific libraries lazily, so we
// must actually create a decompression session. If creating a decompression
// session fails, hardware decoding will be disabled (Initialize() will always
// return false).
static bool InitializeVideoToolboxInternal() {
  if (!IsVtInitialized()) {
    // CoreVideo is also required, but the loader stops after the first path is
    // loaded. Instead we rely on the transitive dependency from VideoToolbox to
    // CoreVideo.
    StubPathMap paths;
    paths[kModuleVt].push_back(FILE_PATH_LITERAL(
        "/System/Library/Frameworks/VideoToolbox.framework/VideoToolbox"));
    if (!InitializeStubs(paths)) {
      DLOG(WARNING) << "Failed to initialize VideoToolbox framework";
      return false;
    }
  }

  // Create a hardware decoding session.
  // SPS and PPS data are taken from a 480p sample (buck2.mp4).
  const uint8_t sps_normal[] = {0x67, 0x64, 0x00, 0x1e, 0xac, 0xd9, 0x80, 0xd4,
                                0x3d, 0xa1, 0x00, 0x00, 0x03, 0x00, 0x01, 0x00,
                                0x00, 0x03, 0x00, 0x30, 0x8f, 0x16, 0x2d, 0x9a};
  const uint8_t pps_normal[] = {0x68, 0xe9, 0x7b, 0xcb};
  if (!CreateVideoToolboxSession(sps_normal, arraysize(sps_normal), pps_normal,
                                 arraysize(pps_normal), true)) {
    DLOG(WARNING) << "Failed to create hardware VideoToolbox session";
    return false;
  }

  // Create a software decoding session.
  // SPS and PPS data are taken from a 18p sample (small2.mp4).
  const uint8_t sps_small[] = {0x67, 0x64, 0x00, 0x0a, 0xac, 0xd9, 0x89, 0x7e,
                               0x22, 0x10, 0x00, 0x00, 0x3e, 0x90, 0x00, 0x0e,
                               0xa6, 0x08, 0xf1, 0x22, 0x59, 0xa0};
  const uint8_t pps_small[] = {0x68, 0xe9, 0x79, 0x72, 0xc0};
  if (!CreateVideoToolboxSession(sps_small, arraysize(sps_small), pps_small,
                                 arraysize(pps_small), false)) {
    DLOG(WARNING) << "Failed to create software VideoToolbox session";
    return false;
  }

  return true;
}

bool InitializeVideoToolbox() {
  // InitializeVideoToolbox() is called only from the GPU process main thread;
  // once for sandbox warmup, and then once each time a VTVideoDecodeAccelerator
  // is initialized.
  static bool attempted = false;
  static bool succeeded = false;

  if (!attempted) {
    attempted = true;
    succeeded = InitializeVideoToolboxInternal();
  }

  return succeeded;
}

// Route decoded frame callbacks back into the VTVideoDecodeAccelerator.
static void OutputThunk(void* decompression_output_refcon,
                        void* source_frame_refcon,
                        OSStatus status,
                        VTDecodeInfoFlags info_flags,
                        CVImageBufferRef image_buffer,
                        CMTime presentation_time_stamp,
                        CMTime presentation_duration) {
  VTVideoDecodeAccelerator* vda =
      reinterpret_cast<VTVideoDecodeAccelerator*>(decompression_output_refcon);
  vda->Output(source_frame_refcon, status, image_buffer);
}

VTVideoDecodeAccelerator::Task::Task(TaskType type) : type(type) {}

VTVideoDecodeAccelerator::Task::Task(const Task& other) = default;

VTVideoDecodeAccelerator::Task::~Task() {}

VTVideoDecodeAccelerator::Frame::Frame(int32_t bitstream_id)
    : bitstream_id(bitstream_id),
      pic_order_cnt(0),
      is_idr(false),
      reorder_window(0) {}

VTVideoDecodeAccelerator::Frame::~Frame() {}

VTVideoDecodeAccelerator::PictureInfo::PictureInfo(uint32_t client_texture_id,
                                                   uint32_t service_texture_id)
    : client_texture_id(client_texture_id),
      service_texture_id(service_texture_id) {}

VTVideoDecodeAccelerator::PictureInfo::~PictureInfo() {
  if (gl_image)
    gl_image->Destroy(false);
}

bool VTVideoDecodeAccelerator::FrameOrder::operator()(
    const linked_ptr<Frame>& lhs,
    const linked_ptr<Frame>& rhs) const {
  if (lhs->pic_order_cnt != rhs->pic_order_cnt)
    return lhs->pic_order_cnt > rhs->pic_order_cnt;
  // If |pic_order_cnt| is the same, fall back on using the bitstream order.
  // TODO(sandersd): Assign a sequence number in Decode() and use that instead.
  // TODO(sandersd): Using the sequence number, ensure that frames older than
  // |kMaxReorderQueueSize| are ordered first, regardless of |pic_order_cnt|.
  return lhs->bitstream_id > rhs->bitstream_id;
}

VTVideoDecodeAccelerator::VTVideoDecodeAccelerator(
    const MakeGLContextCurrentCallback& make_context_current_cb,
    const BindGLImageCallback& bind_image_cb)
    : make_context_current_cb_(make_context_current_cb),
      bind_image_cb_(bind_image_cb),
      client_(nullptr),
      state_(STATE_DECODING),
      format_(nullptr),
      session_(nullptr),
      last_sps_id_(-1),
      last_pps_id_(-1),
      config_changed_(false),
      waiting_for_idr_(true),
      missing_idr_logged_(false),
      gpu_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      decoder_thread_("VTDecoderThread"),
      weak_this_factory_(this) {
  callback_.decompressionOutputCallback = OutputThunk;
  callback_.decompressionOutputRefCon = this;
  weak_this_ = weak_this_factory_.GetWeakPtr();
}

VTVideoDecodeAccelerator::~VTVideoDecodeAccelerator() {
  DCHECK(gpu_thread_checker_.CalledOnValidThread());
}

bool VTVideoDecodeAccelerator::Initialize(const Config& config,
                                          Client* client) {
  DCHECK(gpu_thread_checker_.CalledOnValidThread());

  if (make_context_current_cb_.is_null() || bind_image_cb_.is_null()) {
    NOTREACHED() << "GL callbacks are required for this VDA";
    return false;
  }

  if (config.is_encrypted) {
    NOTREACHED() << "Encrypted streams are not supported for this VDA";
    return false;
  }

  if (config.output_mode != Config::OutputMode::ALLOCATE) {
    NOTREACHED() << "Only ALLOCATE OutputMode is supported by this VDA";
    return false;
  }

  client_ = client;

  if (!InitializeVideoToolbox())
    return false;

  bool profile_supported = false;
  for (const auto& supported_profile : kSupportedProfiles) {
    if (config.profile == supported_profile) {
      profile_supported = true;
      break;
    }
  }
  if (!profile_supported)
    return false;

  // Spawn a thread to handle parsing and calling VideoToolbox.
  if (!decoder_thread_.Start())
    return false;

  // Count the session as successfully initialized.
  UMA_HISTOGRAM_ENUMERATION("Media.VTVDA.SessionFailureReason",
                            SFT_SUCCESSFULLY_INITIALIZED, SFT_MAX + 1);
  return true;
}

bool VTVideoDecodeAccelerator::FinishDelayedFrames() {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  if (session_) {
    OSStatus status = VTDecompressionSessionWaitForAsynchronousFrames(session_);
    if (status) {
      NOTIFY_STATUS("VTDecompressionSessionWaitForAsynchronousFrames()", status,
                    SFT_PLATFORM_ERROR);
      return false;
    }
  }
  return true;
}

bool VTVideoDecodeAccelerator::ConfigureDecoder() {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK(!last_sps_.empty());
  DCHECK(!last_pps_.empty());

  // Build the configuration records.
  std::vector<const uint8_t*> nalu_data_ptrs;
  std::vector<size_t> nalu_data_sizes;
  nalu_data_ptrs.reserve(3);
  nalu_data_sizes.reserve(3);
  nalu_data_ptrs.push_back(&last_sps_.front());
  nalu_data_sizes.push_back(last_sps_.size());
  if (!last_spsext_.empty()) {
    nalu_data_ptrs.push_back(&last_spsext_.front());
    nalu_data_sizes.push_back(last_spsext_.size());
  }
  nalu_data_ptrs.push_back(&last_pps_.front());
  nalu_data_sizes.push_back(last_pps_.size());

  // Construct a new format description from the parameter sets.
  format_.reset();
  OSStatus status = CMVideoFormatDescriptionCreateFromH264ParameterSets(
      kCFAllocatorDefault,
      nalu_data_ptrs.size(),     // parameter_set_count
      &nalu_data_ptrs.front(),   // &parameter_set_pointers
      &nalu_data_sizes.front(),  // &parameter_set_sizes
      kNALUHeaderLength,         // nal_unit_header_length
      format_.InitializeInto());
  if (status) {
    NOTIFY_STATUS("CMVideoFormatDescriptionCreateFromH264ParameterSets()",
                  status, SFT_PLATFORM_ERROR);
    return false;
  }

  // Store the new configuration data.
  // TODO(sandersd): Despite the documentation, this seems to return the visible
  // size. However, the output always appears to be top-left aligned, so it
  // makes no difference. Re-verify this and update the variable name.
  CMVideoDimensions coded_dimensions =
      CMVideoFormatDescriptionGetDimensions(format_);
  coded_size_.SetSize(coded_dimensions.width, coded_dimensions.height);

  // Prepare VideoToolbox configuration dictionaries.
  base::ScopedCFTypeRef<CFMutableDictionaryRef> decoder_config(
      CFDictionaryCreateMutable(kCFAllocatorDefault,
                                1,  // capacity
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  if (!decoder_config.get()) {
    DLOG(ERROR) << "Failed to create CFMutableDictionary";
    NotifyError(PLATFORM_FAILURE, SFT_PLATFORM_ERROR);
    return false;
  }

  CFDictionarySetValue(
      decoder_config,
      // kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder
      CFSTR("EnableHardwareAcceleratedVideoDecoder"), kCFBooleanTrue);

  base::ScopedCFTypeRef<CFMutableDictionaryRef> image_config(
      BuildImageConfig(coded_dimensions));
  if (!image_config.get()) {
    DLOG(ERROR) << "Failed to create decoder image configuration";
    NotifyError(PLATFORM_FAILURE, SFT_PLATFORM_ERROR);
    return false;
  }

  // Ensure that the old decoder emits all frames before the new decoder can
  // emit any.
  if (!FinishDelayedFrames())
    return false;

  session_.reset();
  status = VTDecompressionSessionCreate(
      kCFAllocatorDefault,
      format_,         // video_format_description
      decoder_config,  // video_decoder_specification
      image_config,    // destination_image_buffer_attributes
      &callback_,      // output_callback
      session_.InitializeInto());
  if (status) {
    NOTIFY_STATUS("VTDecompressionSessionCreate()", status,
                  SFT_UNSUPPORTED_STREAM_PARAMETERS);
    return false;
  }

  // Report whether hardware decode is being used.
  bool using_hardware = false;
  base::ScopedCFTypeRef<CFBooleanRef> cf_using_hardware;
  if (VTSessionCopyProperty(
          session_,
          // kVTDecompressionPropertyKey_UsingHardwareAcceleratedVideoDecoder
          CFSTR("UsingHardwareAcceleratedVideoDecoder"), kCFAllocatorDefault,
          cf_using_hardware.InitializeInto()) == 0) {
    using_hardware = CFBooleanGetValue(cf_using_hardware);
  }
  UMA_HISTOGRAM_BOOLEAN("Media.VTVDA.HardwareAccelerated", using_hardware);

  return true;
}

void VTVideoDecodeAccelerator::DecodeTask(const BitstreamBuffer& bitstream,
                                          Frame* frame) {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  // Map the bitstream buffer.
  SharedMemoryRegion memory(bitstream, true);
  if (!memory.Map()) {
    DLOG(ERROR) << "Failed to map bitstream buffer";
    NotifyError(PLATFORM_FAILURE, SFT_PLATFORM_ERROR);
    return;
  }
  const uint8_t* buf = static_cast<uint8_t*>(memory.memory());

  // NALUs are stored with Annex B format in the bitstream buffer (start codes),
  // but VideoToolbox expects AVC format (length headers), so we must rewrite
  // the data.
  //
  // Locate relevant NALUs and compute the size of the rewritten data. Also
  // record any parameter sets for VideoToolbox initialization.
  std::vector<uint8_t> sps;
  std::vector<uint8_t> spsext;
  std::vector<uint8_t> pps;
  bool has_slice = false;
  size_t data_size = 0;
  std::vector<H264NALU> nalus;
  parser_.SetStream(buf, memory.size());
  H264NALU nalu;
  while (true) {
    H264Parser::Result result = parser_.AdvanceToNextNALU(&nalu);
    if (result == H264Parser::kEOStream)
      break;
    if (result == H264Parser::kUnsupportedStream) {
      DLOG(ERROR) << "Unsupported H.264 stream";
      NotifyError(PLATFORM_FAILURE, SFT_UNSUPPORTED_STREAM);
      return;
    }
    if (result != H264Parser::kOk) {
      DLOG(ERROR) << "Failed to parse H.264 stream";
      NotifyError(UNREADABLE_INPUT, SFT_INVALID_STREAM);
      return;
    }
    switch (nalu.nal_unit_type) {
      case H264NALU::kSPS:
        result = parser_.ParseSPS(&last_sps_id_);
        if (result == H264Parser::kUnsupportedStream) {
          DLOG(ERROR) << "Unsupported SPS";
          NotifyError(PLATFORM_FAILURE, SFT_UNSUPPORTED_STREAM);
          return;
        }
        if (result != H264Parser::kOk) {
          DLOG(ERROR) << "Could not parse SPS";
          NotifyError(UNREADABLE_INPUT, SFT_INVALID_STREAM);
          return;
        }
        sps.assign(nalu.data, nalu.data + nalu.size);
        spsext.clear();
        break;

      case H264NALU::kSPSExt:
        // TODO(sandersd): Check that the previous NALU was an SPS.
        spsext.assign(nalu.data, nalu.data + nalu.size);
        break;

      case H264NALU::kPPS:
        result = parser_.ParsePPS(&last_pps_id_);
        if (result == H264Parser::kUnsupportedStream) {
          DLOG(ERROR) << "Unsupported PPS";
          NotifyError(PLATFORM_FAILURE, SFT_UNSUPPORTED_STREAM);
          return;
        }
        if (result != H264Parser::kOk) {
          DLOG(ERROR) << "Could not parse PPS";
          NotifyError(UNREADABLE_INPUT, SFT_INVALID_STREAM);
          return;
        }
        pps.assign(nalu.data, nalu.data + nalu.size);
        break;

      case H264NALU::kSliceDataA:
      case H264NALU::kSliceDataB:
      case H264NALU::kSliceDataC:
      case H264NALU::kNonIDRSlice:
      case H264NALU::kIDRSlice:
        // Compute the |pic_order_cnt| for the picture from the first slice.
        if (!has_slice) {
          // Verify that we are not trying to decode a slice without an IDR.
          if (waiting_for_idr_) {
            if (nalu.nal_unit_type == H264NALU::kIDRSlice) {
              waiting_for_idr_ = false;
            } else {
              // We can't compute anything yet, bail on this frame.
              has_slice = true;
              break;
            }
          }

          H264SliceHeader slice_hdr;
          result = parser_.ParseSliceHeader(nalu, &slice_hdr);
          if (result == H264Parser::kUnsupportedStream) {
            DLOG(ERROR) << "Unsupported slice header";
            NotifyError(PLATFORM_FAILURE, SFT_UNSUPPORTED_STREAM);
            return;
          }
          if (result != H264Parser::kOk) {
            DLOG(ERROR) << "Could not parse slice header";
            NotifyError(UNREADABLE_INPUT, SFT_INVALID_STREAM);
            return;
          }

          // TODO(sandersd): Maintain a cache of configurations and reconfigure
          // when a slice references a new config.
          DCHECK_EQ(slice_hdr.pic_parameter_set_id, last_pps_id_);
          const H264PPS* pps = parser_.GetPPS(slice_hdr.pic_parameter_set_id);
          if (!pps) {
            DLOG(ERROR) << "Mising PPS referenced by slice";
            NotifyError(UNREADABLE_INPUT, SFT_INVALID_STREAM);
            return;
          }

          DCHECK_EQ(pps->seq_parameter_set_id, last_sps_id_);
          const H264SPS* sps = parser_.GetSPS(pps->seq_parameter_set_id);
          if (!sps) {
            DLOG(ERROR) << "Mising SPS referenced by PPS";
            NotifyError(UNREADABLE_INPUT, SFT_INVALID_STREAM);
            return;
          }

          if (!poc_.ComputePicOrderCnt(sps, slice_hdr, &frame->pic_order_cnt)) {
            DLOG(ERROR) << "Unable to compute POC";
            NotifyError(UNREADABLE_INPUT, SFT_INVALID_STREAM);
            return;
          }

          if (nalu.nal_unit_type == H264NALU::kIDRSlice)
            frame->is_idr = true;

          if (sps->vui_parameters_present_flag &&
              sps->bitstream_restriction_flag) {
            frame->reorder_window =
                std::min(sps->max_num_reorder_frames, kMaxReorderQueueSize - 1);
          }
        }
        has_slice = true;
      default:
        nalus.push_back(nalu);
        data_size += kNALUHeaderLength + nalu.size;
        break;
    }
  }

  // Initialize VideoToolbox.
  if (!sps.empty() && sps != last_sps_) {
    last_sps_.swap(sps);
    last_spsext_.swap(spsext);
    config_changed_ = true;
  }
  if (!pps.empty() && pps != last_pps_) {
    last_pps_.swap(pps);
    config_changed_ = true;
  }
  if (config_changed_) {
    // Only reconfigure at IDRs to avoid corruption.
    if (frame->is_idr) {
      config_changed_ = false;

      if (last_sps_.empty()) {
        DLOG(ERROR) << "Invalid configuration; no SPS";
        NotifyError(INVALID_ARGUMENT, SFT_INVALID_STREAM);
        return;
      }
      if (last_pps_.empty()) {
        DLOG(ERROR) << "Invalid configuration; no PPS";
        NotifyError(INVALID_ARGUMENT, SFT_INVALID_STREAM);
        return;
      }

      // ConfigureDecoder() calls NotifyError() on failure.
      if (!ConfigureDecoder())
        return;
    }
  }

  // If no IDR has been seen yet, skip decoding.
  if (has_slice && waiting_for_idr_) {
    if (!missing_idr_logged_) {
      LOG(ERROR) << "Illegal attempt to decode without IDR. "
                 << "Discarding decode requests until next IDR.";
      missing_idr_logged_ = true;
    }
    has_slice = false;
  }

  // If there is nothing to decode, drop the bitstream buffer by returning an
  // empty frame.
  if (!has_slice) {
    // Keep everything in order by flushing first.
    if (!FinishDelayedFrames())
      return;
    gpu_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&VTVideoDecodeAccelerator::DecodeDone, weak_this_, frame));
    return;
  }

  // If the session is not configured by this point, fail.
  if (!session_) {
    DLOG(ERROR) << "Cannot decode without configuration";
    NotifyError(INVALID_ARGUMENT, SFT_INVALID_STREAM);
    return;
  }

  // Update the frame metadata with configuration data.
  frame->coded_size = coded_size_;

  // Create a memory-backed CMBlockBuffer for the translated data.
  // TODO(sandersd): Pool of memory blocks.
  base::ScopedCFTypeRef<CMBlockBufferRef> data;
  OSStatus status = CMBlockBufferCreateWithMemoryBlock(
      kCFAllocatorDefault,
      nullptr,              // &memory_block
      data_size,            // block_length
      kCFAllocatorDefault,  // block_allocator
      nullptr,              // &custom_block_source
      0,                    // offset_to_data
      data_size,            // data_length
      0,                    // flags
      data.InitializeInto());
  if (status) {
    NOTIFY_STATUS("CMBlockBufferCreateWithMemoryBlock()", status,
                  SFT_PLATFORM_ERROR);
    return;
  }

  // Make sure that the memory is actually allocated.
  // CMBlockBufferReplaceDataBytes() is documented to do this, but prints a
  // message each time starting in Mac OS X 10.10.
  status = CMBlockBufferAssureBlockMemory(data);
  if (status) {
    NOTIFY_STATUS("CMBlockBufferAssureBlockMemory()", status,
                  SFT_PLATFORM_ERROR);
    return;
  }

  // Copy NALU data into the CMBlockBuffer, inserting length headers.
  size_t offset = 0;
  for (size_t i = 0; i < nalus.size(); i++) {
    H264NALU& nalu = nalus[i];
    uint32_t header = base::HostToNet32(static_cast<uint32_t>(nalu.size));
    status =
        CMBlockBufferReplaceDataBytes(&header, data, offset, kNALUHeaderLength);
    if (status) {
      NOTIFY_STATUS("CMBlockBufferReplaceDataBytes()", status,
                    SFT_PLATFORM_ERROR);
      return;
    }
    offset += kNALUHeaderLength;
    status = CMBlockBufferReplaceDataBytes(nalu.data, data, offset, nalu.size);
    if (status) {
      NOTIFY_STATUS("CMBlockBufferReplaceDataBytes()", status,
                    SFT_PLATFORM_ERROR);
      return;
    }
    offset += nalu.size;
  }

  // Package the data in a CMSampleBuffer.
  base::ScopedCFTypeRef<CMSampleBufferRef> sample;
  status = CMSampleBufferCreate(kCFAllocatorDefault,
                                data,        // data_buffer
                                true,        // data_ready
                                nullptr,     // make_data_ready_callback
                                nullptr,     // make_data_ready_refcon
                                format_,     // format_description
                                1,           // num_samples
                                0,           // num_sample_timing_entries
                                nullptr,     // &sample_timing_array
                                1,           // num_sample_size_entries
                                &data_size,  // &sample_size_array
                                sample.InitializeInto());
  if (status) {
    NOTIFY_STATUS("CMSampleBufferCreate()", status, SFT_PLATFORM_ERROR);
    return;
  }

  // Send the frame for decoding.
  // Asynchronous Decompression allows for parallel submission of frames
  // (without it, DecodeFrame() does not return until the frame has been
  // decoded). We don't enable Temporal Processing because we are not passing
  // timestamps anyway.
  VTDecodeFrameFlags decode_flags =
      kVTDecodeFrame_EnableAsynchronousDecompression;
  status = VTDecompressionSessionDecodeFrame(
      session_,
      sample,                          // sample_buffer
      decode_flags,                    // decode_flags
      reinterpret_cast<void*>(frame),  // source_frame_refcon
      nullptr);                        // &info_flags_out
  if (status) {
    NOTIFY_STATUS("VTDecompressionSessionDecodeFrame()", status,
                  SFT_DECODE_ERROR);
    return;
  }
}

// This method may be called on any VideoToolbox thread.
void VTVideoDecodeAccelerator::Output(void* source_frame_refcon,
                                      OSStatus status,
                                      CVImageBufferRef image_buffer) {
  if (status) {
    NOTIFY_STATUS("Decoding", status, SFT_DECODE_ERROR);
    return;
  }

  // The type of |image_buffer| is CVImageBuffer, but we only handle
  // CVPixelBuffers. This should be guaranteed as we set
  // kCVPixelBufferOpenGLCompatibilityKey in |image_config|.
  //
  // Sometimes, for unknown reasons (http://crbug.com/453050), |image_buffer| is
  // NULL, which causes CFGetTypeID() to crash. While the rest of the code would
  // smoothly handle NULL as a dropped frame, we choose to fail permanantly here
  // until the issue is better understood.
  if (!image_buffer || CFGetTypeID(image_buffer) != CVPixelBufferGetTypeID()) {
    DLOG(ERROR) << "Decoded frame is not a CVPixelBuffer";
    NotifyError(PLATFORM_FAILURE, SFT_DECODE_ERROR);
    return;
  }

  Frame* frame = reinterpret_cast<Frame*>(source_frame_refcon);
  frame->image.reset(image_buffer, base::scoped_policy::RETAIN);
  gpu_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VTVideoDecodeAccelerator::DecodeDone, weak_this_, frame));
}

void VTVideoDecodeAccelerator::DecodeDone(Frame* frame) {
  DCHECK(gpu_thread_checker_.CalledOnValidThread());
  DCHECK_EQ(1u, pending_frames_.count(frame->bitstream_id));
  Task task(TASK_FRAME);
  task.frame = pending_frames_[frame->bitstream_id];
  pending_frames_.erase(frame->bitstream_id);
  task_queue_.push(task);
  ProcessWorkQueues();
}

void VTVideoDecodeAccelerator::FlushTask(TaskType type) {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  FinishDelayedFrames();

  // Always queue a task, even if FinishDelayedFrames() fails, so that
  // destruction always completes.
  gpu_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VTVideoDecodeAccelerator::FlushDone, weak_this_, type));
}

void VTVideoDecodeAccelerator::FlushDone(TaskType type) {
  DCHECK(gpu_thread_checker_.CalledOnValidThread());
  task_queue_.push(Task(type));
  ProcessWorkQueues();
}

void VTVideoDecodeAccelerator::Decode(const BitstreamBuffer& bitstream) {
  DCHECK(gpu_thread_checker_.CalledOnValidThread());
  if (bitstream.id() < 0) {
    DLOG(ERROR) << "Invalid bitstream, id: " << bitstream.id();
    if (base::SharedMemory::IsHandleValid(bitstream.handle()))
      base::SharedMemory::CloseHandle(bitstream.handle());
    NotifyError(INVALID_ARGUMENT, SFT_INVALID_STREAM);
    return;
  }
  DCHECK_EQ(0u, assigned_bitstream_ids_.count(bitstream.id()));
  assigned_bitstream_ids_.insert(bitstream.id());
  Frame* frame = new Frame(bitstream.id());
  pending_frames_[frame->bitstream_id] = make_linked_ptr(frame);
  decoder_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&VTVideoDecodeAccelerator::DecodeTask,
                            base::Unretained(this), bitstream, frame));
}

void VTVideoDecodeAccelerator::AssignPictureBuffers(
    const std::vector<PictureBuffer>& pictures) {
  DCHECK(gpu_thread_checker_.CalledOnValidThread());

  for (const PictureBuffer& picture : pictures) {
    DCHECK(!picture_info_map_.count(picture.id()));
    assigned_picture_ids_.insert(picture.id());
    available_picture_ids_.push_back(picture.id());
    DCHECK_LE(1u, picture.internal_texture_ids().size());
    DCHECK_LE(1u, picture.texture_ids().size());
    picture_info_map_.insert(std::make_pair(
        picture.id(),
        base::WrapUnique(new PictureInfo(picture.internal_texture_ids()[0],
                                         picture.texture_ids()[0]))));
  }

  // Pictures are not marked as uncleared until after this method returns, and
  // they will be broken if they are used before that happens. So, schedule
  // future work after that happens.
  gpu_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VTVideoDecodeAccelerator::ProcessWorkQueues, weak_this_));
}

void VTVideoDecodeAccelerator::ReusePictureBuffer(int32_t picture_id) {
  DCHECK(gpu_thread_checker_.CalledOnValidThread());

  auto it = picture_info_map_.find(picture_id);
  if (it != picture_info_map_.end()) {
    PictureInfo* picture_info = it->second.get();
    picture_info->cv_image.reset();
    picture_info->gl_image->Destroy(false);
    picture_info->gl_image = nullptr;
  }

  if (assigned_picture_ids_.count(picture_id)) {
    available_picture_ids_.push_back(picture_id);
    ProcessWorkQueues();
  } else {
    client_->DismissPictureBuffer(picture_id);
  }
}

void VTVideoDecodeAccelerator::ProcessWorkQueues() {
  DCHECK(gpu_thread_checker_.CalledOnValidThread());
  switch (state_) {
    case STATE_DECODING:
      // TODO(sandersd): Batch where possible.
      while (state_ == STATE_DECODING) {
        if (!ProcessReorderQueue() && !ProcessTaskQueue())
          break;
      }
      return;

    case STATE_ERROR:
      // Do nothing until Destroy() is called.
      return;

    case STATE_DESTROYING:
      // Drop tasks until we are ready to destruct.
      while (!task_queue_.empty()) {
        if (task_queue_.front().type == TASK_DESTROY) {
          delete this;
          return;
        }
        task_queue_.pop();
      }
      return;
  }
}

bool VTVideoDecodeAccelerator::ProcessTaskQueue() {
  DCHECK(gpu_thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, STATE_DECODING);

  if (task_queue_.empty())
    return false;

  const Task& task = task_queue_.front();
  switch (task.type) {
    case TASK_FRAME:
      if (reorder_queue_.size() < kMaxReorderQueueSize &&
          (!task.frame->is_idr || reorder_queue_.empty())) {
        assigned_bitstream_ids_.erase(task.frame->bitstream_id);
        client_->NotifyEndOfBitstreamBuffer(task.frame->bitstream_id);
        reorder_queue_.push(task.frame);
        task_queue_.pop();
        return true;
      }
      return false;

    case TASK_FLUSH:
      DCHECK_EQ(task.type, pending_flush_tasks_.front());
      if (reorder_queue_.size() == 0) {
        pending_flush_tasks_.pop();
        client_->NotifyFlushDone();
        task_queue_.pop();
        return true;
      }
      return false;

    case TASK_RESET:
      DCHECK_EQ(task.type, pending_flush_tasks_.front());
      if (reorder_queue_.size() == 0) {
        waiting_for_idr_ = true;
        pending_flush_tasks_.pop();
        client_->NotifyResetDone();
        task_queue_.pop();
        return true;
      }
      return false;

    case TASK_DESTROY:
      NOTREACHED() << "Can't destroy while in STATE_DECODING";
      NotifyError(ILLEGAL_STATE, SFT_PLATFORM_ERROR);
      return false;
  }
}

bool VTVideoDecodeAccelerator::ProcessReorderQueue() {
  DCHECK(gpu_thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, STATE_DECODING);

  if (reorder_queue_.empty())
    return false;

  // If the next task is a flush (because there is a pending flush or becuase
  // the next frame is an IDR), then we don't need a full reorder buffer to send
  // the next frame.
  bool flushing =
      !task_queue_.empty() && (task_queue_.front().type != TASK_FRAME ||
                               task_queue_.front().frame->is_idr);

  size_t reorder_window = std::max(0, reorder_queue_.top()->reorder_window);
  if (flushing || reorder_queue_.size() > reorder_window) {
    if (ProcessFrame(*reorder_queue_.top())) {
      reorder_queue_.pop();
      return true;
    }
  }

  return false;
}

bool VTVideoDecodeAccelerator::ProcessFrame(const Frame& frame) {
  DCHECK(gpu_thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, STATE_DECODING);

  // If the next pending flush is for a reset, then the frame will be dropped.
  bool resetting = !pending_flush_tasks_.empty() &&
                   pending_flush_tasks_.front() == TASK_RESET;

  if (!resetting && frame.image.get()) {
    // If the |coded_size| has changed, request new picture buffers and then
    // wait for them.
    // TODO(sandersd): If GpuVideoDecoder didn't specifically check the size of
    // textures, this would be unnecessary, as the size is actually a property
    // of the texture binding, not the texture. We rebind every frame, so the
    // size passed to ProvidePictureBuffers() is meaningless.
    if (picture_size_ != frame.coded_size) {
      // Dismiss current pictures.
      for (int32_t picture_id : assigned_picture_ids_)
        client_->DismissPictureBuffer(picture_id);
      assigned_picture_ids_.clear();
      available_picture_ids_.clear();

      // Request new pictures.
      picture_size_ = frame.coded_size;
      client_->ProvidePictureBuffers(kNumPictureBuffers, PIXEL_FORMAT_UNKNOWN,
                                     1, coded_size_, GL_TEXTURE_RECTANGLE_ARB);
      return false;
    }
    if (!SendFrame(frame))
      return false;
  }

  return true;
}

bool VTVideoDecodeAccelerator::SendFrame(const Frame& frame) {
  DCHECK(gpu_thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, STATE_DECODING);

  if (available_picture_ids_.empty())
    return false;

  int32_t picture_id = available_picture_ids_.back();
  auto it = picture_info_map_.find(picture_id);
  DCHECK(it != picture_info_map_.end());
  PictureInfo* picture_info = it->second.get();
  DCHECK(!picture_info->cv_image);
  DCHECK(!picture_info->gl_image);

  if (!make_context_current_cb_.Run()) {
    DLOG(ERROR) << "Failed to make GL context current";
    NotifyError(PLATFORM_FAILURE, SFT_PLATFORM_ERROR);
    return false;
  }

  scoped_refptr<gl::GLImageIOSurface> gl_image(
      new gl::GLImageIOSurface(frame.coded_size, GL_BGRA_EXT));
  if (!gl_image->InitializeWithCVPixelBuffer(
          frame.image.get(), gfx::GenericSharedMemoryId(),
          gfx::BufferFormat::YUV_420_BIPLANAR)) {
    NOTIFY_STATUS("Failed to initialize GLImageIOSurface", PLATFORM_FAILURE,
                  SFT_PLATFORM_ERROR);
  }

  if (!bind_image_cb_.Run(picture_info->client_texture_id,
                          GL_TEXTURE_RECTANGLE_ARB, gl_image, false)) {
    DLOG(ERROR) << "Failed to bind image";
    NotifyError(PLATFORM_FAILURE, SFT_PLATFORM_ERROR);
    return false;
  }

  // Assign the new image(s) to the the picture info.
  picture_info->gl_image = gl_image;
  picture_info->cv_image = frame.image;
  available_picture_ids_.pop_back();

  // TODO(sandersd): Currently, the size got from
  // CMVideoFormatDescriptionGetDimensions is visible size. We pass it to
  // GpuVideoDecoder so that GpuVideoDecoder can use correct visible size in
  // resolution changed. We should find the correct API to get the real
  // coded size and fix it.
  client_->PictureReady(Picture(picture_id, frame.bitstream_id,
                                gfx::Rect(frame.coded_size), true));
  return true;
}

void VTVideoDecodeAccelerator::NotifyError(
    Error vda_error_type,
    VTVDASessionFailureType session_failure_type) {
  DCHECK_LT(session_failure_type, SFT_MAX + 1);
  if (!gpu_thread_checker_.CalledOnValidThread()) {
    gpu_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&VTVideoDecodeAccelerator::NotifyError, weak_this_,
                   vda_error_type, session_failure_type));
  } else if (state_ == STATE_DECODING) {
    state_ = STATE_ERROR;
    UMA_HISTOGRAM_ENUMERATION("Media.VTVDA.SessionFailureReason",
                              session_failure_type, SFT_MAX + 1);
    client_->NotifyError(vda_error_type);
  }
}

void VTVideoDecodeAccelerator::QueueFlush(TaskType type) {
  DCHECK(gpu_thread_checker_.CalledOnValidThread());
  pending_flush_tasks_.push(type);
  decoder_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&VTVideoDecodeAccelerator::FlushTask,
                            base::Unretained(this), type));

  // If this is a new flush request, see if we can make progress.
  if (pending_flush_tasks_.size() == 1)
    ProcessWorkQueues();
}

void VTVideoDecodeAccelerator::Flush() {
  DCHECK(gpu_thread_checker_.CalledOnValidThread());
  QueueFlush(TASK_FLUSH);
}

void VTVideoDecodeAccelerator::Reset() {
  DCHECK(gpu_thread_checker_.CalledOnValidThread());
  QueueFlush(TASK_RESET);
}

void VTVideoDecodeAccelerator::Destroy() {
  DCHECK(gpu_thread_checker_.CalledOnValidThread());

  // In a forceful shutdown, the decoder thread may be dead already.
  if (!decoder_thread_.IsRunning()) {
    delete this;
    return;
  }

  // For a graceful shutdown, return assigned buffers and flush before
  // destructing |this|.
  // TODO(sandersd): Prevent the decoder from reading buffers before discarding
  // them.
  for (int32_t bitstream_id : assigned_bitstream_ids_)
    client_->NotifyEndOfBitstreamBuffer(bitstream_id);
  assigned_bitstream_ids_.clear();
  state_ = STATE_DESTROYING;
  QueueFlush(TASK_DESTROY);
}

bool VTVideoDecodeAccelerator::TryToSetupDecodeOnSeparateThread(
    const base::WeakPtr<Client>& decode_client,
    const scoped_refptr<base::SingleThreadTaskRunner>& decode_task_runner) {
  return false;
}

// static
VideoDecodeAccelerator::SupportedProfiles
VTVideoDecodeAccelerator::GetSupportedProfiles() {
  SupportedProfiles profiles;
  for (const auto& supported_profile : kSupportedProfiles) {
    SupportedProfile profile;
    profile.profile = supported_profile;
    profile.min_resolution.SetSize(16, 16);
    profile.max_resolution.SetSize(4096, 2160);
    profiles.push_back(profile);
  }
  return profiles;
}

}  // namespace media
