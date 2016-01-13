// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/resource_provider.h"

#include <algorithm>
#include <limits>

#include "base/containers/hash_tables.h"
#include "base/debug/trace_event.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "cc/base/util.h"
#include "cc/output/gl_renderer.h"  // For the GLC() macro.
#include "cc/resources/platform_color.h"
#include "cc/resources/returned_resource.h"
#include "cc/resources/shared_bitmap_manager.h"
#include "cc/resources/texture_uploader.h"
#include "cc/resources/transferable_resource.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/SkGpuDevice.h"
#include "ui/gfx/frame_time.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/vector2d.h"

using gpu::gles2::GLES2Interface;

namespace cc {

class IdAllocator {
 public:
  virtual ~IdAllocator() {}

  virtual GLuint NextId() = 0;

 protected:
  IdAllocator(GLES2Interface* gl, size_t id_allocation_chunk_size)
      : gl_(gl),
        id_allocation_chunk_size_(id_allocation_chunk_size),
        ids_(new GLuint[id_allocation_chunk_size]),
        next_id_index_(id_allocation_chunk_size) {
    DCHECK(id_allocation_chunk_size_);
  }

  GLES2Interface* gl_;
  const size_t id_allocation_chunk_size_;
  scoped_ptr<GLuint[]> ids_;
  size_t next_id_index_;
};

namespace {

// Measured in seconds.
const double kSoftwareUploadTickRate = 0.000250;
const double kTextureUploadTickRate = 0.004;

GLenum TextureToStorageFormat(ResourceFormat format) {
  GLenum storage_format = GL_RGBA8_OES;
  switch (format) {
    case RGBA_8888:
      break;
    case BGRA_8888:
      storage_format = GL_BGRA8_EXT;
      break;
    case RGBA_4444:
    case LUMINANCE_8:
    case RGB_565:
    case ETC1:
      NOTREACHED();
      break;
  }

  return storage_format;
}

bool IsFormatSupportedForStorage(ResourceFormat format) {
  switch (format) {
    case RGBA_8888:
    case BGRA_8888:
      return true;
    case RGBA_4444:
    case LUMINANCE_8:
    case RGB_565:
    case ETC1:
      return false;
  }
  return false;
}

GrPixelConfig ToGrPixelConfig(ResourceFormat format) {
  switch (format) {
    case RGBA_8888:
      return kRGBA_8888_GrPixelConfig;
    case BGRA_8888:
      return kBGRA_8888_GrPixelConfig;
    case RGBA_4444:
      return kRGBA_4444_GrPixelConfig;
    default:
      break;
  }
  DCHECK(false) << "Unsupported resource format.";
  return kSkia8888_GrPixelConfig;
}

class IdentityAllocator : public SkBitmap::Allocator {
 public:
  explicit IdentityAllocator(void* buffer) : buffer_(buffer) {}
  virtual bool allocPixelRef(SkBitmap* dst, SkColorTable*) OVERRIDE {
    dst->setPixels(buffer_);
    return true;
  }

 private:
  void* buffer_;
};

void CopyBitmap(const SkBitmap& src, uint8_t* dst, SkColorType dst_colorType) {
  SkBitmap dst_bitmap;
  IdentityAllocator allocator(dst);
  src.copyTo(&dst_bitmap, dst_colorType, &allocator);
  // TODO(kaanb): The GL pipeline assumes a 4-byte alignment for the
  // bitmap data. This check will be removed once crbug.com/293728 is fixed.
  CHECK_EQ(0u, dst_bitmap.rowBytes() % 4);
}

class ScopedSetActiveTexture {
 public:
  ScopedSetActiveTexture(GLES2Interface* gl, GLenum unit)
      : gl_(gl), unit_(unit) {
    DCHECK_EQ(GL_TEXTURE0, ResourceProvider::GetActiveTextureUnit(gl_));

    if (unit_ != GL_TEXTURE0)
      GLC(gl_, gl_->ActiveTexture(unit_));
  }

  ~ScopedSetActiveTexture() {
    // Active unit being GL_TEXTURE0 is effectively the ground state.
    if (unit_ != GL_TEXTURE0)
      GLC(gl_, gl_->ActiveTexture(GL_TEXTURE0));
  }

 private:
  GLES2Interface* gl_;
  GLenum unit_;
};

class TextureIdAllocator : public IdAllocator {
 public:
  TextureIdAllocator(GLES2Interface* gl,
                     size_t texture_id_allocation_chunk_size)
      : IdAllocator(gl, texture_id_allocation_chunk_size) {}
  virtual ~TextureIdAllocator() {
    gl_->DeleteTextures(id_allocation_chunk_size_ - next_id_index_,
                        ids_.get() + next_id_index_);
  }

  // Overridden from IdAllocator:
  virtual GLuint NextId() OVERRIDE {
    if (next_id_index_ == id_allocation_chunk_size_) {
      gl_->GenTextures(id_allocation_chunk_size_, ids_.get());
      next_id_index_ = 0;
    }

    return ids_[next_id_index_++];
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TextureIdAllocator);
};

class BufferIdAllocator : public IdAllocator {
 public:
  BufferIdAllocator(GLES2Interface* gl, size_t buffer_id_allocation_chunk_size)
      : IdAllocator(gl, buffer_id_allocation_chunk_size) {}
  virtual ~BufferIdAllocator() {
    gl_->DeleteBuffers(id_allocation_chunk_size_ - next_id_index_,
                       ids_.get() + next_id_index_);
  }

  // Overridden from IdAllocator:
  virtual GLuint NextId() OVERRIDE {
    if (next_id_index_ == id_allocation_chunk_size_) {
      gl_->GenBuffers(id_allocation_chunk_size_, ids_.get());
      next_id_index_ = 0;
    }

    return ids_[next_id_index_++];
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BufferIdAllocator);
};

// Generic fence implementation for query objects. Fence has passed when query
// result is available.
class QueryFence : public ResourceProvider::Fence {
 public:
  QueryFence(gpu::gles2::GLES2Interface* gl, unsigned query_id)
      : gl_(gl), query_id_(query_id) {}

  // Overridden from ResourceProvider::Fence:
  virtual bool HasPassed() OVERRIDE {
    unsigned available = 1;
    gl_->GetQueryObjectuivEXT(
        query_id_, GL_QUERY_RESULT_AVAILABLE_EXT, &available);
    return !!available;
  }

 private:
  virtual ~QueryFence() {}

  gpu::gles2::GLES2Interface* gl_;
  unsigned query_id_;

  DISALLOW_COPY_AND_ASSIGN(QueryFence);
};

}  // namespace

ResourceProvider::Resource::Resource()
    : child_id(0),
      gl_id(0),
      gl_pixel_buffer_id(0),
      gl_upload_query_id(0),
      gl_read_lock_query_id(0),
      pixels(NULL),
      lock_for_read_count(0),
      imported_count(0),
      exported_count(0),
      dirty_image(false),
      locked_for_write(false),
      lost(false),
      marked_for_deletion(false),
      pending_set_pixels(false),
      set_pixels_completion_forced(false),
      allocated(false),
      enable_read_lock_fences(false),
      has_shared_bitmap_id(false),
      allow_overlay(false),
      read_lock_fence(NULL),
      size(),
      origin(Internal),
      target(0),
      original_filter(0),
      filter(0),
      image_id(0),
      bound_image_id(0),
      texture_pool(0),
      wrap_mode(0),
      hint(TextureUsageAny),
      type(InvalidType),
      format(RGBA_8888),
      shared_bitmap(NULL) {}

ResourceProvider::Resource::~Resource() {}

ResourceProvider::Resource::Resource(GLuint texture_id,
                                     const gfx::Size& size,
                                     Origin origin,
                                     GLenum target,
                                     GLenum filter,
                                     GLenum texture_pool,
                                     GLint wrap_mode,
                                     TextureUsageHint hint,
                                     ResourceFormat format)
    : child_id(0),
      gl_id(texture_id),
      gl_pixel_buffer_id(0),
      gl_upload_query_id(0),
      gl_read_lock_query_id(0),
      pixels(NULL),
      lock_for_read_count(0),
      imported_count(0),
      exported_count(0),
      dirty_image(false),
      locked_for_write(false),
      lost(false),
      marked_for_deletion(false),
      pending_set_pixels(false),
      set_pixels_completion_forced(false),
      allocated(false),
      enable_read_lock_fences(false),
      has_shared_bitmap_id(false),
      allow_overlay(false),
      read_lock_fence(NULL),
      size(size),
      origin(origin),
      target(target),
      original_filter(filter),
      filter(filter),
      image_id(0),
      bound_image_id(0),
      texture_pool(texture_pool),
      wrap_mode(wrap_mode),
      hint(hint),
      type(GLTexture),
      format(format),
      shared_bitmap(NULL) {
  DCHECK(wrap_mode == GL_CLAMP_TO_EDGE || wrap_mode == GL_REPEAT);
  DCHECK_EQ(origin == Internal, !!texture_pool);
}

ResourceProvider::Resource::Resource(uint8_t* pixels,
                                     SharedBitmap* bitmap,
                                     const gfx::Size& size,
                                     Origin origin,
                                     GLenum filter,
                                     GLint wrap_mode)
    : child_id(0),
      gl_id(0),
      gl_pixel_buffer_id(0),
      gl_upload_query_id(0),
      gl_read_lock_query_id(0),
      pixels(pixels),
      lock_for_read_count(0),
      imported_count(0),
      exported_count(0),
      dirty_image(false),
      locked_for_write(false),
      lost(false),
      marked_for_deletion(false),
      pending_set_pixels(false),
      set_pixels_completion_forced(false),
      allocated(false),
      enable_read_lock_fences(false),
      has_shared_bitmap_id(!!bitmap),
      allow_overlay(false),
      read_lock_fence(NULL),
      size(size),
      origin(origin),
      target(0),
      original_filter(filter),
      filter(filter),
      image_id(0),
      bound_image_id(0),
      texture_pool(0),
      wrap_mode(wrap_mode),
      hint(TextureUsageAny),
      type(Bitmap),
      format(RGBA_8888),
      shared_bitmap(bitmap) {
  DCHECK(wrap_mode == GL_CLAMP_TO_EDGE || wrap_mode == GL_REPEAT);
  DCHECK(origin == Delegated || pixels);
  if (bitmap)
    shared_bitmap_id = bitmap->id();
}

ResourceProvider::Resource::Resource(const SharedBitmapId& bitmap_id,
                                     const gfx::Size& size,
                                     Origin origin,
                                     GLenum filter,
                                     GLint wrap_mode)
    : child_id(0),
      gl_id(0),
      gl_pixel_buffer_id(0),
      gl_upload_query_id(0),
      gl_read_lock_query_id(0),
      pixels(NULL),
      lock_for_read_count(0),
      imported_count(0),
      exported_count(0),
      dirty_image(false),
      locked_for_write(false),
      lost(false),
      marked_for_deletion(false),
      pending_set_pixels(false),
      set_pixels_completion_forced(false),
      allocated(false),
      enable_read_lock_fences(false),
      has_shared_bitmap_id(true),
      allow_overlay(false),
      read_lock_fence(NULL),
      size(size),
      origin(origin),
      target(0),
      original_filter(filter),
      filter(filter),
      image_id(0),
      bound_image_id(0),
      texture_pool(0),
      wrap_mode(wrap_mode),
      hint(TextureUsageAny),
      type(Bitmap),
      format(RGBA_8888),
      shared_bitmap_id(bitmap_id),
      shared_bitmap(NULL) {
  DCHECK(wrap_mode == GL_CLAMP_TO_EDGE || wrap_mode == GL_REPEAT);
}

ResourceProvider::RasterBuffer::RasterBuffer(
    const Resource* resource,
    ResourceProvider* resource_provider)
    : resource_(resource),
      resource_provider_(resource_provider),
      locked_canvas_(NULL),
      canvas_save_count_(0) {
  DCHECK(resource_);
  DCHECK(resource_provider_);
}

ResourceProvider::RasterBuffer::~RasterBuffer() {}

SkCanvas* ResourceProvider::RasterBuffer::LockForWrite() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "ResourceProvider::RasterBuffer::LockForWrite");

  DCHECK(!locked_canvas_);

  locked_canvas_ = DoLockForWrite();
  canvas_save_count_ = locked_canvas_ ? locked_canvas_->save() : 0;
  return locked_canvas_;
}

