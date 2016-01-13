// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/aligned_memory.h"
#include "base/strings/string_piece.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "media/base/limits.h"
#include "media/base/video_util.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace media {

static inline size_t RoundUp(size_t value, size_t alignment) {
  // Check that |alignment| is a power of 2.
  DCHECK((alignment + (alignment - 1)) == (alignment | (alignment - 1)));
  return ((value + (alignment - 1)) & ~(alignment - 1));
}

// static
scoped_refptr<VideoFrame> VideoFrame::CreateFrame(
    VideoFrame::Format format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp) {
  // Since we're creating a new YUV frame (and allocating memory for it
  // ourselves), we can pad the requested |coded_size| if necessary if the
  // request does not line up on sample boundaries.
  gfx::Size new_coded_size(coded_size);
  switch (format) {
    case VideoFrame::YV24:
      break;
    case VideoFrame::YV12:
    case VideoFrame::YV12A:
    case VideoFrame::I420:
    case VideoFrame::YV12J:
      new_coded_size.set_height((new_coded_size.height() + 1) / 2 * 2);
    // Fallthrough.
    case VideoFrame::YV16:
      new_coded_size.set_width((new_coded_size.width() + 1) / 2 * 2);
      break;
    case VideoFrame::UNKNOWN:
    case VideoFrame::NV12:
#if defined(VIDEO_HOLE)
    case VideoFrame::HOLE:
#endif  // defined(VIDEO_HOLE)
    case VideoFrame::NATIVE_TEXTURE:
      LOG(FATAL) << "Only YUV formats supported: " << format;
      return NULL;
  }
  DCHECK(IsValidConfig(format, new_coded_size, visible_rect, natural_size));
  scoped_refptr<VideoFrame> frame(
      new VideoFrame(format,
                     new_coded_size,
                     visible_rect,
                     natural_size,
                     scoped_ptr<gpu::MailboxHolder>(),
                     timestamp,
                     false));
  frame->AllocateYUV();
  return frame;
}

// static
std::string VideoFrame::FormatToString(VideoFrame::Format format) {
  switch (format) {
    case VideoFrame::UNKNOWN:
      return "UNKNOWN";
    case VideoFrame::YV12:
      return "YV12";
    case VideoFrame::YV16:
      return "YV16";
    case VideoFrame::I420:
      return "I420";
    case VideoFrame::NATIVE_TEXTURE:
      return "NATIVE_TEXTURE";
#if defined(VIDEO_HOLE)
    case VideoFrame::HOLE:
      return "HOLE";
#endif  // defined(VIDEO_HOLE)
    case VideoFrame::YV12A:
      return "YV12A";
    case VideoFrame::YV12J:
      return "YV12J";
    case VideoFrame::NV12:
      return "NV12";
    case VideoFrame::YV24:
      return "YV24";
  }
  NOTREACHED() << "Invalid videoframe format provided: " << format;
  return "";
}

