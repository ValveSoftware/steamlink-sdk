// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/webm/chromeos/webm_encoder.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "libyuv/convert.h"
#include "libyuv/video_common.h"
#include "third_party/skia/include/core/SkBitmap.h"

extern "C" {
// Getting the right degree of C compatibility has been a constant struggle.
// - Stroustrup, C++ Report, 12(7), July/August 2000.
#define private priv
#include "third_party/libvpx/source/libvpx/third_party/libmkv/EbmlIDs.h"
#include "third_party/libvpx/source/libvpx/third_party/libmkv/EbmlWriter.h"
#undef private
}

// Number of encoder threads to use.
static const int kNumEncoderThreads = 2;

// Need a fixed size serializer for the track ID. libmkv provides a 64 bit
// one, but not a 32 bit one.
static void Ebml_SerializeUnsigned32(EbmlGlobal* ebml,
                                     unsigned long class_id,
                                     uint64_t value) {
  uint8 size_serialized = 4 | 0x80;
  Ebml_WriteID(ebml, class_id);
  Ebml_Serialize(ebml, &size_serialized, sizeof(size_serialized), 1);
  Ebml_Serialize(ebml, &value, sizeof(value), 4);
}

// Wrapper functor for vpx_codec_destroy().
struct VpxCodecDeleter {
  void operator()(vpx_codec_ctx_t* codec) {
    vpx_codec_destroy(codec);
  }
};

// Wrapper functor for vpx_img_free().
struct VpxImgDeleter {
  void operator()(vpx_image_t* image) {
    vpx_img_free(image);
  }
};