bool ResourceProvider::RasterBuffer::UnlockForWrite() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "ResourceProvider::RasterBuffer::UnlockForWrite");

  if (locked_canvas_) {
    locked_canvas_->restoreToCount(canvas_save_count_);
    locked_canvas_ = NULL;
  }
  return DoUnlockForWrite();
}

ResourceProvider::DirectRasterBuffer::DirectRasterBuffer(
    const Resource* resource,
    ResourceProvider* resource_provider,
    bool use_distance_field_text )
    : RasterBuffer(resource, resource_provider),
      surface_generation_id_(0u),
      use_distance_field_text_(use_distance_field_text) {}

ResourceProvider::DirectRasterBuffer::~DirectRasterBuffer() {}

SkCanvas* ResourceProvider::DirectRasterBuffer::DoLockForWrite() {
  if (!surface_)
    surface_ = CreateSurface();
  surface_generation_id_ = surface_ ? surface_->generationID() : 0u;
  return surface_ ? surface_->getCanvas() : NULL;
}

bool ResourceProvider::DirectRasterBuffer::DoUnlockForWrite() {
  // generationID returns a non-zero, unique value corresponding to the content
  // of surface. Hence, a change since DoLockForWrite was called means the
  // surface has changed.
  return surface_ ? surface_generation_id_ != surface_->generationID() : false;
}

skia::RefPtr<SkSurface> ResourceProvider::DirectRasterBuffer::CreateSurface() {
  skia::RefPtr<SkSurface> surface;
  switch (resource()->type) {
    case GLTexture: {
      DCHECK(resource()->gl_id);
      class GrContext* gr_context = resource_provider()->GrContext();
      if (gr_context) {
        GrBackendTextureDesc desc;
        desc.fFlags = kRenderTarget_GrBackendTextureFlag;
        desc.fWidth = resource()->size.width();
        desc.fHeight = resource()->size.height();
        desc.fConfig = ToGrPixelConfig(resource()->format);
        desc.fOrigin = kTopLeft_GrSurfaceOrigin;
        desc.fTextureHandle = resource()->gl_id;
        skia::RefPtr<GrTexture> gr_texture =
            skia::AdoptRef(gr_context->wrapBackendTexture(desc));
        SkSurface::TextRenderMode text_render_mode =
            use_distance_field_text_ ? SkSurface::kDistanceField_TextRenderMode
                                     : SkSurface::kStandard_TextRenderMode;
        surface = skia::AdoptRef(SkSurface::NewRenderTargetDirect(
            gr_texture->asRenderTarget(), text_render_mode));
      }
      break;
    }
    case Bitmap: {
      DCHECK(resource()->pixels);
      DCHECK_EQ(RGBA_8888, resource()->format);
      SkImageInfo image_info = SkImageInfo::MakeN32Premul(
          resource()->size.width(), resource()->size.height());
      surface = skia::AdoptRef(SkSurface::NewRasterDirect(
          image_info, resource()->pixels, image_info.minRowBytes()));
      break;
    }
    default:
      NOTREACHED();
  }
  return surface;
}

ResourceProvider::BitmapRasterBuffer::BitmapRasterBuffer(
    const Resource* resource,
    ResourceProvider* resource_provider)
    : RasterBuffer(resource, resource_provider),
      mapped_buffer_(NULL),
      raster_bitmap_generation_id_(0u) {}

ResourceProvider::BitmapRasterBuffer::~BitmapRasterBuffer() {}

SkCanvas* ResourceProvider::BitmapRasterBuffer::DoLockForWrite() {
  DCHECK(!mapped_buffer_);
  DCHECK(!raster_canvas_);

  int stride = 0;
  mapped_buffer_ = MapBuffer(&stride);
  if (!mapped_buffer_)
    return NULL;

  switch (resource()->format) {
    case RGBA_4444:
      // Use the default stride if we will eventually convert this
      // bitmap to 4444.
      raster_bitmap_.allocN32Pixels(resource()->size.width(),
                                    resource()->size.height());
      break;
    case RGBA_8888:
    case BGRA_8888: {
      SkImageInfo info = SkImageInfo::MakeN32Premul(resource()->size.width(),
                                                    resource()->size.height());
      if (0 == stride)
        stride = info.minRowBytes();
      raster_bitmap_.installPixels(info, mapped_buffer_, stride);
      break;
    }
    case LUMINANCE_8:
    case RGB_565:
    case ETC1:
      NOTREACHED();
      break;
  }
  raster_canvas_ = skia::AdoptRef(new SkCanvas(raster_bitmap_));
  raster_bitmap_generation_id_ = raster_bitmap_.getGenerationID();
  return raster_canvas_.get();
}

bool ResourceProvider::BitmapRasterBuffer::DoUnlockForWrite() {
  raster_canvas_.clear();

  // getGenerationID returns a non-zero, unique value corresponding to the
  // pixels in bitmap. Hence, a change since DoLockForWrite was called means the
  // bitmap has changed.
  bool raster_bitmap_changed =
      raster_bitmap_generation_id_ != raster_bitmap_.getGenerationID();

  if (raster_bitmap_changed) {
    SkColorType buffer_colorType =
        ResourceFormatToSkColorType(resource()->format);
    if (mapped_buffer_ && (buffer_colorType != raster_bitmap_.colorType()))
      CopyBitmap(raster_bitmap_, mapped_buffer_, buffer_colorType);
  }
  raster_bitmap_.reset();

  UnmapBuffer();
  mapped_buffer_ = NULL;
  return raster_bitmap_changed;
}

ResourceProvider::ImageRasterBuffer::ImageRasterBuffer(
    const Resource* resource,
    ResourceProvider* resource_provider)
    : BitmapRasterBuffer(resource, resource_provider) {}

ResourceProvider::ImageRasterBuffer::~ImageRasterBuffer() {}

uint8_t* ResourceProvider::ImageRasterBuffer::MapBuffer(int* stride) {
  return resource_provider()->MapImage(resource(), stride);
}

void ResourceProvider::ImageRasterBuffer::UnmapBuffer() {
  resource_provider()->UnmapImage(resource());
}

ResourceProvider::PixelRasterBuffer::PixelRasterBuffer(
    const Resource* resource,
    ResourceProvider* resource_provider)
    : BitmapRasterBuffer(resource, resource_provider) {}

ResourceProvider::PixelRasterBuffer::~PixelRasterBuffer() {}

uint8_t* ResourceProvider::PixelRasterBuffer::MapBuffer(int* stride) {
  return resource_provider()->MapPixelBuffer(resource(), stride);
}

void ResourceProvider::PixelRasterBuffer::UnmapBuffer() {
  resource_provider()->UnmapPixelBuffer(resource());
}

ResourceProvider::Child::Child() : marked_for_deletion(false) {}

ResourceProvider::Child::~Child() {}

scoped_ptr<ResourceProvider> ResourceProvider::Create(
    OutputSurface* output_surface,
    SharedBitmapManager* shared_bitmap_manager,
    int highp_threshold_min,
    bool use_rgba_4444_texture_format,
    size_t id_allocation_chunk_size,
    bool use_distance_field_text) {
  scoped_ptr<ResourceProvider> resource_provider(
      new ResourceProvider(output_surface,
                           shared_bitmap_manager,
                           highp_threshold_min,
                           use_rgba_4444_texture_format,
                           id_allocation_chunk_size,
                           use_distance_field_text));

  if (resource_provider->ContextGL())
    resource_provider->InitializeGL();
  else
    resource_provider->InitializeSoftware();

  DCHECK_NE(InvalidType, resource_provider->default_resource_type());
  return resource_provider.Pass();
}