// static
bool VideoFrame::IsValidConfig(VideoFrame::Format format,
                               const gfx::Size& coded_size,
                               const gfx::Rect& visible_rect,
                               const gfx::Size& natural_size) {
  // Check maximum limits for all formats.
  if (coded_size.GetArea() > limits::kMaxCanvas ||
      coded_size.width() > limits::kMaxDimension ||
      coded_size.height() > limits::kMaxDimension ||
      visible_rect.x() < 0 || visible_rect.y() < 0 ||
      visible_rect.right() > coded_size.width() ||
      visible_rect.bottom() > coded_size.height() ||
      natural_size.GetArea() > limits::kMaxCanvas ||
      natural_size.width() > limits::kMaxDimension ||
      natural_size.height() > limits::kMaxDimension)
    return false;

  // Check format-specific width/height requirements.
  switch (format) {
    case VideoFrame::UNKNOWN:
      return (coded_size.IsEmpty() && visible_rect.IsEmpty() &&
              natural_size.IsEmpty());
    case VideoFrame::YV24:
      break;
    case VideoFrame::YV12:
    case VideoFrame::YV12J:
    case VideoFrame::I420:
    case VideoFrame::YV12A:
    case VideoFrame::NV12:
      // Subsampled YUV formats have width/height requirements.
      if (static_cast<size_t>(coded_size.height()) <
          RoundUp(visible_rect.bottom(), 2))
        return false;
    // Fallthrough.
    case VideoFrame::YV16:
      if (static_cast<size_t>(coded_size.width()) <
          RoundUp(visible_rect.right(), 2))
        return false;
      break;
    case VideoFrame::NATIVE_TEXTURE:
#if defined(VIDEO_HOLE)
    case VideoFrame::HOLE:
#endif  // defined(VIDEO_HOLE)
      // NATIVE_TEXTURE and HOLE have no software-allocated buffers and are
      // allowed to skip the below check and be empty.
      return true;
  }

  // Check that software-allocated buffer formats are not empty.
  return (!coded_size.IsEmpty() && !visible_rect.IsEmpty() &&
          !natural_size.IsEmpty());
}

// static
scoped_refptr<VideoFrame> VideoFrame::WrapNativeTexture(
    scoped_ptr<gpu::MailboxHolder> mailbox_holder,
    const ReleaseMailboxCB& mailbox_holder_release_cb,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp,
    const ReadPixelsCB& read_pixels_cb) {
  scoped_refptr<VideoFrame> frame(new VideoFrame(NATIVE_TEXTURE,
                                                 coded_size,
                                                 visible_rect,
                                                 natural_size,
                                                 mailbox_holder.Pass(),
                                                 timestamp,
                                                 false));
  frame->mailbox_holder_release_cb_ = mailbox_holder_release_cb;
  frame->read_pixels_cb_ = read_pixels_cb;

  return frame;
}

void VideoFrame::ReadPixelsFromNativeTexture(const SkBitmap& pixels) {
  DCHECK_EQ(format_, NATIVE_TEXTURE);
  if (!read_pixels_cb_.is_null())
    read_pixels_cb_.Run(pixels);
}

// static
scoped_refptr<VideoFrame> VideoFrame::WrapExternalPackedMemory(
    Format format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    uint8* data,
    size_t data_size,
    base::SharedMemoryHandle handle,
    base::TimeDelta timestamp,
    const base::Closure& no_longer_needed_cb) {
  if (!IsValidConfig(format, coded_size, visible_rect, natural_size))
    return NULL;
  if (data_size < AllocationSize(format, coded_size))
    return NULL;

  switch (format) {
    case VideoFrame::I420: {
      scoped_refptr<VideoFrame> frame(
          new VideoFrame(format,
                         coded_size,
                         visible_rect,
                         natural_size,
                         scoped_ptr<gpu::MailboxHolder>(),
                         timestamp,
                         false));
      frame->shared_memory_handle_ = handle;
      frame->strides_[kYPlane] = coded_size.width();
      frame->strides_[kUPlane] = coded_size.width() / 2;
      frame->strides_[kVPlane] = coded_size.width() / 2;
      frame->data_[kYPlane] = data;
      frame->data_[kUPlane] = data + coded_size.GetArea();
      frame->data_[kVPlane] = data + (coded_size.GetArea() * 5 / 4);
      frame->no_longer_needed_cb_ = no_longer_needed_cb;
      return frame;
    }
    default:
      NOTIMPLEMENTED();
      return NULL;
  }
}