namespace media {

namespace chromeos {

WebmEncoder::WebmEncoder(const base::FilePath& output_path,
                         int bitrate,
                         bool realtime)
    : bitrate_(bitrate),
      deadline_(realtime ? VPX_DL_REALTIME : VPX_DL_GOOD_QUALITY),
      output_path_(output_path),
      has_errors_(false) {
  ebml_writer_.write_cb = base::Bind(
      &WebmEncoder::EbmlWrite, base::Unretained(this));
  ebml_writer_.serialize_cb = base::Bind(
      &WebmEncoder::EbmlSerialize, base::Unretained(this));
}

WebmEncoder::~WebmEncoder() {
}

bool WebmEncoder::EncodeFromSprite(const SkBitmap& sprite,
                                   int fps_n,
                                   int fps_d) {
  DCHECK(!sprite.isNull());
  DCHECK(!sprite.empty());

  has_errors_ = false;
  width_ = sprite.width();
  height_ = sprite.width();
  fps_.num = fps_n;
  fps_.den = fps_d;

  // Sprite is tiled vertically.
  frame_count_ = sprite.height() / width_;

  vpx_image_t image;
  vpx_img_alloc(&image, VPX_IMG_FMT_I420, width_, height_, 16);
  // Ensure that image is freed after return.
  scoped_ptr<vpx_image_t, VpxImgDeleter> image_ptr(&image);

  const vpx_codec_iface_t* codec_iface = vpx_codec_vp8_cx();
  DCHECK(codec_iface);
  vpx_codec_err_t ret = vpx_codec_enc_config_default(codec_iface, &config_, 0);
  DCHECK_EQ(VPX_CODEC_OK, ret);

  config_.rc_target_bitrate = bitrate_;
  config_.g_w = width_;
  config_.g_h = height_;
  config_.g_pass = VPX_RC_ONE_PASS;
  config_.g_profile = 0;          // Default profile.
  config_.g_threads = kNumEncoderThreads;
  config_.rc_min_quantizer = 0;
  config_.rc_max_quantizer = 63;  // Maximum possible range.
  config_.g_timebase.num = fps_.den;
  config_.g_timebase.den = fps_.num;
  config_.kf_mode = VPX_KF_AUTO;  // Auto key frames.

  vpx_codec_ctx_t codec;
  ret = vpx_codec_enc_init(&codec, codec_iface, &config_, 0);
  if (ret != VPX_CODEC_OK)
    return false;
  // Ensure that codec context is freed after return.
  scoped_ptr<vpx_codec_ctx_t, VpxCodecDeleter> codec_ptr(&codec);

  SkAutoLockPixels lock_sprite(sprite);

  const uint8* src = reinterpret_cast<const uint8*>(sprite.getAddr32(0, 0));
  size_t src_frame_size = sprite.getSize();
  int crop_y = 0;

  if (!WriteWebmHeader())
    return false;

  for (size_t frame = 0; frame < frame_count_ && !has_errors_; ++frame) {
    int res = libyuv::ConvertToI420(
        src, src_frame_size,
        image.planes[VPX_PLANE_Y], image.stride[VPX_PLANE_Y],
        image.planes[VPX_PLANE_U], image.stride[VPX_PLANE_U],
        image.planes[VPX_PLANE_V], image.stride[VPX_PLANE_V],
        0, crop_y,                // src origin
        width_, sprite.height(),  // src size
        width_, height_,          // dest size
        libyuv::kRotate0,
        libyuv::FOURCC_ARGB);
    if (res) {
      has_errors_ = true;
      break;
    }
    crop_y += height_;

    ret = vpx_codec_encode(&codec, &image, frame, 1, 0, deadline_);
    if (ret != VPX_CODEC_OK) {
      has_errors_ = true;
      break;
    }

    vpx_codec_iter_t iter = NULL;
    const vpx_codec_cx_pkt_t* packet;
    while (!has_errors_ && (packet = vpx_codec_get_cx_data(&codec, &iter))) {
      if (packet->kind == VPX_CODEC_CX_FRAME_PKT)
        WriteWebmBlock(packet);
    }
  }

  return WriteWebmFooter();
}

bool WebmEncoder::WriteWebmHeader() {
  output_ = base::OpenFile(output_path_, "wb");
  if (!output_)
    return false;

  // Global header.
  StartSubElement(EBML);
  {
    Ebml_SerializeUnsigned(&ebml_writer_, EBMLVersion, 1);
    Ebml_SerializeUnsigned(&ebml_writer_, EBMLReadVersion, 1);
    Ebml_SerializeUnsigned(&ebml_writer_, EBMLMaxIDLength, 4);
    Ebml_SerializeUnsigned(&ebml_writer_, EBMLMaxSizeLength, 8);
    Ebml_SerializeString(&ebml_writer_, DocType, "webm");
    Ebml_SerializeUnsigned(&ebml_writer_, DocTypeVersion, 2);
    Ebml_SerializeUnsigned(&ebml_writer_, DocTypeReadVersion, 2);
  }
  EndSubElement();  // EBML

  // Single segment with a video track.
  StartSubElement(Segment);
  {
    StartSubElement(Info);
    {
      // All timecodes in the segment will be expressed in milliseconds.
      Ebml_SerializeUnsigned(&ebml_writer_, TimecodeScale, 1000000);
      double duration = 1000. * frame_count_ * fps_.den / fps_.num;
      Ebml_SerializeFloat(&ebml_writer_, Segment_Duration, duration);
    }
    EndSubElement();  // Info

    StartSubElement(Tracks);
    {
      StartSubElement(TrackEntry);
      {
        Ebml_SerializeUnsigned(&ebml_writer_, TrackNumber, 1);
        Ebml_SerializeUnsigned32(&ebml_writer_, TrackUID, 1);
        Ebml_SerializeUnsigned(&ebml_writer_, TrackType, 1);  // Video
        Ebml_SerializeString(&ebml_writer_, CodecID, "V_VP8");

        StartSubElement(Video);
        {
          Ebml_SerializeUnsigned(&ebml_writer_, PixelWidth, width_);
          Ebml_SerializeUnsigned(&ebml_writer_, PixelHeight, height_);
          Ebml_SerializeUnsigned(&ebml_writer_, StereoMode, 0);  // Mono
          float fps = static_cast<float>(fps_.num) / fps_.den;
          Ebml_SerializeFloat(&ebml_writer_, FrameRate, fps);
        }
        EndSubElement();  // Video
      }
      EndSubElement();  // TrackEntry
    }
    EndSubElement();  // Tracks

    StartSubElement(Cluster); {
      Ebml_SerializeUnsigned(&ebml_writer_, Timecode, 0);
    }  // Cluster left open.
  }  // Segment left open.

  // No check for |has_errors_| here because |false| is only returned when
  // opening file fails.
  return true;
}

void WebmEncoder::WriteWebmBlock(const vpx_codec_cx_pkt_t* packet) {
  bool is_keyframe = packet->data.frame.flags & VPX_FRAME_IS_KEY;
  int64_t pts_ms = 1000 * packet->data.frame.pts * fps_.den / fps_.num;

  DVLOG(1) << "Video packet @" << pts_ms << " ms "
           << packet->data.frame.sz << " bytes "
           << (is_keyframe ? "K" : "");

  Ebml_WriteID(&ebml_writer_, SimpleBlock);

  uint32 block_length = (packet->data.frame.sz + 4) | 0x10000000;
  EbmlSerializeHelper(&block_length, 4);

  uint8 track_number = 1 | 0x80;
  EbmlSerializeHelper(&track_number, 1);

  EbmlSerializeHelper(&pts_ms, 2);

  uint8 flags = 0;
  if (is_keyframe)
    flags |= 0x80;
  if (packet->data.frame.flags & VPX_FRAME_IS_INVISIBLE)
    flags |= 0x08;
  EbmlSerializeHelper(&flags, 1);

  EbmlWrite(packet->data.frame.buf, packet->data.frame.sz);
}

bool WebmEncoder::WriteWebmFooter() {
  EndSubElement();  // Cluster
  EndSubElement();  // Segment
  DCHECK(ebml_sub_elements_.empty());
  return base::CloseFile(output_) && !has_errors_;
}

void WebmEncoder::StartSubElement(unsigned long class_id) {
  Ebml_WriteID(&ebml_writer_, class_id);
  ebml_sub_elements_.push(ftell(output_));
  static const uint64_t kUnknownLen = 0x01FFFFFFFFFFFFFFLLU;
  EbmlSerializeHelper(&kUnknownLen, 8);
}

void WebmEncoder::EndSubElement() {
  DCHECK(!ebml_sub_elements_.empty());

  long int end_pos = ftell(output_);
  long int start_pos = ebml_sub_elements_.top();
  ebml_sub_elements_.pop();

  uint64_t size = (end_pos - start_pos - 8) | 0x0100000000000000ULL;
  // Seek to the beginning of the sub-element and patch in the calculated size.
  if (fseek(output_, start_pos, SEEK_SET)) {
    has_errors_ = true;
    LOG(ERROR) << "Error writing to " << output_path_.value();
  }
  EbmlSerializeHelper(&size, 8);

  // Restore write position.
  if (fseek(output_, end_pos, SEEK_SET)) {
    has_errors_ = true;
    LOG(ERROR) << "Error writing to " << output_path_.value();
  }
}

void WebmEncoder::EbmlWrite(const void* buffer,
                            unsigned long len) {
  if (fwrite(buffer, 1, len, output_) != len) {
    has_errors_ = true;
    LOG(ERROR) << "Error writing to " << output_path_.value();
  }
}

template <class T>
void WebmEncoder::EbmlSerializeHelper(const T* buffer, unsigned long len) {
  for (int i = len - 1; i >= 0; i--) {
    uint8 c = *buffer >> (i * CHAR_BIT);
    EbmlWrite(&c, 1);
  }
}

void WebmEncoder::EbmlSerialize(const void* buffer,
                                int buffer_size,
                                unsigned long len) {
  switch (buffer_size) {
    case 1:
      return EbmlSerializeHelper(static_cast<const int8_t*>(buffer), len);
    case 2:
      return EbmlSerializeHelper(static_cast<const int16_t*>(buffer), len);
    case 4:
      return EbmlSerializeHelper(static_cast<const int32_t*>(buffer), len);
    case 8:
      return EbmlSerializeHelper(static_cast<const int64_t*>(buffer), len);
    default:
      NOTREACHED() << "Invalid EbmlSerialize length: " << len;
  }
}

}  // namespace chromeos

}  // namespace media