ResourceProvider::~ResourceProvider() {
  while (!children_.empty())
    DestroyChildInternal(children_.begin(), ForShutdown);
  while (!resources_.empty())
    DeleteResourceInternal(resources_.begin(), ForShutdown);

  CleanUpGLIfNeeded();
}

bool ResourceProvider::InUseByConsumer(ResourceId id) {
  Resource* resource = GetResource(id);
  return resource->lock_for_read_count > 0 || resource->exported_count > 0 ||
         resource->lost;
}

bool ResourceProvider::IsLost(ResourceId id) {
  Resource* resource = GetResource(id);
  return resource->lost;
}

bool ResourceProvider::AllowOverlay(ResourceId id) {
  Resource* resource = GetResource(id);
  return resource->allow_overlay;
}

ResourceProvider::ResourceId ResourceProvider::CreateResource(
    const gfx::Size& size,
    GLint wrap_mode,
    TextureUsageHint hint,
    ResourceFormat format) {
  DCHECK(!size.IsEmpty());
  switch (default_resource_type_) {
    case GLTexture:
      return CreateGLTexture(size,
                             GL_TEXTURE_2D,
                             GL_TEXTURE_POOL_UNMANAGED_CHROMIUM,
                             wrap_mode,
                             hint,
                             format);
    case Bitmap:
      DCHECK_EQ(RGBA_8888, format);
      return CreateBitmap(size, wrap_mode);
    case InvalidType:
      break;
  }

  LOG(FATAL) << "Invalid default resource type.";
  return 0;
}

ResourceProvider::ResourceId ResourceProvider::CreateManagedResource(
    const gfx::Size& size,
    GLenum target,
    GLint wrap_mode,
    TextureUsageHint hint,
    ResourceFormat format) {
  DCHECK(!size.IsEmpty());
  switch (default_resource_type_) {
    case GLTexture:
      return CreateGLTexture(size,
                             target,
                             GL_TEXTURE_POOL_MANAGED_CHROMIUM,
                             wrap_mode,
                             hint,
                             format);
    case Bitmap:
      DCHECK_EQ(RGBA_8888, format);
      return CreateBitmap(size, wrap_mode);
    case InvalidType:
      break;
  }

  LOG(FATAL) << "Invalid default resource type.";
  return 0;
}

ResourceProvider::ResourceId ResourceProvider::CreateGLTexture(
    const gfx::Size& size,
    GLenum target,
    GLenum texture_pool,
    GLint wrap_mode,
    TextureUsageHint hint,
    ResourceFormat format) {
  DCHECK_LE(size.width(), max_texture_size_);
  DCHECK_LE(size.height(), max_texture_size_);
  DCHECK(thread_checker_.CalledOnValidThread());

  ResourceId id = next_id_++;
  Resource resource(0,
                    size,
                    Resource::Internal,
                    target,
                    GL_LINEAR,
                    texture_pool,
                    wrap_mode,
                    hint,
                    format);
  resource.allocated = false;
  resources_[id] = resource;
  return id;
}

ResourceProvider::ResourceId ResourceProvider::CreateBitmap(
    const gfx::Size& size, GLint wrap_mode) {
  DCHECK(thread_checker_.CalledOnValidThread());

  scoped_ptr<SharedBitmap> bitmap;
  if (shared_bitmap_manager_)
    bitmap = shared_bitmap_manager_->AllocateSharedBitmap(size);

  uint8_t* pixels;
  if (bitmap) {
    pixels = bitmap->pixels();
  } else {
    size_t bytes = SharedBitmap::CheckedSizeInBytes(size);
    pixels = new uint8_t[bytes];
  }
  DCHECK(pixels);

  ResourceId id = next_id_++;
  Resource resource(
      pixels, bitmap.release(), size, Resource::Internal, GL_LINEAR, wrap_mode);
  resource.allocated = true;
  resources_[id] = resource;
  return id;
}

ResourceProvider::ResourceId ResourceProvider::CreateResourceFromIOSurface(
    const gfx::Size& size,
    unsigned io_surface_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  ResourceId id = next_id_++;
  Resource resource(0,
                    gfx::Size(),
                    Resource::Internal,
                    GL_TEXTURE_RECTANGLE_ARB,
                    GL_LINEAR,
                    GL_TEXTURE_POOL_UNMANAGED_CHROMIUM,
                    GL_CLAMP_TO_EDGE,
                    TextureUsageAny,
                    RGBA_8888);
  LazyCreate(&resource);
  GLES2Interface* gl = ContextGL();
  DCHECK(gl);
  gl->BindTexture(GL_TEXTURE_RECTANGLE_ARB, resource.gl_id);
  gl->TexImageIOSurface2DCHROMIUM(
      GL_TEXTURE_RECTANGLE_ARB, size.width(), size.height(), io_surface_id, 0);
  resource.allocated = true;
  resources_[id] = resource;
  return id;
}

ResourceProvider::ResourceId ResourceProvider::CreateResourceFromTextureMailbox(
    const TextureMailbox& mailbox,
    scoped_ptr<SingleReleaseCallback> release_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Just store the information. Mailbox will be consumed in LockForRead().
  ResourceId id = next_id_++;
  DCHECK(mailbox.IsValid());
  Resource& resource = resources_[id];
  if (mailbox.IsTexture()) {
    resource = Resource(0,
                        gfx::Size(),
                        Resource::External,
                        mailbox.target(),
                        GL_LINEAR,
                        0,
                        GL_CLAMP_TO_EDGE,
                        TextureUsageAny,
                        RGBA_8888);
  } else {
    DCHECK(mailbox.IsSharedMemory());
    base::SharedMemory* shared_memory = mailbox.shared_memory();
    DCHECK(shared_memory->memory());
    uint8_t* pixels = reinterpret_cast<uint8_t*>(shared_memory->memory());
    DCHECK(pixels);
    scoped_ptr<SharedBitmap> shared_bitmap;
    if (shared_bitmap_manager_) {
      shared_bitmap =
          shared_bitmap_manager_->GetBitmapForSharedMemory(shared_memory);
    }
    resource = Resource(pixels,
                        shared_bitmap.release(),
                        mailbox.shared_memory_size(),
                        Resource::External,
                        GL_LINEAR,
                        GL_CLAMP_TO_EDGE);
  }
  resource.allocated = true;
  resource.mailbox = mailbox;
  resource.release_callback =
      base::Bind(&SingleReleaseCallback::Run,
                 base::Owned(release_callback.release()));
  resource.allow_overlay = mailbox.allow_overlay();
  return id;
}

void ResourceProvider::DeleteResource(ResourceId id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ResourceMap::iterator it = resources_.find(id);
  CHECK(it != resources_.end());
  Resource* resource = &it->second;
  DCHECK(!resource->marked_for_deletion);
  DCHECK_EQ(resource->imported_count, 0);
  DCHECK(resource->pending_set_pixels || !resource->locked_for_write);

  if (resource->exported_count > 0 || resource->lock_for_read_count > 0) {
    resource->marked_for_deletion = true;
    return;
  } else {
    DeleteResourceInternal(it, Normal);
  }
}

void ResourceProvider::DeleteResourceInternal(ResourceMap::iterator it,
                                              DeleteStyle style) {
  TRACE_EVENT0("cc", "ResourceProvider::DeleteResourceInternal");
  Resource* resource = &it->second;
  bool lost_resource = resource->lost;

  DCHECK(resource->exported_count == 0 || style != Normal);
  if (style == ForShutdown && resource->exported_count > 0)
    lost_resource = true;

  resource->direct_raster_buffer.reset();
  resource->image_raster_buffer.reset();
  resource->pixel_raster_buffer.reset();

  if (resource->image_id) {
    DCHECK(resource->origin == Resource::Internal);
    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    GLC(gl, gl->DestroyImageCHROMIUM(resource->image_id));
  }
  if (resource->gl_upload_query_id) {
    DCHECK(resource->origin == Resource::Internal);
    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    GLC(gl, gl->DeleteQueriesEXT(1, &resource->gl_upload_query_id));
  }
  if (resource->gl_read_lock_query_id) {
    DCHECK(resource->origin == Resource::Internal);
    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    GLC(gl, gl->DeleteQueriesEXT(1, &resource->gl_read_lock_query_id));
  }
  if (resource->gl_pixel_buffer_id) {
    DCHECK(resource->origin == Resource::Internal);
    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    GLC(gl, gl->DeleteBuffers(1, &resource->gl_pixel_buffer_id));
  }
  if (resource->origin == Resource::External) {
    DCHECK(resource->mailbox.IsValid());
    GLuint sync_point = resource->mailbox.sync_point();
    if (resource->type == GLTexture) {
      DCHECK(resource->mailbox.IsTexture());
      lost_resource |= lost_output_surface_;
      GLES2Interface* gl = ContextGL();
      DCHECK(gl);
      if (resource->gl_id) {
        GLC(gl, gl->DeleteTextures(1, &resource->gl_id));
        resource->gl_id = 0;
        if (!lost_resource)
          sync_point = gl->InsertSyncPointCHROMIUM();
      }
    } else {
      DCHECK(resource->mailbox.IsSharedMemory());
      base::SharedMemory* shared_memory = resource->mailbox.shared_memory();
      if (resource->pixels && shared_memory) {
        DCHECK(shared_memory->memory() == resource->pixels);
        resource->pixels = NULL;
        delete resource->shared_bitmap;
        resource->shared_bitmap = NULL;
      }
    }
    resource->release_callback.Run(sync_point, lost_resource);
  }
  if (resource->gl_id) {
    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    GLC(gl, gl->DeleteTextures(1, &resource->gl_id));
    resource->gl_id = 0;
  }
  if (resource->shared_bitmap) {
    DCHECK(resource->origin != Resource::External);
    DCHECK_EQ(Bitmap, resource->type);
    delete resource->shared_bitmap;
    resource->pixels = NULL;
  }
  if (resource->pixels) {
    DCHECK(resource->origin == Resource::Internal);
    delete[] resource->pixels;
  }
  resources_.erase(it);
}

ResourceProvider::ResourceType ResourceProvider::GetResourceType(
    ResourceId id) {
  return GetResource(id)->type;
}