#if defined(OS_POSIX)
// static
scoped_refptr<VideoFrame> VideoFrame::WrapExternalDmabufs(
    Format format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    const std::vector<int> dmabuf_fds,
    base::TimeDelta timestamp,
    const base::Closure& no_longer_needed_cb) {
  if (!IsValidConfig(format, coded_size, visible_rect, natural_size))
    return NULL;

  if (dmabuf_fds.size() != NumPlanes(format)) {
    LOG(FATAL) << "Not enough dmabuf fds provided!";
    return NULL;
  }

  scoped_refptr<VideoFrame> frame(
      new VideoFrame(format,
                     coded_size,
                     visible_rect,
                     natural_size,
                     scoped_ptr<gpu::MailboxHolder>(),
                     timestamp,
                     false));

  for (size_t i = 0; i < dmabuf_fds.size(); ++i) {
    int duped_fd = HANDLE_EINTR(dup(dmabuf_fds[i]));
    if (duped_fd == -1) {
      // The already-duped in previous iterations fds will be closed when
      // the partially-created frame drops out of scope here.
      DLOG(ERROR) << "Failed duplicating a dmabuf fd";
      return NULL;
    }

    frame->dmabuf_fds_[i].reset(duped_fd);
    // Data is accessible only via fds.
    frame->data_[i] = NULL;
    frame->strides_[i] = 0;
  }

  frame->no_longer_needed_cb_ = no_longer_needed_cb;
  return frame;
}
#endif

// static
scoped_refptr<VideoFrame> VideoFrame::WrapExternalYuvData(
    Format format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    int32 y_stride,
    int32 u_stride,
    int32 v_stride,
    uint8* y_data,
    uint8* u_data,
    uint8* v_data,
    base::TimeDelta timestamp,
    const base::Closure& no_longer_needed_cb) {
  if (!IsValidConfig(format, coded_size, visible_rect, natural_size))
    return NULL;

  scoped_refptr<VideoFrame> frame(
      new VideoFrame(format,
                     coded_size,
                     visible_rect,
                     natural_size,
                     scoped_ptr<gpu::MailboxHolder>(),
                     timestamp,
                     false));
  frame->strides_[kYPlane] = y_stride;
  frame->strides_[kUPlane] = u_stride;
  frame->strides_[kVPlane] = v_stride;
  frame->data_[kYPlane] = y_data;
  frame->data_[kUPlane] = u_data;
  frame->data_[kVPlane] = v_data;
  frame->no_longer_needed_cb_ = no_longer_needed_cb;
  return frame;
}

// static
scoped_refptr<VideoFrame> VideoFrame::WrapVideoFrame(
      const scoped_refptr<VideoFrame>& frame,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      const base::Closure& no_longer_needed_cb) {
  // NATIVE_TEXTURE frames need mailbox info propagated, and there's no support
  // for that here yet, see http://crbug/362521.
  CHECK(frame->format() != NATIVE_TEXTURE);

  DCHECK(frame->visible_rect().Contains(visible_rect));
  scoped_refptr<VideoFrame> wrapped_frame(
      new VideoFrame(frame->format(),
                     frame->coded_size(),
                     visible_rect,
                     natural_size,
                     scoped_ptr<gpu::MailboxHolder>(),
                     frame->timestamp(),
                     frame->end_of_stream()));

  for (size_t i = 0; i < NumPlanes(frame->format()); ++i) {
    wrapped_frame->strides_[i] = frame->stride(i);
    wrapped_frame->data_[i] = frame->data(i);
  }

  wrapped_frame->no_longer_needed_cb_ = no_longer_needed_cb;
  return wrapped_frame;
}

// static
scoped_refptr<VideoFrame> VideoFrame::CreateEOSFrame() {
  return new VideoFrame(VideoFrame::UNKNOWN,
                        gfx::Size(),
                        gfx::Rect(),
                        gfx::Size(),
                        scoped_ptr<gpu::MailboxHolder>(),
                        kNoTimestamp(),
                        true);
}

// static
scoped_refptr<VideoFrame> VideoFrame::CreateColorFrame(
    const gfx::Size& size,
    uint8 y, uint8 u, uint8 v,
    base::TimeDelta timestamp) {
  scoped_refptr<VideoFrame> frame = VideoFrame::CreateFrame(
      VideoFrame::YV12, size, gfx::Rect(size), size, timestamp);
  FillYUV(frame.get(), y, u, v);
  return frame;
}

