// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <linux/videodev2.h>

#include "base/numerics/safe_conversions.h"
#include "content/common/gpu/media/exynos_v4l2_video_device.h"
#include "content/common/gpu/media/tegra_v4l2_video_device.h"

namespace content {

V4L2Device::~V4L2Device() {}

// static
scoped_ptr<V4L2Device> V4L2Device::Create(Type type) {
  DVLOG(3) << __PRETTY_FUNCTION__;

  scoped_ptr<ExynosV4L2Device> exynos_device(new ExynosV4L2Device(type));
  if (exynos_device->Initialize())
    return exynos_device.PassAs<V4L2Device>();

  scoped_ptr<TegraV4L2Device> tegra_device(new TegraV4L2Device(type));
  if (tegra_device->Initialize())
    return tegra_device.PassAs<V4L2Device>();

  DLOG(ERROR) << "Failed to create V4L2Device";
  return scoped_ptr<V4L2Device>();
}

// static
media::VideoFrame::Format V4L2Device::V4L2PixFmtToVideoFrameFormat(
    uint32 pix_fmt) {
  switch (pix_fmt) {
    case V4L2_PIX_FMT_NV12:
    case V4L2_PIX_FMT_NV12M:
      return media::VideoFrame::NV12;

    case V4L2_PIX_FMT_YUV420:
    case V4L2_PIX_FMT_YUV420M:
      return media::VideoFrame::I420;

    default:
      LOG(FATAL) << "Add more cases as needed";
      return media::VideoFrame::UNKNOWN;
  }
}

// static
uint32 V4L2Device::VideoFrameFormatToV4L2PixFmt(
    media::VideoFrame::Format format) {
  switch (format) {
    case media::VideoFrame::NV12:
      return V4L2_PIX_FMT_NV12M;

    case media::VideoFrame::I420:
      return V4L2_PIX_FMT_YUV420M;

    default:
      LOG(FATAL) << "Add more cases as needed";
      return 0;
  }
}

// static
uint32 V4L2Device::VideoCodecProfileToV4L2PixFmt(
    media::VideoCodecProfile profile) {
  if (profile >= media::H264PROFILE_MIN &&
      profile <= media::H264PROFILE_MAX) {
    return V4L2_PIX_FMT_H264;
  } else if (profile >= media::VP8PROFILE_MIN &&
             profile <= media::VP8PROFILE_MAX) {
    return V4L2_PIX_FMT_VP8;
  } else {
    LOG(FATAL) << "Add more cases as needed";
    return 0;
  }
}

// static
gfx::Size V4L2Device::CodedSizeFromV4L2Format(struct v4l2_format format) {
  gfx::Size coded_size;
  gfx::Size visible_size;
  media::VideoFrame::Format frame_format = media::VideoFrame::UNKNOWN;
  int bytesperline = 0;
  int sizeimage = 0;

  if (V4L2_TYPE_IS_MULTIPLANAR(format.type) &&
      format.fmt.pix_mp.num_planes > 0) {
    bytesperline =
        base::checked_cast<int>(format.fmt.pix_mp.plane_fmt[0].bytesperline);
    sizeimage =
        base::checked_cast<int>(format.fmt.pix_mp.plane_fmt[0].sizeimage);
    visible_size.SetSize(base::checked_cast<int>(format.fmt.pix_mp.width),
                         base::checked_cast<int>(format.fmt.pix_mp.height));
    frame_format =
        V4L2Device::V4L2PixFmtToVideoFrameFormat(format.fmt.pix_mp.pixelformat);
  } else {
    bytesperline = base::checked_cast<int>(format.fmt.pix.bytesperline);
    sizeimage = base::checked_cast<int>(format.fmt.pix.sizeimage);
    visible_size.SetSize(base::checked_cast<int>(format.fmt.pix.width),
                         base::checked_cast<int>(format.fmt.pix.height));
    frame_format =
        V4L2Device::V4L2PixFmtToVideoFrameFormat(format.fmt.pix.pixelformat);
  }

  int horiz_bpp =
      media::VideoFrame::PlaneHorizontalBitsPerPixel(frame_format, 0);
  if (sizeimage == 0 || bytesperline == 0 || horiz_bpp == 0 ||
      (bytesperline * 8) % horiz_bpp != 0) {
    DLOG(ERROR) << "Invalid format provided";
    return coded_size;
  }

  // Round up sizeimage to full bytesperlines. sizeimage does not have to be
  // a multiple of bytesperline, as in V4L2 terms it's just a byte size of
  // the buffer, unrelated to its payload.
  sizeimage = ((sizeimage + bytesperline - 1) / bytesperline) * bytesperline;

  coded_size.SetSize(bytesperline * 8 / horiz_bpp, sizeimage / bytesperline);

  // Sanity checks. Calculated coded size has to contain given visible size
  // and fulfill buffer byte size requirements for each plane.
  DCHECK(gfx::Rect(coded_size).Contains(gfx::Rect(visible_size)));

  if (V4L2_TYPE_IS_MULTIPLANAR(format.type)) {
    for (size_t i = 0; i < format.fmt.pix_mp.num_planes; ++i) {
      DCHECK_LE(
          format.fmt.pix_mp.plane_fmt[i].sizeimage,
          media::VideoFrame::PlaneAllocationSize(frame_format, i, coded_size));
      DCHECK_LE(format.fmt.pix_mp.plane_fmt[i].bytesperline,
                base::checked_cast<__u32>(coded_size.width()));
    }
  }

  return coded_size;
}

}  //  namespace content