void ResourceProvider::SetPixels(ResourceId id,
                                 const uint8_t* image,
                                 const gfx::Rect& image_rect,
                                 const gfx::Rect& source_rect,
                                 const gfx::Vector2d& dest_offset) {
  Resource* resource = GetResource(id);
  DCHECK(!resource->locked_for_write);
  DCHECK(!resource->lock_for_read_count);
  DCHECK(resource->origin == Resource::Internal);
  DCHECK_EQ(resource->exported_count, 0);
  DCHECK(ReadLockFenceHasPassed(resource));
  LazyAllocate(resource);

  if (resource->type == GLTexture) {
    DCHECK(resource->gl_id);
    DCHECK(!resource->pending_set_pixels);
    DCHECK_EQ(resource->target, static_cast<GLenum>(GL_TEXTURE_2D));
    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    DCHECK(texture_uploader_.get());
    gl->BindTexture(GL_TEXTURE_2D, resource->gl_id);
    texture_uploader_->Upload(image,
                              image_rect,
                              source_rect,
                              dest_offset,
                              resource->format,
                              resource->size);
  } else {
    DCHECK_EQ(Bitmap, resource->type);
    DCHECK(resource->allocated);
    DCHECK_EQ(RGBA_8888, resource->format);
    DCHECK(source_rect.x() >= image_rect.x());
    DCHECK(source_rect.y() >= image_rect.y());
    DCHECK(source_rect.right() <= image_rect.right());
    DCHECK(source_rect.bottom() <= image_rect.bottom());
    SkImageInfo source_info =
        SkImageInfo::MakeN32Premul(source_rect.width(), source_rect.height());
    size_t image_row_bytes = image_rect.width() * 4;
    gfx::Vector2d source_offset = source_rect.origin() - image_rect.origin();
    image += source_offset.y() * image_row_bytes + source_offset.x() * 4;

    ScopedWriteLockSoftware lock(this, id);
    SkCanvas* dest = lock.sk_canvas();
    dest->writePixels(
        source_info, image, image_row_bytes, dest_offset.x(), dest_offset.y());
  }
}

size_t ResourceProvider::NumBlockingUploads() {
  if (!texture_uploader_)
    return 0;

  return texture_uploader_->NumBlockingUploads();
}

void ResourceProvider::MarkPendingUploadsAsNonBlocking() {
  if (!texture_uploader_)
    return;

  texture_uploader_->MarkPendingUploadsAsNonBlocking();
}

size_t ResourceProvider::EstimatedUploadsPerTick() {
  if (!texture_uploader_)
    return 1u;

  double textures_per_second = texture_uploader_->EstimatedTexturesPerSecond();
  size_t textures_per_tick = floor(
      kTextureUploadTickRate * textures_per_second);
  return textures_per_tick ? textures_per_tick : 1u;
}

void ResourceProvider::FlushUploads() {
  if (!texture_uploader_)
    return;

  texture_uploader_->Flush();
}

void ResourceProvider::ReleaseCachedData() {
  if (!texture_uploader_)
    return;

  texture_uploader_->ReleaseCachedQueries();
}

base::TimeTicks ResourceProvider::EstimatedUploadCompletionTime(
    size_t uploads_per_tick) {
  if (lost_output_surface_)
    return base::TimeTicks();

  // Software resource uploads happen on impl thread, so don't bother batching
  // them up and trying to wait for them to complete.
  if (!texture_uploader_) {
    return gfx::FrameTime::Now() + base::TimeDelta::FromMicroseconds(
        base::Time::kMicrosecondsPerSecond * kSoftwareUploadTickRate);
  }

  base::TimeDelta upload_one_texture_time =
      base::TimeDelta::FromMicroseconds(
          base::Time::kMicrosecondsPerSecond * kTextureUploadTickRate) /
      uploads_per_tick;

  size_t total_uploads = NumBlockingUploads() + uploads_per_tick;
  return gfx::FrameTime::Now() + upload_one_texture_time * total_uploads;
}

void ResourceProvider::Flush() {
  DCHECK(thread_checker_.CalledOnValidThread());
  GLES2Interface* gl = ContextGL();
  if (gl)
    gl->Flush();
}

void ResourceProvider::Finish() {
  DCHECK(thread_checker_.CalledOnValidThread());
  GLES2Interface* gl = ContextGL();
  if (gl)
    gl->Finish();
}

bool ResourceProvider::ShallowFlushIfSupported() {
  DCHECK(thread_checker_.CalledOnValidThread());
  GLES2Interface* gl = ContextGL();
  if (!gl)
    return false;

  gl->ShallowFlushCHROMIUM();
  return true;
}

ResourceProvider::Resource* ResourceProvider::GetResource(ResourceId id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ResourceMap::iterator it = resources_.find(id);
  CHECK(it != resources_.end());
  return &it->second;
}

const ResourceProvider::Resource* ResourceProvider::LockForRead(ResourceId id) {
  Resource* resource = GetResource(id);
  DCHECK(!resource->locked_for_write ||
         resource->set_pixels_completion_forced) <<
      "locked for write: " << resource->locked_for_write <<
      " pixels completion forced: " << resource->set_pixels_completion_forced;
  DCHECK_EQ(resource->exported_count, 0);
  // Uninitialized! Call SetPixels or LockForWrite first.
  DCHECK(resource->allocated);

  LazyCreate(resource);

  if (resource->type == GLTexture && !resource->gl_id) {
    DCHECK(resource->origin != Resource::Internal);
    DCHECK(resource->mailbox.IsTexture());
    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    if (resource->mailbox.sync_point()) {
      GLC(gl, gl->WaitSyncPointCHROMIUM(resource->mailbox.sync_point()));
      resource->mailbox.set_sync_point(0);
    }
    resource->gl_id = texture_id_allocator_->NextId();
    GLC(gl, gl->BindTexture(resource->target, resource->gl_id));
    GLC(gl,
        gl->ConsumeTextureCHROMIUM(resource->mailbox.target(),
                                   resource->mailbox.name()));
  }

  if (!resource->pixels && resource->has_shared_bitmap_id &&
      shared_bitmap_manager_) {
    scoped_ptr<SharedBitmap> bitmap =
        shared_bitmap_manager_->GetSharedBitmapFromId(
            resource->size, resource->shared_bitmap_id);
    if (bitmap) {
      resource->shared_bitmap = bitmap.release();
      resource->pixels = resource->shared_bitmap->pixels();
    }
  }

  resource->lock_for_read_count++;
  if (resource->enable_read_lock_fences)
    resource->read_lock_fence = current_read_lock_fence_;

  return resource;
}

void ResourceProvider::UnlockForRead(ResourceId id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ResourceMap::iterator it = resources_.find(id);
  CHECK(it != resources_.end());

  Resource* resource = &it->second;
  DCHECK_GT(resource->lock_for_read_count, 0);
  DCHECK_EQ(resource->exported_count, 0);
  resource->lock_for_read_count--;
  if (resource->marked_for_deletion && !resource->lock_for_read_count) {
    if (!resource->child_id) {
      // The resource belongs to this ResourceProvider, so it can be destroyed.
      DeleteResourceInternal(it, Normal);
    } else {
      ChildMap::iterator child_it = children_.find(resource->child_id);
      ResourceIdArray unused;
      unused.push_back(id);
      DeleteAndReturnUnusedResourcesToChild(child_it, Normal, unused);
    }
  }
}

const ResourceProvider::Resource* ResourceProvider::LockForWrite(
    ResourceId id) {
  Resource* resource = GetResource(id);
  DCHECK(!resource->locked_for_write);
  DCHECK(!resource->lock_for_read_count);
  DCHECK_EQ(resource->exported_count, 0);
  DCHECK(resource->origin == Resource::Internal);
  DCHECK(!resource->lost);
  DCHECK(ReadLockFenceHasPassed(resource));
  LazyAllocate(resource);

  resource->locked_for_write = true;
  return resource;
}

bool ResourceProvider::CanLockForWrite(ResourceId id) {
  Resource* resource = GetResource(id);
  return !resource->locked_for_write && !resource->lock_for_read_count &&
         !resource->exported_count && resource->origin == Resource::Internal &&
         !resource->lost && ReadLockFenceHasPassed(resource);
}

void ResourceProvider::UnlockForWrite(ResourceId id) {
  Resource* resource = GetResource(id);
  DCHECK(resource->locked_for_write);
  DCHECK_EQ(resource->exported_count, 0);
  DCHECK(resource->origin == Resource::Internal);
  resource->locked_for_write = false;
}

ResourceProvider::ScopedReadLockGL::ScopedReadLockGL(
    ResourceProvider* resource_provider,
    ResourceProvider::ResourceId resource_id)
    : resource_provider_(resource_provider),
      resource_id_(resource_id),
      texture_id_(resource_provider->LockForRead(resource_id)->gl_id) {
  DCHECK(texture_id_);
}

ResourceProvider::ScopedReadLockGL::~ScopedReadLockGL() {
  resource_provider_->UnlockForRead(resource_id_);
}

ResourceProvider::ScopedSamplerGL::ScopedSamplerGL(
    ResourceProvider* resource_provider,
    ResourceProvider::ResourceId resource_id,
    GLenum filter)
    : ScopedReadLockGL(resource_provider, resource_id),
      unit_(GL_TEXTURE0),
      target_(resource_provider->BindForSampling(resource_id, unit_, filter)) {
}

ResourceProvider::ScopedSamplerGL::ScopedSamplerGL(
    ResourceProvider* resource_provider,
    ResourceProvider::ResourceId resource_id,
    GLenum unit,
    GLenum filter)
    : ScopedReadLockGL(resource_provider, resource_id),
      unit_(unit),
      target_(resource_provider->BindForSampling(resource_id, unit_, filter)) {
}

ResourceProvider::ScopedSamplerGL::~ScopedSamplerGL() {
}

ResourceProvider::ScopedWriteLockGL::ScopedWriteLockGL(
    ResourceProvider* resource_provider,
    ResourceProvider::ResourceId resource_id)
    : resource_provider_(resource_provider),
      resource_id_(resource_id),
      texture_id_(resource_provider->LockForWrite(resource_id)->gl_id) {
  DCHECK(texture_id_);
}

