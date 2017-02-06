// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_PICTURE_H_
#define MEDIA_VIDEO_PICTURE_H_

#include <stdint.h>

#include <vector>

#include "gpu/command_buffer/common/mailbox.h"
#include "media/base/media_export.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// A picture buffer that is composed of a GLES2 texture.
// This is the media-namespace equivalent of PP_PictureBuffer_Dev.
class MEDIA_EXPORT PictureBuffer {
 public:
  using TextureIds = std::vector<uint32_t>;

  PictureBuffer(int32_t id, gfx::Size size, const TextureIds& texture_ids);
  PictureBuffer(int32_t id,
                gfx::Size size,
                const TextureIds& texture_ids,
                const TextureIds& internal_texture_ids);
  PictureBuffer(int32_t id,
                gfx::Size size,
                const TextureIds& texture_ids,
                const std::vector<gpu::Mailbox>& texture_mailboxes);
  PictureBuffer(const PictureBuffer& other);
  ~PictureBuffer();

  // Returns the client-specified id of the buffer.
  int32_t id() const { return id_; }

  // Returns the size of the buffer.
  gfx::Size size() const {
    return size_;
  }
  void set_size(const gfx::Size& size) { size_ = size; }

  // Returns the id of the texture.
  // NOTE: The texture id in the renderer process corresponds to a different
  // texture id in the GPU process.
  const TextureIds& texture_ids() const { return texture_ids_; }

  const TextureIds& internal_texture_ids() const {
    return internal_texture_ids_;
  }

  const gpu::Mailbox& texture_mailbox(size_t plane) const {
    return texture_mailboxes_[plane];
  }

 private:
  int32_t id_;
  gfx::Size size_;
  TextureIds texture_ids_;
  TextureIds internal_texture_ids_;
  std::vector<gpu::Mailbox> texture_mailboxes_;
};

// A decoded picture frame.
// This is the media-namespace equivalent of PP_Picture_Dev.
class MEDIA_EXPORT Picture {
 public:
  // Defaults |size_changed_| to false. Size changed is currently only used
  // by AVDA and is set via set_size_changd().
  Picture(int32_t picture_buffer_id,
          int32_t bitstream_buffer_id,
          const gfx::Rect& visible_rect,
          bool allow_overlay);

  // Returns the id of the picture buffer where this picture is contained.
  int32_t picture_buffer_id() const { return picture_buffer_id_; }

  // Returns the id of the bitstream buffer from which this frame was decoded.
  int32_t bitstream_buffer_id() const { return bitstream_buffer_id_; }

  void set_bitstream_buffer_id(int32_t bitstream_buffer_id) {
    bitstream_buffer_id_ = bitstream_buffer_id;
  }

  // Returns the visible rectangle of the picture. Its size may be smaller
  // than the size of the PictureBuffer, as it is the only visible part of the
  // Picture contained in the PictureBuffer.
  gfx::Rect visible_rect() const { return visible_rect_; }

  bool allow_overlay() const { return allow_overlay_; }

  // Returns true when the VDA has adjusted the resolution of this Picture
  // without requesting new PictureBuffers. GpuVideoDecoder should read this
  // as a signal to update the size of the corresponding PicutreBuffer using
  // visible_rect() upon receiving this Picture from a VDA.
  bool size_changed() const { return size_changed_; };

  void set_size_changed(bool size_changed) { size_changed_ = size_changed; }

 private:
  int32_t picture_buffer_id_;
  int32_t bitstream_buffer_id_;
  gfx::Rect visible_rect_;
  bool allow_overlay_;
  bool size_changed_;
};

}  // namespace media

#endif  // MEDIA_VIDEO_PICTURE_H_