// static
scoped_refptr<VideoFrame> VideoFrame::CreateBlackFrame(const gfx::Size& size) {
  const uint8 kBlackY = 0x00;
  const uint8 kBlackUV = 0x80;
  const base::TimeDelta kZero;
  return CreateColorFrame(size, kBlackY, kBlackUV, kBlackUV, kZero);
}

#if defined(VIDEO_HOLE)
// This block and other blocks wrapped around #if defined(VIDEO_HOLE) is not
// maintained by the general compositor team. Please contact the following
// people instead:
//
// wonsik@chromium.org
// ycheo@chromium.org

// static
scoped_refptr<VideoFrame> VideoFrame::CreateHoleFrame(
    const gfx::Size& size) {
  DCHECK(IsValidConfig(VideoFrame::HOLE, size, gfx::Rect(size), size));
  scoped_refptr<VideoFrame> frame(
      new VideoFrame(VideoFrame::HOLE,
                     size,
                     gfx::Rect(size),
                     size,
                     scoped_ptr<gpu::MailboxHolder>(),
                     base::TimeDelta(),
                     false));
  return frame;
}
#endif  // defined(VIDEO_HOLE)

// static
size_t VideoFrame::NumPlanes(Format format) {
  switch (format) {
    case VideoFrame::NATIVE_TEXTURE:
#if defined(VIDEO_HOLE)
    case VideoFrame::HOLE:
#endif  // defined(VIDEO_HOLE)
      return 0;
    case VideoFrame::NV12:
      return 2;
    case VideoFrame::YV12:
    case VideoFrame::YV16:
    case VideoFrame::I420:
    case VideoFrame::YV12J:
    case VideoFrame::YV24:
      return 3;
    case VideoFrame::YV12A:
      return 4;
    case VideoFrame::UNKNOWN:
      break;
  }
  NOTREACHED() << "Unsupported video frame format: " << format;
  return 0;
}


// static
size_t VideoFrame::AllocationSize(Format format, const gfx::Size& coded_size) {
  size_t total = 0;
  for (size_t i = 0; i < NumPlanes(format); ++i)
    total += PlaneAllocationSize(format, i, coded_size);
  return total;
}

// static
gfx::Size VideoFrame::PlaneSize(Format format,
                                size_t plane,
                                const gfx::Size& coded_size) {
  // Align to multiple-of-two size overall. This ensures that non-subsampled
  // planes can be addressed by pixel with the same scaling as the subsampled
  // planes.
  const int width = RoundUp(coded_size.width(), 2);
  const int height = RoundUp(coded_size.height(), 2);
  switch (format) {
    case VideoFrame::YV24:
      switch (plane) {
        case VideoFrame::kYPlane:
        case VideoFrame::kUPlane:
        case VideoFrame::kVPlane:
          return gfx::Size(width, height);
        default:
          break;
      }
      break;
    case VideoFrame::YV12:
    case VideoFrame::YV12J:
    case VideoFrame::I420:
      switch (plane) {
        case VideoFrame::kYPlane:
          return gfx::Size(width, height);
        case VideoFrame::kUPlane:
        case VideoFrame::kVPlane:
          return gfx::Size(width / 2, height / 2);
        default:
          break;
      }
      break;
    case VideoFrame::YV12A:
      switch (plane) {
        case VideoFrame::kYPlane:
        case VideoFrame::kAPlane:
          return gfx::Size(width, height);
        case VideoFrame::kUPlane:
        case VideoFrame::kVPlane:
          return gfx::Size(width / 2, height / 2);
        default:
          break;
      }
      break;
    case VideoFrame::YV16:
      switch (plane) {
        case VideoFrame::kYPlane:
          return gfx::Size(width, height);
        case VideoFrame::kUPlane:
        case VideoFrame::kVPlane:
          return gfx::Size(width / 2, height);
        default:
          break;
      }
      break;
    case VideoFrame::NV12:
      switch (plane) {
        case VideoFrame::kYPlane:
          return gfx::Size(width, height);
        case VideoFrame::kUVPlane:
          return gfx::Size(width, height / 2);
        default:
          break;
      }
      break;
    case VideoFrame::UNKNOWN:
    case VideoFrame::NATIVE_TEXTURE:
#if defined(VIDEO_HOLE)
    case VideoFrame::HOLE:
#endif  // defined(VIDEO_HOLE)
      break;
  }
  NOTREACHED() << "Unsupported video frame format/plane: "
               << format << "/" << plane;
  return gfx::Size();
}