ResourceProvider::ScopedWriteLockGL::~ScopedWriteLockGL() {
  resource_provider_->UnlockForWrite(resource_id_);
}

void ResourceProvider::PopulateSkBitmapWithResource(
    SkBitmap* sk_bitmap, const Resource* resource) {
  DCHECK_EQ(RGBA_8888, resource->format);
  SkImageInfo info = SkImageInfo::MakeN32Premul(resource->size.width(),
                                                resource->size.height());
  sk_bitmap->installPixels(info, resource->pixels, info.minRowBytes());
}

ResourceProvider::ScopedReadLockSoftware::ScopedReadLockSoftware(
    ResourceProvider* resource_provider,
    ResourceProvider::ResourceId resource_id)
    : resource_provider_(resource_provider),
      resource_id_(resource_id) {
  const Resource* resource = resource_provider->LockForRead(resource_id);
  wrap_mode_ = resource->wrap_mode;
  ResourceProvider::PopulateSkBitmapWithResource(&sk_bitmap_, resource);
}

ResourceProvider::ScopedReadLockSoftware::~ScopedReadLockSoftware() {
  resource_provider_->UnlockForRead(resource_id_);
}

ResourceProvider::ScopedWriteLockSoftware::ScopedWriteLockSoftware(
    ResourceProvider* resource_provider,
    ResourceProvider::ResourceId resource_id)
    : resource_provider_(resource_provider),
      resource_id_(resource_id) {
  ResourceProvider::PopulateSkBitmapWithResource(
      &sk_bitmap_, resource_provider->LockForWrite(resource_id));
  DCHECK(valid());
  sk_canvas_.reset(new SkCanvas(sk_bitmap_));
}

ResourceProvider::ScopedWriteLockSoftware::~ScopedWriteLockSoftware() {
  resource_provider_->UnlockForWrite(resource_id_);
}

ResourceProvider::ResourceProvider(OutputSurface* output_surface,
                                   SharedBitmapManager* shared_bitmap_manager,
                                   int highp_threshold_min,
                                   bool use_rgba_4444_texture_format,
                                   size_t id_allocation_chunk_size,
                                   bool use_distance_field_text)
    : output_surface_(output_surface),
      shared_bitmap_manager_(shared_bitmap_manager),
      lost_output_surface_(false),
      highp_threshold_min_(highp_threshold_min),
      next_id_(1),
      next_child_(1),
      default_resource_type_(InvalidType),
      use_texture_storage_ext_(false),
      use_texture_usage_hint_(false),
      use_compressed_texture_etc1_(false),
      max_texture_size_(0),
      best_texture_format_(RGBA_8888),
      use_rgba_4444_texture_format_(use_rgba_4444_texture_format),
      id_allocation_chunk_size_(id_allocation_chunk_size),
      use_sync_query_(false),
      use_distance_field_text_(use_distance_field_text) {
  DCHECK(output_surface_->HasClient());
  DCHECK(id_allocation_chunk_size_);
}

void ResourceProvider::InitializeSoftware() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(Bitmap, default_resource_type_);

  CleanUpGLIfNeeded();

  default_resource_type_ = Bitmap;
  // Pick an arbitrary limit here similar to what hardware might.
  max_texture_size_ = 16 * 1024;
  best_texture_format_ = RGBA_8888;
}

void ResourceProvider::InitializeGL() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!texture_uploader_);
  DCHECK_NE(GLTexture, default_resource_type_);
  DCHECK(!texture_id_allocator_);
  DCHECK(!buffer_id_allocator_);

  default_resource_type_ = GLTexture;

  const ContextProvider::Capabilities& caps =
      output_surface_->context_provider()->ContextCapabilities();

  bool use_bgra = caps.gpu.texture_format_bgra8888;
  use_texture_storage_ext_ = caps.gpu.texture_storage;
  use_texture_usage_hint_ = caps.gpu.texture_usage;
  use_compressed_texture_etc1_ = caps.gpu.texture_format_etc1;
  use_sync_query_ = caps.gpu.sync_query;

  GLES2Interface* gl = ContextGL();
  DCHECK(gl);

  texture_uploader_ = TextureUploader::Create(gl);
  max_texture_size_ = 0;  // Context expects cleared value.
  GLC(gl, gl->GetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size_));
  best_texture_format_ = PlatformColor::BestTextureFormat(use_bgra);

  texture_id_allocator_.reset(
      new TextureIdAllocator(gl, id_allocation_chunk_size_));
  buffer_id_allocator_.reset(
      new BufferIdAllocator(gl, id_allocation_chunk_size_));
}

void ResourceProvider::CleanUpGLIfNeeded() {
  GLES2Interface* gl = ContextGL();
  if (default_resource_type_ != GLTexture) {
    // We are not in GL mode, but double check before returning.
    DCHECK(!gl);
    DCHECK(!texture_uploader_);
    return;
  }

  DCHECK(gl);
#if DCHECK_IS_ON
  // Check that all GL resources has been deleted.
  for (ResourceMap::const_iterator itr = resources_.begin();
       itr != resources_.end();
       ++itr) {
    DCHECK_NE(GLTexture, itr->second.type);
  }
#endif  // DCHECK_IS_ON

  texture_uploader_.reset();
  texture_id_allocator_.reset();
  buffer_id_allocator_.reset();
  Finish();
}

int ResourceProvider::CreateChild(const ReturnCallback& return_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  Child child_info;
  child_info.return_callback = return_callback;

  int child = next_child_++;
  children_[child] = child_info;
  return child;
}

void ResourceProvider::DestroyChild(int child_id) {
  ChildMap::iterator it = children_.find(child_id);
  DCHECK(it != children_.end());
  DestroyChildInternal(it, Normal);
}

void ResourceProvider::DestroyChildInternal(ChildMap::iterator it,
                                            DeleteStyle style) {
  DCHECK(thread_checker_.CalledOnValidThread());

  Child& child = it->second;
  DCHECK(style == ForShutdown || !child.marked_for_deletion);

  ResourceIdArray resources_for_child;

  for (ResourceIdMap::iterator child_it = child.child_to_parent_map.begin();
       child_it != child.child_to_parent_map.end();
       ++child_it) {
    ResourceId id = child_it->second;
    resources_for_child.push_back(id);
  }

  // If the child is going away, don't consider any resources in use.
  child.in_use_resources.clear();
  child.marked_for_deletion = true;

  DeleteAndReturnUnusedResourcesToChild(it, style, resources_for_child);
}

const ResourceProvider::ResourceIdMap& ResourceProvider::GetChildToParentMap(
    int child) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  ChildMap::const_iterator it = children_.find(child);
  DCHECK(it != children_.end());
  DCHECK(!it->second.marked_for_deletion);
  return it->second.child_to_parent_map;
}

void ResourceProvider::PrepareSendToParent(const ResourceIdArray& resources,
                                           TransferableResourceArray* list) {
  DCHECK(thread_checker_.CalledOnValidThread());
  GLES2Interface* gl = ContextGL();
  bool need_sync_point = false;
  for (ResourceIdArray::const_iterator it = resources.begin();
       it != resources.end();
       ++it) {
    TransferableResource resource;
    TransferResource(gl, *it, &resource);
    if (!resource.mailbox_holder.sync_point && !resource.is_software)
      need_sync_point = true;
    ++resources_.find(*it)->second.exported_count;
    list->push_back(resource);
  }
  if (need_sync_point) {
    GLuint sync_point = gl->InsertSyncPointCHROMIUM();
    for (TransferableResourceArray::iterator it = list->begin();
         it != list->end();
         ++it) {
      if (!it->mailbox_holder.sync_point)
        it->mailbox_holder.sync_point = sync_point;
    }
  }
}

