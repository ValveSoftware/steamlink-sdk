// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_util.h"

#include <cmath>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"
#include "media/base/video_frame.h"
#include "media/base/yuv_convert.h"
#include "third_party/libyuv/include/libyuv.h"

namespace media {

namespace {

// Empty method used for keeping a reference to the original media::VideoFrame.
void ReleaseOriginalFrame(const scoped_refptr<media::VideoFrame>& frame) {}

// Helper to apply padding to the region outside visible rect up to the coded
// size with the repeated last column / row of the visible rect.
void FillRegionOutsideVisibleRect(uint8_t* data,
                                  size_t stride,
                                  const gfx::Size& coded_size,
                                  const gfx::Size& visible_size) {
  if (visible_size.IsEmpty()) {
    if (!coded_size.IsEmpty())
      memset(data, 0, coded_size.height() * stride);
    return;
  }

  const int coded_width = coded_size.width();
  if (visible_size.width() < coded_width) {
    const int pad_length = coded_width - visible_size.width();
    uint8_t* dst = data + visible_size.width();
    for (int i = 0; i < visible_size.height(); ++i, dst += stride)
      std::memset(dst, *(dst - 1), pad_length);
  }

  if (visible_size.height() < coded_size.height()) {
    uint8_t* dst = data + visible_size.height() * stride;
    uint8_t* src = dst - stride;
    for (int i = visible_size.height(); i < coded_size.height();
         ++i, dst += stride)
      std::memcpy(dst, src, coded_width);
  }
}

}  // namespace

gfx::Size GetNaturalSize(const gfx::Size& visible_size,
                         int aspect_ratio_numerator,
                         int aspect_ratio_denominator) {
  if (aspect_ratio_denominator == 0 ||
      aspect_ratio_numerator < 0 ||
      aspect_ratio_denominator < 0)
    return gfx::Size();

  double aspect_ratio = aspect_ratio_numerator /
      static_cast<double>(aspect_ratio_denominator);

  return gfx::Size(round(visible_size.width() * aspect_ratio),
                   visible_size.height());
}

void FillYUV(VideoFrame* frame, uint8_t y, uint8_t u, uint8_t v) {
  // Fill the Y plane.
  uint8_t* y_plane = frame->data(VideoFrame::kYPlane);
  int y_rows = frame->rows(VideoFrame::kYPlane);
  int y_row_bytes = frame->row_bytes(VideoFrame::kYPlane);
  for (int i = 0; i < y_rows; ++i) {
    memset(y_plane, y, y_row_bytes);
    y_plane += frame->stride(VideoFrame::kYPlane);
  }

  // Fill the U and V planes.
  uint8_t* u_plane = frame->data(VideoFrame::kUPlane);
  uint8_t* v_plane = frame->data(VideoFrame::kVPlane);
  int uv_rows = frame->rows(VideoFrame::kUPlane);
  int u_row_bytes = frame->row_bytes(VideoFrame::kUPlane);
  int v_row_bytes = frame->row_bytes(VideoFrame::kVPlane);
  for (int i = 0; i < uv_rows; ++i) {
    memset(u_plane, u, u_row_bytes);
    memset(v_plane, v, v_row_bytes);
    u_plane += frame->stride(VideoFrame::kUPlane);
    v_plane += frame->stride(VideoFrame::kVPlane);
  }
}

void FillYUVA(VideoFrame* frame, uint8_t y, uint8_t u, uint8_t v, uint8_t a) {
  // Fill Y, U and V planes.
  FillYUV(frame, y, u, v);

  // Fill the A plane.
  uint8_t* a_plane = frame->data(VideoFrame::kAPlane);
  int a_rows = frame->rows(VideoFrame::kAPlane);
  int a_row_bytes = frame->row_bytes(VideoFrame::kAPlane);
  for (int i = 0; i < a_rows; ++i) {
    memset(a_plane, a, a_row_bytes);
    a_plane += frame->stride(VideoFrame::kAPlane);
  }
}

static void LetterboxPlane(VideoFrame* frame,
                           int plane,
                           const gfx::Rect& view_area,
                           uint8_t fill_byte) {
  uint8_t* ptr = frame->data(plane);
  const int rows = frame->rows(plane);
  const int row_bytes = frame->row_bytes(plane);
  const int stride = frame->stride(plane);

  CHECK_GE(stride, row_bytes);
  CHECK_GE(view_area.x(), 0);
  CHECK_GE(view_area.y(), 0);
  CHECK_LE(view_area.right(), row_bytes);
  CHECK_LE(view_area.bottom(), rows);

  int y = 0;
  for (; y < view_area.y(); y++) {
    memset(ptr, fill_byte, row_bytes);
    ptr += stride;
  }
  if (view_area.width() < row_bytes) {
    for (; y < view_area.bottom(); y++) {
      if (view_area.x() > 0) {
        memset(ptr, fill_byte, view_area.x());
      }
      if (view_area.right() < row_bytes) {
        memset(ptr + view_area.right(),
               fill_byte,
               row_bytes - view_area.right());
      }
      ptr += stride;
    }
  } else {
    y += view_area.height();
    ptr += stride * view_area.height();
  }
  for (; y < rows; y++) {
    memset(ptr, fill_byte, row_bytes);
    ptr += stride;
  }
}

void LetterboxYUV(VideoFrame* frame, const gfx::Rect& view_area) {
  DCHECK(!(view_area.x() & 1));
  DCHECK(!(view_area.y() & 1));
  DCHECK(!(view_area.width() & 1));
  DCHECK(!(view_area.height() & 1));
  DCHECK(frame->format() == PIXEL_FORMAT_YV12 ||
         frame->format() == PIXEL_FORMAT_I420);
  LetterboxPlane(frame, VideoFrame::kYPlane, view_area, 0x00);
  gfx::Rect half_view_area(view_area.x() / 2,
                           view_area.y() / 2,
                           view_area.width() / 2,
                           view_area.height() / 2);
  LetterboxPlane(frame, VideoFrame::kUPlane, half_view_area, 0x80);
  LetterboxPlane(frame, VideoFrame::kVPlane, half_view_area, 0x80);
}

void RotatePlaneByPixels(const uint8_t* src,
                         uint8_t* dest,
                         int width,
                         int height,
                         int rotation,  // Clockwise.
                         bool flip_vert,
                         bool flip_horiz) {
  DCHECK((width > 0) && (height > 0) &&
         ((width & 1) == 0) && ((height & 1) == 0) &&
         (rotation >= 0) && (rotation < 360) && (rotation % 90 == 0));

  // Consolidate cases. Only 0 and 90 are left.
  if (rotation == 180 || rotation == 270) {
    rotation -= 180;
    flip_vert = !flip_vert;
    flip_horiz = !flip_horiz;
  }

  int num_rows = height;
  int num_cols = width;
  int src_stride = width;
  // During pixel copying, the corresponding incremental of dest pointer
  // when src pointer moves to next row.
  int dest_row_step = width;
  // During pixel copying, the corresponding incremental of dest pointer
  // when src pointer moves to next column.
  int dest_col_step = 1;

  if (rotation == 0) {
    if (flip_horiz) {
      // Use pixel copying.
      dest_col_step = -1;
      if (flip_vert) {
        // Rotation 180.
        dest_row_step = -width;
        dest += height * width - 1;
      } else {
        dest += width - 1;
      }
    } else {
      if (flip_vert) {
        // Fast copy by rows.
        dest += width * (height - 1);
        for (int row = 0; row < height; ++row) {
          memcpy(dest, src, width);
          src += width;
          dest -= width;
        }
      } else {
        memcpy(dest, src, width * height);
      }
      return;
    }
  } else if (rotation == 90) {
    int offset;
    if (width > height) {
      offset = (width - height) / 2;
      src += offset;
      num_rows = num_cols = height;
    } else {
      offset = (height - width) / 2;
      src += width * offset;
      num_rows = num_cols = width;
    }

    dest_col_step = (flip_vert ? -width : width);
    dest_row_step = (flip_horiz ? 1 : -1);
    if (flip_horiz) {
      if (flip_vert) {
        dest += (width > height ? width * (height - 1) + offset :
                                  width * (height - offset - 1));
      } else {
        dest += (width > height ? offset : width * offset);
      }
    } else {
      if (flip_vert) {
        dest += (width > height ?  width * height - offset - 1 :
                                   width * (height - offset) - 1);
      } else {
        dest += (width > height ? width - offset - 1 :
                                  width * (offset + 1) - 1);
      }
    }
  } else {
    NOTREACHED();
  }

  // Copy pixels.
  for (int row = 0; row < num_rows; ++row) {
    const uint8_t* src_ptr = src;
    uint8_t* dest_ptr = dest;
    for (int col = 0; col < num_cols; ++col) {
      *dest_ptr = *src_ptr++;
      dest_ptr += dest_col_step;
    }
    src += src_stride;
    dest += dest_row_step;
  }
}

// Helper function to return |a| divided by |b|, rounded to the nearest integer.
static int RoundedDivision(int64_t a, int b) {
  DCHECK_GE(a, 0);
  DCHECK_GT(b, 0);
  base::CheckedNumeric<uint64_t> result(a);
  result += b / 2;
  result /= b;
  return base::checked_cast<int>(result.ValueOrDie());
}

// Common logic for the letterboxing and scale-within/scale-encompassing
// functions.  Scales |size| to either fit within or encompass |target|,
// depending on whether |fit_within_target| is true.
static gfx::Size ScaleSizeToTarget(const gfx::Size& size,
                                   const gfx::Size& target,
                                   bool fit_within_target) {
  if (size.IsEmpty())
    return gfx::Size();  // Corner case: Aspect ratio is undefined.

  const int64_t x = static_cast<int64_t>(size.width()) * target.height();
  const int64_t y = static_cast<int64_t>(size.height()) * target.width();
  const bool use_target_width = fit_within_target ? (y < x) : (x < y);
  return use_target_width ?
      gfx::Size(target.width(), RoundedDivision(y, size.width())) :
      gfx::Size(RoundedDivision(x, size.height()), target.height());
}

gfx::Rect ComputeLetterboxRegion(const gfx::Rect& bounds,
                                 const gfx::Size& content) {
  // If |content| has an undefined aspect ratio, let's not try to divide by
  // zero.
  if (content.IsEmpty())
    return gfx::Rect();

  gfx::Rect result = bounds;
  result.ClampToCenteredSize(ScaleSizeToTarget(content, bounds.size(), true));
  return result;
}

gfx::Size ScaleSizeToFitWithinTarget(const gfx::Size& size,
                                     const gfx::Size& target) {
  return ScaleSizeToTarget(size, target, true);
}

gfx::Size ScaleSizeToEncompassTarget(const gfx::Size& size,
                                     const gfx::Size& target) {
  return ScaleSizeToTarget(size, target, false);
}

gfx::Size PadToMatchAspectRatio(const gfx::Size& size,
                                const gfx::Size& target) {
  if (target.IsEmpty())
    return gfx::Size();  // Aspect ratio is undefined.

  const int64_t x = static_cast<int64_t>(size.width()) * target.height();
  const int64_t y = static_cast<int64_t>(size.height()) * target.width();
  if (x < y)
    return gfx::Size(RoundedDivision(y, target.height()), size.height());
  return gfx::Size(size.width(), RoundedDivision(x, target.width()));
}

void CopyRGBToVideoFrame(const uint8_t* source,
                         int stride,
                         const gfx::Rect& region_in_frame,
                         VideoFrame* frame) {
  const int kY = VideoFrame::kYPlane;
  const int kU = VideoFrame::kUPlane;
  const int kV = VideoFrame::kVPlane;
  CHECK_EQ(frame->stride(kU), frame->stride(kV));
  const int uv_stride = frame->stride(kU);

  if (region_in_frame != gfx::Rect(frame->coded_size())) {
    LetterboxYUV(frame, region_in_frame);
  }

  const int y_offset = region_in_frame.x()
                     + (region_in_frame.y() * frame->stride(kY));
  const int uv_offset = region_in_frame.x() / 2
                      + (region_in_frame.y() / 2 * uv_stride);

  ConvertRGB32ToYUV(source,
                    frame->data(kY) + y_offset,
                    frame->data(kU) + uv_offset,
                    frame->data(kV) + uv_offset,
                    region_in_frame.width(),
                    region_in_frame.height(),
                    stride,
                    frame->stride(kY),
                    uv_stride);
}

scoped_refptr<VideoFrame> WrapAsI420VideoFrame(
    const scoped_refptr<VideoFrame>& frame) {
  DCHECK_EQ(VideoFrame::STORAGE_OWNED_MEMORY, frame->storage_type());
  DCHECK_EQ(PIXEL_FORMAT_YV12A, frame->format());

  scoped_refptr<media::VideoFrame> wrapped_frame =
      media::VideoFrame::WrapVideoFrame(frame, PIXEL_FORMAT_I420,
                                        frame->visible_rect(),
                                        frame->natural_size());
  if (!wrapped_frame)
    return nullptr;
  wrapped_frame->AddDestructionObserver(
      base::Bind(&ReleaseOriginalFrame, frame));
  return wrapped_frame;
}

bool I420CopyWithPadding(const VideoFrame& src_frame, VideoFrame* dst_frame) {
  if (!dst_frame || !dst_frame->IsMappable())
    return false;

  DCHECK_GE(dst_frame->coded_size().width(), src_frame.visible_rect().width());
  DCHECK_GE(dst_frame->coded_size().height(),
            src_frame.visible_rect().height());
  DCHECK(dst_frame->visible_rect().origin().IsOrigin());

  if (libyuv::I420Copy(src_frame.visible_data(VideoFrame::kYPlane),
                       src_frame.stride(VideoFrame::kYPlane),
                       src_frame.visible_data(VideoFrame::kUPlane),
                       src_frame.stride(VideoFrame::kUPlane),
                       src_frame.visible_data(VideoFrame::kVPlane),
                       src_frame.stride(VideoFrame::kVPlane),
                       dst_frame->data(VideoFrame::kYPlane),
                       dst_frame->stride(VideoFrame::kYPlane),
                       dst_frame->data(VideoFrame::kUPlane),
                       dst_frame->stride(VideoFrame::kUPlane),
                       dst_frame->data(VideoFrame::kVPlane),
                       dst_frame->stride(VideoFrame::kVPlane),
                       src_frame.visible_rect().width(),
                       src_frame.visible_rect().height()))
    return false;

  // Padding the region outside the visible rect with the repeated last
  // column / row of the visible rect. This can improve the coding efficiency.
  FillRegionOutsideVisibleRect(dst_frame->data(VideoFrame::kYPlane),
                               dst_frame->stride(VideoFrame::kYPlane),
                               dst_frame->coded_size(),
                               src_frame.visible_rect().size());
  FillRegionOutsideVisibleRect(
      dst_frame->data(VideoFrame::kUPlane),
      dst_frame->stride(VideoFrame::kUPlane),
      VideoFrame::PlaneSize(PIXEL_FORMAT_I420, VideoFrame::kUPlane,
                            dst_frame->coded_size()),
      VideoFrame::PlaneSize(PIXEL_FORMAT_I420, VideoFrame::kUPlane,
                            src_frame.visible_rect().size()));
  FillRegionOutsideVisibleRect(
      dst_frame->data(VideoFrame::kVPlane),
      dst_frame->stride(VideoFrame::kVPlane),
      VideoFrame::PlaneSize(PIXEL_FORMAT_I420, VideoFrame::kVPlane,
                            dst_frame->coded_size()),
      VideoFrame::PlaneSize(PIXEL_FORMAT_I420, VideoFrame::kVPlane,
                            src_frame.visible_rect().size()));

  return true;
}

}  // namespace media