size_t VideoFrame::PlaneAllocationSize(Format format,
                                       size_t plane,
                                       const gfx::Size& coded_size) {
  // VideoFrame formats are (so far) all YUV and 1 byte per sample.
  return PlaneSize(format, plane, coded_size).GetArea();
}

// static
int VideoFrame::PlaneHorizontalBitsPerPixel(Format format, size_t plane) {
  switch (format) {
    case VideoFrame::YV24:
      switch (plane) {
        case kYPlane:
        case kUPlane:
        case kVPlane:
          return 8;
        default:
          break;
      }
      break;
    case VideoFrame::YV12:
    case VideoFrame::YV16:
    case VideoFrame::I420:
    case VideoFrame::YV12J:
      switch (plane) {
        case kYPlane:
          return 8;
        case kUPlane:
        case kVPlane:
          return 2;
        default:
          break;
      }
      break;
    case VideoFrame::YV12A:
      switch (plane) {
        case kYPlane:
        case kAPlane:
          return 8;
        case kUPlane:
        case kVPlane:
          return 2;
        default:
          break;
      }
      break;
    case VideoFrame::NV12:
      switch (plane) {
        case kYPlane:
          return 8;
        case kUVPlane:
          return 4;
        default:
          break;
      }
      break;
    case VideoFrame::UNKNOWN:
#if defined(VIDEO_HOLE)
    case VideoFrame::HOLE:
#endif  // defined(VIDEO_HOLE)
    case VideoFrame::NATIVE_TEXTURE:
      break;
  }
  NOTREACHED() << "Unsupported video frame format/plane: "
               << format << "/" << plane;
  return 0;
}

// Release data allocated by AllocateYUV().
static void ReleaseData(uint8* data) {
  DCHECK(data);
  base::AlignedFree(data);
}