void ResourceProvider::ReceiveFromChild(
    int child, const TransferableResourceArray& resources) {
  DCHECK(thread_checker_.CalledOnValidThread());
  GLES2Interface* gl = ContextGL();
  Child& child_info = children_.find(child)->second;
  DCHECK(!child_info.marked_for_deletion);
  for (TransferableResourceArray::const_iterator it = resources.begin();
       it != resources.end();
       ++it) {
    ResourceIdMap::iterator resource_in_map_it =
        child_info.child_to_parent_map.find(it->id);
    if (resource_in_map_it != child_info.child_to_parent_map.end()) {
      Resource& resource = resources_[resource_in_map_it->second];
      resource.marked_for_deletion = false;
      resource.imported_count++;
      continue;
    }

    if ((!it->is_software && !gl) ||
        (it->is_software && !shared_bitmap_manager_)) {
      TRACE_EVENT0("cc", "ResourceProvider::ReceiveFromChild dropping invalid");
      ReturnedResourceArray to_return;
      to_return.push_back(it->ToReturnedResource());
      child_info.return_callback.Run(to_return);
      continue;
    }

    ResourceId local_id = next_id_++;
    Resource& resource = resources_[local_id];
    if (it->is_software) {
      resource = Resource(it->mailbox_holder.mailbox,
                          it->size,
                          Resource::Delegated,
                          GL_LINEAR,
                          it->is_repeated ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    } else {
      resource = Resource(0,
                          it->size,
                          Resource::Delegated,
                          it->mailbox_holder.texture_target,
                          it->filter,
                          0,
                          it->is_repeated ? GL_REPEAT : GL_CLAMP_TO_EDGE,
                          TextureUsageAny,
                          it->format);
      resource.mailbox = TextureMailbox(it->mailbox_holder.mailbox,
                                        it->mailbox_holder.texture_target,
                                        it->mailbox_holder.sync_point);
    }
    resource.child_id = child;
    // Don't allocate a texture for a child.
    resource.allocated = true;
    resource.imported_count = 1;
    child_info.parent_to_child_map[local_id] = it->id;
    child_info.child_to_parent_map[it->id] = local_id;
  }
}

void ResourceProvider::DeclareUsedResourcesFromChild(
    int child,
    const ResourceIdArray& resources_from_child) {
  DCHECK(thread_checker_.CalledOnValidThread());

  ChildMap::iterator child_it = children_.find(child);
  DCHECK(child_it != children_.end());
  Child& child_info = child_it->second;
  DCHECK(!child_info.marked_for_deletion);
  child_info.in_use_resources.clear();

  for (size_t i = 0; i < resources_from_child.size(); ++i) {
    ResourceIdMap::iterator it =
        child_info.child_to_parent_map.find(resources_from_child[i]);
    DCHECK(it != child_info.child_to_parent_map.end());

    ResourceId local_id = it->second;
    DCHECK(!resources_[local_id].marked_for_deletion);
    child_info.in_use_resources.insert(local_id);
  }

  ResourceIdArray unused;
  for (ResourceIdMap::iterator it = child_info.child_to_parent_map.begin();
       it != child_info.child_to_parent_map.end();
       ++it) {
    ResourceId local_id = it->second;
    bool resource_is_in_use = child_info.in_use_resources.count(local_id) > 0;
    if (!resource_is_in_use)
      unused.push_back(local_id);
  }
  DeleteAndReturnUnusedResourcesToChild(child_it, Normal, unused);
}

// static
bool ResourceProvider::CompareResourceMapIteratorsByChildId(
    const std::pair<ReturnedResource, ResourceMap::iterator>& a,
    const std::pair<ReturnedResource, ResourceMap::iterator>& b) {
  const ResourceMap::iterator& a_it = a.second;
  const ResourceMap::iterator& b_it = b.second;
  const Resource& a_resource = a_it->second;
  const Resource& b_resource = b_it->second;
  return a_resource.child_id < b_resource.child_id;
}

void ResourceProvider::ReceiveReturnsFromParent(
    const ReturnedResourceArray& resources) {
  DCHECK(thread_checker_.CalledOnValidThread());
  GLES2Interface* gl = ContextGL();

  int child_id = 0;
  ResourceIdArray resources_for_child;

  std::vector<std::pair<ReturnedResource, ResourceMap::iterator> >
      sorted_resources;

  for (ReturnedResourceArray::const_iterator it = resources.begin();
       it != resources.end();
       ++it) {
    ResourceId local_id = it->id;
    ResourceMap::iterator map_iterator = resources_.find(local_id);

    // Resource was already lost (e.g. it belonged to a child that was
    // destroyed).
    if (map_iterator == resources_.end())
      continue;

    sorted_resources.push_back(
        std::pair<ReturnedResource, ResourceMap::iterator>(*it, map_iterator));
  }

  std::sort(sorted_resources.begin(),
            sorted_resources.end(),
            CompareResourceMapIteratorsByChildId);

  ChildMap::iterator child_it = children_.end();
  for (size_t i = 0; i < sorted_resources.size(); ++i) {
    ReturnedResource& returned = sorted_resources[i].first;
    ResourceMap::iterator& map_iterator = sorted_resources[i].second;
    ResourceId local_id = map_iterator->first;
    Resource* resource = &map_iterator->second;

    CHECK_GE(resource->exported_count, returned.count);
    resource->exported_count -= returned.count;
    resource->lost |= returned.lost;
    if (resource->exported_count)
      continue;

    // Need to wait for the current read lock fence to pass before we can
    // recycle this resource.
    if (resource->enable_read_lock_fences)
      resource->read_lock_fence = current_read_lock_fence_;

    if (returned.sync_point) {
      DCHECK(!resource->has_shared_bitmap_id);
      if (resource->origin == Resource::Internal) {
        DCHECK(resource->gl_id);
        GLC(gl, gl->WaitSyncPointCHROMIUM(returned.sync_point));
      } else {
        DCHECK(!resource->gl_id);
        resource->mailbox.set_sync_point(returned.sync_point);
      }
    }

    if (!resource->marked_for_deletion)
      continue;

    if (!resource->child_id) {
      // The resource belongs to this ResourceProvider, so it can be destroyed.
      DeleteResourceInternal(map_iterator, Normal);
      continue;
    }

    DCHECK(resource->origin == Resource::Delegated);
    // Delete the resource and return it to the child it came from one.
    if (resource->child_id != child_id) {
      if (child_id) {
        DCHECK_NE(resources_for_child.size(), 0u);
        DCHECK(child_it != children_.end());
        DeleteAndReturnUnusedResourcesToChild(
            child_it, Normal, resources_for_child);
        resources_for_child.clear();
      }

      child_it = children_.find(resource->child_id);
      DCHECK(child_it != children_.end());
      child_id = resource->child_id;
    }
    resources_for_child.push_back(local_id);
  }

  if (child_id) {
    DCHECK_NE(resources_for_child.size(), 0u);
    DCHECK(child_it != children_.end());
    DeleteAndReturnUnusedResourcesToChild(
        child_it, Normal, resources_for_child);
  }
}

void ResourceProvider::TransferResource(GLES2Interface* gl,
                                        ResourceId id,
                                        TransferableResource* resource) {
  Resource* source = GetResource(id);
  DCHECK(!source->locked_for_write);
  DCHECK(!source->lock_for_read_count);
  DCHECK(source->origin != Resource::External || source->mailbox.IsValid());
  DCHECK(source->allocated);
  resource->id = id;
  resource->format = source->format;
  resource->mailbox_holder.texture_target = source->target;
  resource->filter = source->filter;
  resource->size = source->size;
  resource->is_repeated = (source->wrap_mode == GL_REPEAT);

  if (source->type == Bitmap) {
    resource->mailbox_holder.mailbox = source->shared_bitmap_id;
    resource->is_software = true;
  } else if (!source->mailbox.IsValid()) {
    LazyCreate(source);
    DCHECK(source->gl_id);
    DCHECK(source->origin == Resource::Internal);
    GLC(gl,
        gl->BindTexture(resource->mailbox_holder.texture_target,
                        source->gl_id));
    if (source->image_id) {
      DCHECK(source->dirty_image);
      BindImageForSampling(source);
    }
    // This is a resource allocated by the compositor, we need to produce it.
    // Don't set a sync point, the caller will do it.
    GLC(gl, gl->GenMailboxCHROMIUM(resource->mailbox_holder.mailbox.name));
    GLC(gl,
        gl->ProduceTextureCHROMIUM(resource->mailbox_holder.texture_target,
                                   resource->mailbox_holder.mailbox.name));
    source->mailbox = TextureMailbox(resource->mailbox_holder);
  } else {
    DCHECK(source->mailbox.IsTexture());
    if (source->image_id && source->dirty_image) {
      DCHECK(source->gl_id);
      DCHECK(source->origin == Resource::Internal);
      GLC(gl,
          gl->BindTexture(resource->mailbox_holder.texture_target,
                          source->gl_id));
      BindImageForSampling(source);
    }
    // This is either an external resource, or a compositor resource that we
    // already exported. Make sure to forward the sync point that we were given.
    resource->mailbox_holder.mailbox = source->mailbox.mailbox();
    resource->mailbox_holder.texture_target = source->mailbox.target();
    resource->mailbox_holder.sync_point = source->mailbox.sync_point();
    source->mailbox.set_sync_point(0);
  }
}

void ResourceProvider::DeleteAndReturnUnusedResourcesToChild(
    ChildMap::iterator child_it,
    DeleteStyle style,
    const ResourceIdArray& unused) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(child_it != children_.end());
  Child* child_info = &child_it->second;

  if (unused.empty() && !child_info->marked_for_deletion)
    return;

  ReturnedResourceArray to_return;

  GLES2Interface* gl = ContextGL();
  bool need_sync_point = false;
  for (size_t i = 0; i < unused.size(); ++i) {
    ResourceId local_id = unused[i];

    ResourceMap::iterator it = resources_.find(local_id);
    CHECK(it != resources_.end());
    Resource& resource = it->second;

    DCHECK(!resource.locked_for_write);
    DCHECK_EQ(0u, child_info->in_use_resources.count(local_id));
    DCHECK(child_info->parent_to_child_map.count(local_id));

    ResourceId child_id = child_info->parent_to_child_map[local_id];
    DCHECK(child_info->child_to_parent_map.count(child_id));

    bool is_lost =
        resource.lost || (resource.type == GLTexture && lost_output_surface_);
    if (resource.exported_count > 0 || resource.lock_for_read_count > 0) {
      if (style != ForShutdown) {
        // Defer this until we receive the resource back from the parent or
        // the read lock is released.
        resource.marked_for_deletion = true;
        continue;
      }

      // We still have an exported_count, so we'll have to lose it.
      is_lost = true;
    }

    if (gl && resource.filter != resource.original_filter) {
      DCHECK(resource.target);
      DCHECK(resource.gl_id);

      GLC(gl, gl->BindTexture(resource.target, resource.gl_id));
      GLC(gl,
          gl->TexParameteri(resource.target,
                            GL_TEXTURE_MIN_FILTER,
                            resource.original_filter));
      GLC(gl,
          gl->TexParameteri(resource.target,
                            GL_TEXTURE_MAG_FILTER,
                            resource.original_filter));
    }

    ReturnedResource returned;
    returned.id = child_id;
    returned.sync_point = resource.mailbox.sync_point();
    if (!returned.sync_point && resource.type == GLTexture)
      need_sync_point = true;
    returned.count = resource.imported_count;
    returned.lost = is_lost;
    to_return.push_back(returned);

    child_info->parent_to_child_map.erase(local_id);
    child_info->child_to_parent_map.erase(child_id);
    resource.imported_count = 0;
    DeleteResourceInternal(it, style);
  }
  if (need_sync_point) {
    DCHECK(gl);
    GLuint sync_point = gl->InsertSyncPointCHROMIUM();
    for (size_t i = 0; i < to_return.size(); ++i) {
      if (!to_return[i].sync_point)
        to_return[i].sync_point = sync_point;
    }
  }

  if (!to_return.empty())
    child_info->return_callback.Run(to_return);

  if (child_info->marked_for_deletion &&
      child_info->parent_to_child_map.empty()) {
    DCHECK(child_info->child_to_parent_map.empty());
    children_.erase(child_it);
  }
}

