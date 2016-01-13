// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_TEXTURE_UPLOADER_H_
#define CC_RESOURCES_TEXTURE_UPLOADER_H_

#include <set>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/base/scoped_ptr_deque.h"
#include "cc/resources/resource_provider.h"

namespace gfx {
class Rect;
class Size;
class Vector2d;
}

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace cc {

class CC_EXPORT TextureUploader {
 public:
  static scoped_ptr<TextureUploader> Create(gpu::gles2::GLES2Interface* gl) {
    return make_scoped_ptr(new TextureUploader(gl));
  }
  ~TextureUploader();

  size_t NumBlockingUploads();
  void MarkPendingUploadsAsNonBlocking();
  double EstimatedTexturesPerSecond();

  // Let content_rect be a rectangle, and let content_rect be a sub-rectangle of
  // content_rect, expressed in the same coordinate system as content_rect. Let
  // image be a buffer for content_rect. This function will copy the region
  // corresponding to source_rect to dest_offset in this sub-image.
  void Upload(const uint8* image,
              const gfx::Rect& content_rect,
              const gfx::Rect& source_rect,
              gfx::Vector2d dest_offset,
              ResourceFormat format,
              const gfx::Size& size);

  void Flush();
  void ReleaseCachedQueries();

 private:
  class Query {
   public:
    static scoped_ptr<Query> Create(gpu::gles2::GLES2Interface* gl) {
      return make_scoped_ptr(new Query(gl));
    }

    virtual ~Query();

    void Begin();
    void End();
    bool IsPending();
    unsigned Value();
    size_t TexturesUploaded();
    void mark_as_non_blocking() { is_non_blocking_ = true; }
    bool is_non_blocking() const { return is_non_blocking_; }

   private:
    explicit Query(gpu::gles2::GLES2Interface* gl);

    gpu::gles2::GLES2Interface* gl_;
    unsigned query_id_;
    unsigned value_;
    bool has_value_;
    bool is_non_blocking_;

    DISALLOW_COPY_AND_ASSIGN(Query);
  };

  explicit TextureUploader(gpu::gles2::GLES2Interface* gl);

  void BeginQuery();
  void EndQuery();
  void ProcessQueries();

  void UploadWithTexSubImage(const uint8* image,
                             const gfx::Rect& image_rect,
                             const gfx::Rect& source_rect,
                             gfx::Vector2d dest_offset,
                             ResourceFormat format);
  void UploadWithMapTexSubImage(const uint8* image,
                                const gfx::Rect& image_rect,
                                const gfx::Rect& source_rect,
                                gfx::Vector2d dest_offset,
                                ResourceFormat format);
  void UploadWithTexImageETC1(const uint8* image, const gfx::Size& size);

  gpu::gles2::GLES2Interface* gl_;
  ScopedPtrDeque<Query> pending_queries_;
  ScopedPtrDeque<Query> available_queries_;
  std::multiset<double> textures_per_second_history_;
  size_t num_blocking_texture_uploads_;

  size_t sub_image_size_;
  scoped_ptr<uint8[]> sub_image_;

  size_t num_texture_uploads_since_last_flush_;

  DISALLOW_COPY_AND_ASSIGN(TextureUploader);
};

}  // namespace cc

#endif  // CC_RESOURCES_TEXTURE_UPLOADER_H_