void VideoFrame::AllocateYUV() {
  DCHECK(format_ == VideoFrame::YV12 || format_ == VideoFrame::YV16 ||
         format_ == VideoFrame::YV12A || format_ == VideoFrame::I420 ||
         format_ == VideoFrame::YV12J || format_ == VideoFrame::YV24);
  // Align Y rows at least at 16 byte boundaries.  The stride for both
  // YV12 and YV16 is 1/2 of the stride of Y.  For YV12, every row of bytes for
  // U and V applies to two rows of Y (one byte of UV for 4 bytes of Y), so in
  // the case of YV12 the strides are identical for the same width surface, but
  // the number of bytes allocated for YV12 is 1/2 the amount for U & V as
  // YV16. We also round the height of the surface allocated to be an even
  // number to avoid any potential of faulting by code that attempts to access
  // the Y values of the final row, but assumes that the last row of U & V
  // applies to a full two rows of Y. YV12A is the same as YV12, but with an
  // additional alpha plane that has the same size and alignment as the Y plane.
  size_t y_stride = RoundUp(row_bytes(VideoFrame::kYPlane),
                            kFrameSizeAlignment);
  size_t uv_stride = RoundUp(row_bytes(VideoFrame::kUPlane),
                             kFrameSizeAlignment);

  // The *2 here is because some formats (e.g. h264) allow interlaced coding,
  // and then the size needs to be a multiple of two macroblocks (vertically).
  // See libavcodec/utils.c:avcodec_align_dimensions2().
  size_t y_height = RoundUp(coded_size_.height(), kFrameSizeAlignment * 2);
  size_t uv_height =
      (format_ == VideoFrame::YV12 || format_ == VideoFrame::YV12A ||
       format_ == VideoFrame::I420)
          ? y_height / 2
          : y_height;
  size_t y_bytes = y_height * y_stride;
  size_t uv_bytes = uv_height * uv_stride;
  size_t a_bytes = format_ == VideoFrame::YV12A ? y_bytes : 0;

  // The extra line of UV being allocated is because h264 chroma MC
  // overreads by one line in some cases, see libavcodec/utils.c:
  // avcodec_align_dimensions2() and libavcodec/x86/h264_chromamc.asm:
  // put_h264_chroma_mc4_ssse3().
  uint8* data = reinterpret_cast<uint8*>(
      base::AlignedAlloc(
          y_bytes + (uv_bytes * 2 + uv_stride) + a_bytes + kFrameSizePadding,
          kFrameAddressAlignment));
  no_longer_needed_cb_ = base::Bind(&ReleaseData, data);
  COMPILE_ASSERT(0 == VideoFrame::kYPlane, y_plane_data_must_be_index_0);
  data_[VideoFrame::kYPlane] = data;
  data_[VideoFrame::kUPlane] = data + y_bytes;
  data_[VideoFrame::kVPlane] = data + y_bytes + uv_bytes;
  strides_[VideoFrame::kYPlane] = y_stride;
  strides_[VideoFrame::kUPlane] = uv_stride;
  strides_[VideoFrame::kVPlane] = uv_stride;
  if (format_ == YV12A) {
    data_[VideoFrame::kAPlane] = data + y_bytes + (2 * uv_bytes);
    strides_[VideoFrame::kAPlane] = y_stride;
  }
}

VideoFrame::VideoFrame(VideoFrame::Format format,
                       const gfx::Size& coded_size,
                       const gfx::Rect& visible_rect,
                       const gfx::Size& natural_size,
                       scoped_ptr<gpu::MailboxHolder> mailbox_holder,
                       base::TimeDelta timestamp,
                       bool end_of_stream)
    : format_(format),
      coded_size_(coded_size),
      visible_rect_(visible_rect),
      natural_size_(natural_size),
      mailbox_holder_(mailbox_holder.Pass()),
      shared_memory_handle_(base::SharedMemory::NULLHandle()),
      timestamp_(timestamp),
      end_of_stream_(end_of_stream) {
  DCHECK(IsValidConfig(format_, coded_size_, visible_rect_, natural_size_));

  memset(&strides_, 0, sizeof(strides_));
  memset(&data_, 0, sizeof(data_));
}

VideoFrame::~VideoFrame() {
  if (!mailbox_holder_release_cb_.is_null()) {
    std::vector<uint32> release_sync_points;
    {
      base::AutoLock locker(release_sync_point_lock_);
      release_sync_points_.swap(release_sync_points);
    }
    base::ResetAndReturn(&mailbox_holder_release_cb_).Run(release_sync_points);
  }
  if (!no_longer_needed_cb_.is_null())
    base::ResetAndReturn(&no_longer_needed_cb_).Run();
}

bool VideoFrame::IsValidPlane(size_t plane) const {
  return (plane < NumPlanes(format_));
}

int VideoFrame::stride(size_t plane) const {
  DCHECK(IsValidPlane(plane));
  return strides_[plane];
}