SkCanvas* ResourceProvider::MapDirectRasterBuffer(ResourceId id) {
  // Resource needs to be locked for write since DirectRasterBuffer writes
  // directly to it.
  LockForWrite(id);
  Resource* resource = GetResource(id);
  if (!resource->direct_raster_buffer.get()) {
    resource->direct_raster_buffer.reset(
        new DirectRasterBuffer(resource, this, use_distance_field_text_));
  }
  return resource->direct_raster_buffer->LockForWrite();
}

void ResourceProvider::UnmapDirectRasterBuffer(ResourceId id) {
  Resource* resource = GetResource(id);
  DCHECK(resource->direct_raster_buffer.get());
  resource->direct_raster_buffer->UnlockForWrite();
  UnlockForWrite(id);
}

SkCanvas* ResourceProvider::MapImageRasterBuffer(ResourceId id) {
  Resource* resource = GetResource(id);
  AcquireImage(resource);
  if (!resource->image_raster_buffer.get())
    resource->image_raster_buffer.reset(new ImageRasterBuffer(resource, this));
  return resource->image_raster_buffer->LockForWrite();
}

bool ResourceProvider::UnmapImageRasterBuffer(ResourceId id) {
  Resource* resource = GetResource(id);
  resource->dirty_image = true;
  return resource->image_raster_buffer->UnlockForWrite();
}

void ResourceProvider::AcquirePixelRasterBuffer(ResourceId id) {
  Resource* resource = GetResource(id);
  AcquirePixelBuffer(resource);
  resource->pixel_raster_buffer.reset(new PixelRasterBuffer(resource, this));
}

void ResourceProvider::ReleasePixelRasterBuffer(ResourceId id) {
  Resource* resource = GetResource(id);
  resource->pixel_raster_buffer.reset();
  ReleasePixelBuffer(resource);
}

SkCanvas* ResourceProvider::MapPixelRasterBuffer(ResourceId id) {
  Resource* resource = GetResource(id);
  DCHECK(resource->pixel_raster_buffer.get());
  return resource->pixel_raster_buffer->LockForWrite();
}

bool ResourceProvider::UnmapPixelRasterBuffer(ResourceId id) {
  Resource* resource = GetResource(id);
  DCHECK(resource->pixel_raster_buffer.get());
  return resource->pixel_raster_buffer->UnlockForWrite();
}

void ResourceProvider::AcquirePixelBuffer(Resource* resource) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "ResourceProvider::AcquirePixelBuffer");

  DCHECK(resource->origin == Resource::Internal);
  DCHECK_EQ(resource->exported_count, 0);
  DCHECK(!resource->image_id);
  DCHECK_NE(ETC1, resource->format);

  DCHECK_EQ(GLTexture, resource->type);
  GLES2Interface* gl = ContextGL();
  DCHECK(gl);
  if (!resource->gl_pixel_buffer_id)
    resource->gl_pixel_buffer_id = buffer_id_allocator_->NextId();
  gl->BindBuffer(GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM,
                 resource->gl_pixel_buffer_id);
  unsigned bytes_per_pixel = BitsPerPixel(resource->format) / 8;
  gl->BufferData(GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM,
                 resource->size.height() *
                     RoundUp(bytes_per_pixel * resource->size.width(), 4u),
                 NULL,
                 GL_DYNAMIC_DRAW);
  gl->BindBuffer(GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM, 0);
}

void ResourceProvider::ReleasePixelBuffer(Resource* resource) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "ResourceProvider::ReleasePixelBuffer");

  DCHECK(resource->origin == Resource::Internal);
  DCHECK_EQ(resource->exported_count, 0);
  DCHECK(!resource->image_id);

  // The pixel buffer can be released while there is a pending "set pixels"
  // if completion has been forced. Any shared memory associated with this
  // pixel buffer will not be freed until the waitAsyncTexImage2DCHROMIUM
  // command has been processed on the service side. It is also safe to
  // reuse any query id associated with this resource before they complete
  // as each new query has a unique submit count.
  if (resource->pending_set_pixels) {
    DCHECK(resource->set_pixels_completion_forced);
    resource->pending_set_pixels = false;
    resource->locked_for_write = false;
  }

  DCHECK_EQ(GLTexture, resource->type);
  if (!resource->gl_pixel_buffer_id)
    return;
  GLES2Interface* gl = ContextGL();
  DCHECK(gl);
  gl->BindBuffer(GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM,
                 resource->gl_pixel_buffer_id);
  gl->BufferData(
      GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM, 0, NULL, GL_DYNAMIC_DRAW);
  gl->BindBuffer(GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM, 0);
}

uint8_t* ResourceProvider::MapPixelBuffer(const Resource* resource,
                                          int* stride) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "ResourceProvider::MapPixelBuffer");

  DCHECK(resource->origin == Resource::Internal);
  DCHECK_EQ(resource->exported_count, 0);
  DCHECK(!resource->image_id);

  *stride = 0;
  DCHECK_EQ(GLTexture, resource->type);
  GLES2Interface* gl = ContextGL();
  DCHECK(gl);
  DCHECK(resource->gl_pixel_buffer_id);
  gl->BindBuffer(GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM,
                 resource->gl_pixel_buffer_id);
  uint8_t* image = static_cast<uint8_t*>(gl->MapBufferCHROMIUM(
      GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM, GL_WRITE_ONLY));
  gl->BindBuffer(GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM, 0);
  // Buffer is required to be 4-byte aligned.
  CHECK(!(reinterpret_cast<intptr_t>(image) & 3));
  return image;
}

void ResourceProvider::UnmapPixelBuffer(const Resource* resource) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "ResourceProvider::UnmapPixelBuffer");

  DCHECK(resource->origin == Resource::Internal);
  DCHECK_EQ(resource->exported_count, 0);
  DCHECK(!resource->image_id);

  DCHECK_EQ(GLTexture, resource->type);
  GLES2Interface* gl = ContextGL();
  DCHECK(gl);
  DCHECK(resource->gl_pixel_buffer_id);
  gl->BindBuffer(GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM,
                 resource->gl_pixel_buffer_id);
  gl->UnmapBufferCHROMIUM(GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM);
  gl->BindBuffer(GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM, 0);
}

GLenum ResourceProvider::BindForSampling(
    ResourceProvider::ResourceId resource_id,
    GLenum unit,
    GLenum filter) {
  DCHECK(thread_checker_.CalledOnValidThread());
  GLES2Interface* gl = ContextGL();
  ResourceMap::iterator it = resources_.find(resource_id);
  DCHECK(it != resources_.end());
  Resource* resource = &it->second;
  DCHECK(resource->lock_for_read_count);
  DCHECK(!resource->locked_for_write || resource->set_pixels_completion_forced);

  ScopedSetActiveTexture scoped_active_tex(gl, unit);
  GLenum target = resource->target;
  GLC(gl, gl->BindTexture(target, resource->gl_id));
  if (filter != resource->filter) {
    GLC(gl, gl->TexParameteri(target, GL_TEXTURE_MIN_FILTER, filter));
    GLC(gl, gl->TexParameteri(target, GL_TEXTURE_MAG_FILTER, filter));
    resource->filter = filter;
  }

  if (resource->image_id && resource->dirty_image)
    BindImageForSampling(resource);

  return target;
}

void ResourceProvider::BeginSetPixels(ResourceId id) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "ResourceProvider::BeginSetPixels");

  Resource* resource = GetResource(id);
  DCHECK(!resource->pending_set_pixels);

  LazyCreate(resource);
  DCHECK(resource->origin == Resource::Internal);
  DCHECK(resource->gl_id || resource->allocated);
  DCHECK(ReadLockFenceHasPassed(resource));
  DCHECK(!resource->image_id);

  bool allocate = !resource->allocated;
  resource->allocated = true;
  LockForWrite(id);

  DCHECK_EQ(GLTexture, resource->type);
  DCHECK(resource->gl_id);
  GLES2Interface* gl = ContextGL();
  DCHECK(gl);
  DCHECK(resource->gl_pixel_buffer_id);
  DCHECK_EQ(resource->target, static_cast<GLenum>(GL_TEXTURE_2D));
  gl->BindTexture(GL_TEXTURE_2D, resource->gl_id);
  gl->BindBuffer(GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM,
                 resource->gl_pixel_buffer_id);
  if (!resource->gl_upload_query_id)
    gl->GenQueriesEXT(1, &resource->gl_upload_query_id);
  gl->BeginQueryEXT(GL_ASYNC_PIXEL_UNPACK_COMPLETED_CHROMIUM,
                    resource->gl_upload_query_id);
  if (allocate) {
    gl->AsyncTexImage2DCHROMIUM(GL_TEXTURE_2D,
                                0, /* level */
                                GLInternalFormat(resource->format),
                                resource->size.width(),
                                resource->size.height(),
                                0, /* border */
                                GLDataFormat(resource->format),
                                GLDataType(resource->format),
                                NULL);
  } else {
    gl->AsyncTexSubImage2DCHROMIUM(GL_TEXTURE_2D,
                                   0, /* level */
                                   0, /* x */
                                   0, /* y */
                                   resource->size.width(),
                                   resource->size.height(),
                                   GLDataFormat(resource->format),
                                   GLDataType(resource->format),
                                   NULL);
  }
  gl->EndQueryEXT(GL_ASYNC_PIXEL_UNPACK_COMPLETED_CHROMIUM);
  gl->BindBuffer(GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM, 0);

  resource->pending_set_pixels = true;
  resource->set_pixels_completion_forced = false;
}

void ResourceProvider::ForceSetPixelsToComplete(ResourceId id) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "ResourceProvider::ForceSetPixelsToComplete");

  Resource* resource = GetResource(id);
  DCHECK(resource->locked_for_write);
  DCHECK(resource->pending_set_pixels);
  DCHECK(!resource->set_pixels_completion_forced);

  if (resource->gl_id) {
    GLES2Interface* gl = ContextGL();
    GLC(gl, gl->BindTexture(GL_TEXTURE_2D, resource->gl_id));
    GLC(gl, gl->WaitAsyncTexImage2DCHROMIUM(GL_TEXTURE_2D));
    GLC(gl, gl->BindTexture(GL_TEXTURE_2D, 0));
  }

  resource->set_pixels_completion_forced = true;
}

