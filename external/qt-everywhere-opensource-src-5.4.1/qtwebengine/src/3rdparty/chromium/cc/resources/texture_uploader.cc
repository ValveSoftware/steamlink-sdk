// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/texture_uploader.h"

#include <algorithm>
#include <vector>

#include "base/debug/trace_event.h"
#include "base/metrics/histogram.h"
#include "cc/base/util.h"
#include "cc/resources/prioritized_resource.h"
#include "cc/resources/resource.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/vector2d.h"

using gpu::gles2::GLES2Interface;

namespace {

// How many previous uploads to use when predicting future throughput.
static const size_t kUploadHistorySizeMax = 1000;
static const size_t kUploadHistorySizeInitial = 100;

// Global estimated number of textures per second to maintain estimates across
// subsequent instances of TextureUploader.
// More than one thread will not access this variable, so we do not need to
// synchronize access.
static const double kDefaultEstimatedTexturesPerSecond = 48.0 * 60.0;

// Flush interval when performing texture uploads.
static const size_t kTextureUploadFlushPeriod = 4;

}  // anonymous namespace

namespace cc {

TextureUploader::Query::Query(GLES2Interface* gl)
    : gl_(gl),
      query_id_(0),
      value_(0),
      has_value_(false),
      is_non_blocking_(false) {
  gl_->GenQueriesEXT(1, &query_id_);
}

TextureUploader::Query::~Query() { gl_->DeleteQueriesEXT(1, &query_id_); }

void TextureUploader::Query::Begin() {
  has_value_ = false;
  is_non_blocking_ = false;
  gl_->BeginQueryEXT(GL_COMMANDS_ISSUED_CHROMIUM, query_id_);
}

void TextureUploader::Query::End() {
  gl_->EndQueryEXT(GL_COMMANDS_ISSUED_CHROMIUM);
}

bool TextureUploader::Query::IsPending() {
  unsigned available = 1;
  gl_->GetQueryObjectuivEXT(
      query_id_, GL_QUERY_RESULT_AVAILABLE_EXT, &available);
  return !available;
}

unsigned TextureUploader::Query::Value() {
  if (!has_value_) {
    gl_->GetQueryObjectuivEXT(query_id_, GL_QUERY_RESULT_EXT, &value_);
    has_value_ = true;
  }
  return value_;
}

TextureUploader::TextureUploader(GLES2Interface* gl)
    : gl_(gl),
      num_blocking_texture_uploads_(0),
      sub_image_size_(0),
      num_texture_uploads_since_last_flush_(0) {
  for (size_t i = kUploadHistorySizeInitial; i > 0; i--)
    textures_per_second_history_.insert(kDefaultEstimatedTexturesPerSecond);
}

TextureUploader::~TextureUploader() {}

size_t TextureUploader::NumBlockingUploads() {
  ProcessQueries();
  return num_blocking_texture_uploads_;
}

void TextureUploader::MarkPendingUploadsAsNonBlocking() {
  for (ScopedPtrDeque<Query>::iterator it = pending_queries_.begin();
       it != pending_queries_.end();
       ++it) {
    if ((*it)->is_non_blocking())
      continue;

    num_blocking_texture_uploads_--;
    (*it)->mark_as_non_blocking();
  }

  DCHECK(!num_blocking_texture_uploads_);
}

double TextureUploader::EstimatedTexturesPerSecond() {
  ProcessQueries();

  // Use the median as our estimate.
  std::multiset<double>::iterator median = textures_per_second_history_.begin();
  std::advance(median, textures_per_second_history_.size() / 2);
  return *median;
}

void TextureUploader::BeginQuery() {
  if (available_queries_.empty())
    available_queries_.push_back(Query::Create(gl_));

  available_queries_.front()->Begin();
}

void TextureUploader::EndQuery() {
  available_queries_.front()->End();
  pending_queries_.push_back(available_queries_.take_front());
  num_blocking_texture_uploads_++;
}

void TextureUploader::Upload(const uint8* image,
                             const gfx::Rect& image_rect,
                             const gfx::Rect& source_rect,
                             gfx::Vector2d dest_offset,
                             ResourceFormat format,
                             const gfx::Size& size) {
  CHECK(image_rect.Contains(source_rect));

  bool is_full_upload = dest_offset.IsZero() && source_rect.size() == size;

  if (is_full_upload)
    BeginQuery();

  if (format == ETC1) {
    // ETC1 does not support subimage uploads.
    DCHECK(is_full_upload);
    UploadWithTexImageETC1(image, size);
  } else {
    UploadWithMapTexSubImage(
        image, image_rect, source_rect, dest_offset, format);
  }

  if (is_full_upload)
    EndQuery();

  num_texture_uploads_since_last_flush_++;
  if (num_texture_uploads_since_last_flush_ >= kTextureUploadFlushPeriod)
    Flush();
}

void TextureUploader::Flush() {
  if (!num_texture_uploads_since_last_flush_)
    return;

  gl_->ShallowFlushCHROMIUM();

  num_texture_uploads_since_last_flush_ = 0;
}

void TextureUploader::ReleaseCachedQueries() {
  ProcessQueries();
  available_queries_.clear();
}

void TextureUploader::UploadWithTexSubImage(const uint8* image,
                                            const gfx::Rect& image_rect,
                                            const gfx::Rect& source_rect,
                                            gfx::Vector2d dest_offset,
                                            ResourceFormat format) {
  TRACE_EVENT0("cc", "TextureUploader::UploadWithTexSubImage");

  // Early-out if this is a no-op, and assert that |image| be valid if this is
  // not a no-op.
  if (source_rect.IsEmpty())
    return;
  DCHECK(image);

  // Offset from image-rect to source-rect.
  gfx::Vector2d offset(source_rect.origin() - image_rect.origin());

  const uint8* pixel_source;
  unsigned bytes_per_pixel = BitsPerPixel(format) / 8;
  // Use 4-byte row alignment (OpenGL default) for upload performance.
  // Assuming that GL_UNPACK_ALIGNMENT has not changed from default.
  unsigned upload_image_stride =
      RoundUp(bytes_per_pixel * source_rect.width(), 4u);

  if (upload_image_stride == image_rect.width() * bytes_per_pixel &&
      !offset.x()) {
    pixel_source = &image[image_rect.width() * bytes_per_pixel * offset.y()];
  } else {
    size_t needed_size = upload_image_stride * source_rect.height();
    if (sub_image_size_ < needed_size) {
      sub_image_.reset(new uint8[needed_size]);
      sub_image_size_ = needed_size;
    }
    // Strides not equal, so do a row-by-row memcpy from the
    // paint results into a temp buffer for uploading.
    for (int row = 0; row < source_rect.height(); ++row)
      memcpy(&sub_image_[upload_image_stride * row],
             &image[bytes_per_pixel *
                    (offset.x() + (offset.y() + row) * image_rect.width())],
             source_rect.width() * bytes_per_pixel);

    pixel_source = &sub_image_[0];
  }

  gl_->TexSubImage2D(GL_TEXTURE_2D,
                     0,
                     dest_offset.x(),
                     dest_offset.y(),
                     source_rect.width(),
                     source_rect.height(),
                     GLDataFormat(format),
                     GLDataType(format),
                     pixel_source);
}

void TextureUploader::UploadWithMapTexSubImage(const uint8* image,
                                               const gfx::Rect& image_rect,
                                               const gfx::Rect& source_rect,
                                               gfx::Vector2d dest_offset,
                                               ResourceFormat format) {
  TRACE_EVENT0("cc", "TextureUploader::UploadWithMapTexSubImage");

  // Early-out if this is a no-op, and assert that |image| be valid if this is
  // not a no-op.
  if (source_rect.IsEmpty())
    return;
  DCHECK(image);
  // Compressed textures have no implementation of mapTexSubImage.
  DCHECK_NE(ETC1, format);

  // Offset from image-rect to source-rect.
  gfx::Vector2d offset(source_rect.origin() - image_rect.origin());

  unsigned bytes_per_pixel = BitsPerPixel(format) / 8;
  // Use 4-byte row alignment (OpenGL default) for upload performance.
  // Assuming that GL_UNPACK_ALIGNMENT has not changed from default.
  unsigned upload_image_stride =
      RoundUp(bytes_per_pixel * source_rect.width(), 4u);

  // Upload tile data via a mapped transfer buffer
  uint8* pixel_dest =
      static_cast<uint8*>(gl_->MapTexSubImage2DCHROMIUM(GL_TEXTURE_2D,
                                                        0,
                                                        dest_offset.x(),
                                                        dest_offset.y(),
                                                        source_rect.width(),
                                                        source_rect.height(),
                                                        GLDataFormat(format),
                                                        GLDataType(format),
                                                        GL_WRITE_ONLY));

  if (!pixel_dest) {
    UploadWithTexSubImage(image, image_rect, source_rect, dest_offset, format);
    return;
  }

  if (upload_image_stride == image_rect.width() * bytes_per_pixel &&
      !offset.x()) {
    memcpy(pixel_dest,
           &image[image_rect.width() * bytes_per_pixel * offset.y()],
           source_rect.height() * image_rect.width() * bytes_per_pixel);
  } else {
    // Strides not equal, so do a row-by-row memcpy from the
    // paint results into the pixel_dest.
    for (int row = 0; row < source_rect.height(); ++row) {
      memcpy(&pixel_dest[upload_image_stride * row],
             &image[bytes_per_pixel *
                    (offset.x() + (offset.y() + row) * image_rect.width())],
             source_rect.width() * bytes_per_pixel);
    }
  }

  gl_->UnmapTexSubImage2DCHROMIUM(pixel_dest);
}

void TextureUploader::UploadWithTexImageETC1(const uint8* image,
                                             const gfx::Size& size) {
  TRACE_EVENT0("cc", "TextureUploader::UploadWithTexImageETC1");
  DCHECK_EQ(0, size.width() % 4);
  DCHECK_EQ(0, size.height() % 4);

  gl_->CompressedTexImage2D(GL_TEXTURE_2D,
                            0,
                            GLInternalFormat(ETC1),
                            size.width(),
                            size.height(),
                            0,
                            Resource::MemorySizeBytes(size, ETC1),
                            image);
}

void TextureUploader::ProcessQueries() {
  while (!pending_queries_.empty()) {
    if (pending_queries_.front()->IsPending())
      break;

    unsigned us_elapsed = pending_queries_.front()->Value();
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Renderer4.TextureGpuUploadTimeUS", us_elapsed, 0, 100000, 50);

    // Clamp the queries to saner values in case the queries fail.
    us_elapsed = std::max(1u, us_elapsed);
    us_elapsed = std::min(15000u, us_elapsed);

    if (!pending_queries_.front()->is_non_blocking())
      num_blocking_texture_uploads_--;

    // Remove the min and max value from our history and insert the new one.
    double textures_per_second = 1.0 / (us_elapsed * 1e-6);
    if (textures_per_second_history_.size() >= kUploadHistorySizeMax) {
      textures_per_second_history_.erase(textures_per_second_history_.begin());
      textures_per_second_history_.erase(--textures_per_second_history_.end());
    }
    textures_per_second_history_.insert(textures_per_second);

    available_queries_.push_back(pending_queries_.take_front());
  }
}

}  // namespace cc