int VideoFrame::row_bytes(size_t plane) const {
  DCHECK(IsValidPlane(plane));
  int width = coded_size_.width();
  switch (format_) {
    case VideoFrame::YV24:
      switch (plane) {
        case kYPlane:
        case kUPlane:
        case kVPlane:
          return width;
        default:
          break;
      }
      break;
    case VideoFrame::YV12:
    case VideoFrame::YV16:
    case VideoFrame::I420:
    case VideoFrame::YV12J:
      switch (plane) {
        case kYPlane:
          return width;
        case kUPlane:
        case kVPlane:
          return RoundUp(width, 2) / 2;
        default:
          break;
      }
      break;
    case VideoFrame::YV12A:
      switch (plane) {
        case kYPlane:
        case kAPlane:
          return width;
        case kUPlane:
        case kVPlane:
          return RoundUp(width, 2) / 2;
        default:
          break;
      }
      break;
    case VideoFrame::NV12:
      switch (plane) {
        case kYPlane:
        case kUVPlane:
          return width;
        default:
          break;
      }
      break;
    case VideoFrame::UNKNOWN:
#if defined(VIDEO_HOLE)
    case VideoFrame::HOLE:
#endif  // defined(VIDEO_HOLE)
    case VideoFrame::NATIVE_TEXTURE:
      break;
  }
  NOTREACHED() << "Unsupported video frame format/plane: "
               << format_ << "/" << plane;
  return 0;
}

int VideoFrame::rows(size_t plane) const {
  DCHECK(IsValidPlane(plane));
  int height = coded_size_.height();
  switch (format_) {
    case VideoFrame::YV24:
    case VideoFrame::YV16:
      switch (plane) {
        case kYPlane:
        case kUPlane:
        case kVPlane:
          return height;
        default:
          break;
      }
      break;
    case VideoFrame::YV12:
    case VideoFrame::YV12J:
    case VideoFrame::I420:
      switch (plane) {
        case kYPlane:
          return height;
        case kUPlane:
        case kVPlane:
          return RoundUp(height, 2) / 2;
        default:
          break;
      }
      break;
    case VideoFrame::YV12A:
      switch (plane) {
        case kYPlane:
        case kAPlane:
          return height;
        case kUPlane:
        case kVPlane:
          return RoundUp(height, 2) / 2;
        default:
          break;
      }
      break;
    case VideoFrame::NV12:
      switch (plane) {
        case kYPlane:
          return height;
        case kUVPlane:
          return RoundUp(height, 2) / 2;
        default:
          break;
      }
      break;
    case VideoFrame::UNKNOWN:
#if defined(VIDEO_HOLE)
    case VideoFrame::HOLE:
#endif  // defined(VIDEO_HOLE)
    case VideoFrame::NATIVE_TEXTURE:
      break;
  }
  NOTREACHED() << "Unsupported video frame format/plane: "
               << format_ << "/" << plane;
  return 0;
}

uint8* VideoFrame::data(size_t plane) const {
  DCHECK(IsValidPlane(plane));
  return data_[plane];
}

const gpu::MailboxHolder* VideoFrame::mailbox_holder() const {
  DCHECK_EQ(format_, NATIVE_TEXTURE);
  return mailbox_holder_.get();
}

base::SharedMemoryHandle VideoFrame::shared_memory_handle() const {
  return shared_memory_handle_;
}

void VideoFrame::AppendReleaseSyncPoint(uint32 sync_point) {
  DCHECK_EQ(format_, NATIVE_TEXTURE);
  if (!sync_point)
    return;
  base::AutoLock locker(release_sync_point_lock_);
  release_sync_points_.push_back(sync_point);
}

#if defined(OS_POSIX)
int VideoFrame::dmabuf_fd(size_t plane) const {
  return dmabuf_fds_[plane].get();
}
#endif

void VideoFrame::HashFrameForTesting(base::MD5Context* context) {
  for (int plane = 0; plane < kMaxPlanes; ++plane) {
    if (!IsValidPlane(plane))
      break;
    for (int row = 0; row < rows(plane); ++row) {
      base::MD5Update(context, base::StringPiece(
          reinterpret_cast<char*>(data(plane) + stride(plane) * row),
          row_bytes(plane)));
    }
  }
}

}  // namespace media