bool ResourceProvider::DidSetPixelsComplete(ResourceId id) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "ResourceProvider::DidSetPixelsComplete");

  Resource* resource = GetResource(id);
  DCHECK(resource->locked_for_write);
  DCHECK(resource->pending_set_pixels);

  if (resource->gl_id) {
    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    DCHECK(resource->gl_upload_query_id);
    GLuint complete = 1;
    gl->GetQueryObjectuivEXT(
        resource->gl_upload_query_id, GL_QUERY_RESULT_AVAILABLE_EXT, &complete);
    if (!complete)
      return false;
  }

  resource->pending_set_pixels = false;
  UnlockForWrite(id);

  return true;
}

void ResourceProvider::CreateForTesting(ResourceId id) {
  LazyCreate(GetResource(id));
}

GLenum ResourceProvider::TargetForTesting(ResourceId id) {
  Resource* resource = GetResource(id);
  return resource->target;
}

void ResourceProvider::LazyCreate(Resource* resource) {
  if (resource->type != GLTexture || resource->origin != Resource::Internal)
    return;

  if (resource->gl_id)
    return;

  DCHECK(resource->texture_pool);
  DCHECK(resource->origin == Resource::Internal);
  DCHECK(!resource->mailbox.IsValid());
  resource->gl_id = texture_id_allocator_->NextId();

  GLES2Interface* gl = ContextGL();
  DCHECK(gl);

  // Create and set texture properties. Allocation is delayed until needed.
  GLC(gl, gl->BindTexture(resource->target, resource->gl_id));
  GLC(gl,
      gl->TexParameteri(resource->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
  GLC(gl,
      gl->TexParameteri(resource->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
  GLC(gl,
      gl->TexParameteri(
          resource->target, GL_TEXTURE_WRAP_S, resource->wrap_mode));
  GLC(gl,
      gl->TexParameteri(
          resource->target, GL_TEXTURE_WRAP_T, resource->wrap_mode));
  GLC(gl,
      gl->TexParameteri(
          resource->target, GL_TEXTURE_POOL_CHROMIUM, resource->texture_pool));
  if (use_texture_usage_hint_ && resource->hint == TextureUsageFramebuffer) {
    GLC(gl,
        gl->TexParameteri(resource->target,
                          GL_TEXTURE_USAGE_ANGLE,
                          GL_FRAMEBUFFER_ATTACHMENT_ANGLE));
  }
}

void ResourceProvider::AllocateForTesting(ResourceId id) {
  LazyAllocate(GetResource(id));
}

void ResourceProvider::LazyAllocate(Resource* resource) {
  DCHECK(resource);
  LazyCreate(resource);

  DCHECK(resource->gl_id || resource->allocated);
  if (resource->allocated || !resource->gl_id)
    return;
  resource->allocated = true;
  GLES2Interface* gl = ContextGL();
  gfx::Size& size = resource->size;
  DCHECK_EQ(resource->target, static_cast<GLenum>(GL_TEXTURE_2D));
  ResourceFormat format = resource->format;
  GLC(gl, gl->BindTexture(GL_TEXTURE_2D, resource->gl_id));
  if (use_texture_storage_ext_ && IsFormatSupportedForStorage(format) &&
      resource->hint != TextureUsageFramebuffer) {
    GLenum storage_format = TextureToStorageFormat(format);
    GLC(gl,
        gl->TexStorage2DEXT(
            GL_TEXTURE_2D, 1, storage_format, size.width(), size.height()));
  } else {
    // ETC1 does not support preallocation.
    if (format != ETC1) {
      GLC(gl,
          gl->TexImage2D(GL_TEXTURE_2D,
                         0,
                         GLInternalFormat(format),
                         size.width(),
                         size.height(),
                         0,
                         GLDataFormat(format),
                         GLDataType(format),
                         NULL));
    }
  }
}

void ResourceProvider::BindImageForSampling(Resource* resource) {
  GLES2Interface* gl = ContextGL();
  DCHECK(resource->gl_id);
  DCHECK(resource->image_id);

  // Release image currently bound to texture.
  if (resource->bound_image_id)
    gl->ReleaseTexImage2DCHROMIUM(resource->target, resource->bound_image_id);
  gl->BindTexImage2DCHROMIUM(resource->target, resource->image_id);
  resource->bound_image_id = resource->image_id;
  resource->dirty_image = false;
}

void ResourceProvider::EnableReadLockFences(ResourceProvider::ResourceId id,
                                            bool enable) {
  Resource* resource = GetResource(id);
  resource->enable_read_lock_fences = enable;
}

void ResourceProvider::AcquireImage(Resource* resource) {
  DCHECK(resource->origin == Resource::Internal);
  DCHECK_EQ(resource->exported_count, 0);

  if (resource->type != GLTexture)
    return;

  if (resource->image_id)
    return;

  resource->allocated = true;
  GLES2Interface* gl = ContextGL();
  DCHECK(gl);
  resource->image_id =
      gl->CreateImageCHROMIUM(resource->size.width(),
                              resource->size.height(),
                              TextureToStorageFormat(resource->format),
                              GL_IMAGE_MAP_CHROMIUM);
  DCHECK(resource->image_id);
}

void ResourceProvider::ReleaseImage(Resource* resource) {
  DCHECK(resource->origin == Resource::Internal);
  DCHECK_EQ(resource->exported_count, 0);

  if (!resource->image_id)
    return;

  GLES2Interface* gl = ContextGL();
  DCHECK(gl);
  gl->DestroyImageCHROMIUM(resource->image_id);
  resource->image_id = 0;
  resource->bound_image_id = 0;
  resource->dirty_image = false;
  resource->allocated = false;
}

uint8_t* ResourceProvider::MapImage(const Resource* resource, int* stride) {
  DCHECK(ReadLockFenceHasPassed(resource));
  DCHECK(resource->origin == Resource::Internal);
  DCHECK_EQ(resource->exported_count, 0);

  if (resource->type == GLTexture) {
    DCHECK(resource->image_id);
    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    // MapImageCHROMIUM should be called prior to GetImageParameterivCHROMIUM.
    uint8_t* pixels =
        static_cast<uint8_t*>(gl->MapImageCHROMIUM(resource->image_id));
    gl->GetImageParameterivCHROMIUM(
        resource->image_id, GL_IMAGE_ROWBYTES_CHROMIUM, stride);
    return pixels;
  }
  DCHECK_EQ(Bitmap, resource->type);
  *stride = 0;
  return resource->pixels;
}

void ResourceProvider::UnmapImage(const Resource* resource) {
  DCHECK(resource->origin == Resource::Internal);
  DCHECK_EQ(resource->exported_count, 0);

  if (resource->image_id) {
    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    gl->UnmapImageCHROMIUM(resource->image_id);
  }
}

void ResourceProvider::CopyResource(ResourceId source_id, ResourceId dest_id) {
  TRACE_EVENT0("cc", "ResourceProvider::CopyResource");

  Resource* source_resource = GetResource(source_id);
  DCHECK(!source_resource->lock_for_read_count);
  DCHECK(source_resource->origin == Resource::Internal);
  DCHECK_EQ(source_resource->exported_count, 0);
  DCHECK(source_resource->allocated);
  LazyCreate(source_resource);

  Resource* dest_resource = GetResource(dest_id);
  DCHECK(!dest_resource->locked_for_write);
  DCHECK(!dest_resource->lock_for_read_count);
  DCHECK(dest_resource->origin == Resource::Internal);
  DCHECK_EQ(dest_resource->exported_count, 0);
  LazyCreate(dest_resource);

  DCHECK_EQ(source_resource->type, dest_resource->type);
  DCHECK_EQ(source_resource->format, dest_resource->format);
  DCHECK(source_resource->size == dest_resource->size);

  if (source_resource->type == GLTexture) {
    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    if (source_resource->image_id && source_resource->dirty_image) {
      gl->BindTexture(source_resource->target, source_resource->gl_id);
      BindImageForSampling(source_resource);
    }
    DCHECK(use_sync_query_) << "CHROMIUM_sync_query extension missing";
    if (!source_resource->gl_read_lock_query_id)
      gl->GenQueriesEXT(1, &source_resource->gl_read_lock_query_id);
    gl->BeginQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM,
                      source_resource->gl_read_lock_query_id);
    DCHECK(!dest_resource->image_id);
    dest_resource->allocated = true;
    gl->CopyTextureCHROMIUM(dest_resource->target,
                            source_resource->gl_id,
                            dest_resource->gl_id,
                            0,
                            GLInternalFormat(dest_resource->format),
                            GLDataType(dest_resource->format));
    // End query and create a read lock fence that will prevent access to
    // source resource until CopyTextureCHROMIUM command has completed.
    gl->EndQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM);
    source_resource->read_lock_fence = make_scoped_refptr(
        new QueryFence(gl, source_resource->gl_read_lock_query_id));
  } else {
    DCHECK_EQ(Bitmap, source_resource->type);
    DCHECK_EQ(RGBA_8888, source_resource->format);
    LazyAllocate(dest_resource);

    size_t bytes = SharedBitmap::CheckedSizeInBytes(source_resource->size);
    memcpy(dest_resource->pixels, source_resource->pixels, bytes);
  }
}

GLint ResourceProvider::GetActiveTextureUnit(GLES2Interface* gl) {
  GLint active_unit = 0;
  gl->GetIntegerv(GL_ACTIVE_TEXTURE, &active_unit);
  return active_unit;
}

GLES2Interface* ResourceProvider::ContextGL() const {
  ContextProvider* context_provider = output_surface_->context_provider();
  return context_provider ? context_provider->ContextGL() : NULL;
}

class GrContext* ResourceProvider::GrContext() const {
  ContextProvider* context_provider = output_surface_->context_provider();
  return context_provider ? context_provider->GrContext() : NULL;
}

}  // namespace cc
