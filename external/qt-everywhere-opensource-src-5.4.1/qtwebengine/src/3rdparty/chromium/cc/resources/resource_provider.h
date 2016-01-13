// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_RESOURCE_PROVIDER_H_
#define CC_RESOURCES_RESOURCE_PROVIDER_H_

#include <deque>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "cc/base/cc_export.h"
#include "cc/output/context_provider.h"
#include "cc/output/output_surface.h"
#include "cc/resources/release_callback.h"
#include "cc/resources/resource_format.h"
#include "cc/resources/return_callback.h"
#include "cc/resources/shared_bitmap.h"
#include "cc/resources/single_release_callback.h"
#include "cc/resources/texture_mailbox.h"
#include "cc/resources/transferable_resource.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/size.h"

class GrContext;

namespace gpu {
namespace gles {
class GLES2Interface;
}
}
// A correct fix would be not to use GL types in this interal API file.
typedef unsigned int     GLenum;
typedef int              GLint;

namespace gfx {
class Rect;
class Vector2d;
}

namespace cc {
class IdAllocator;
class SharedBitmap;
class SharedBitmapManager;
class TextureUploader;

// This class is not thread-safe and can only be called from the thread it was
// created on (in practice, the impl thread).
class CC_EXPORT ResourceProvider {
 public:
  typedef unsigned ResourceId;
  typedef std::vector<ResourceId> ResourceIdArray;
  typedef std::set<ResourceId> ResourceIdSet;
  typedef base::hash_map<ResourceId, ResourceId> ResourceIdMap;
  enum TextureUsageHint {
    TextureUsageAny,
    TextureUsageFramebuffer,
  };
  enum ResourceType {
    InvalidType = 0,
    GLTexture = 1,
    Bitmap,
  };

  static scoped_ptr<ResourceProvider> Create(
      OutputSurface* output_surface,
      SharedBitmapManager* shared_bitmap_manager,
      int highp_threshold_min,
      bool use_rgba_4444_texture_format,
      size_t id_allocation_chunk_size,
      bool use_distance_field_text);
  virtual ~ResourceProvider();

  void InitializeSoftware();
  void InitializeGL();

  void DidLoseOutputSurface() { lost_output_surface_ = true; }

  int max_texture_size() const { return max_texture_size_; }
  ResourceFormat memory_efficient_texture_format() const {
    return use_rgba_4444_texture_format_ ? RGBA_4444 : best_texture_format_;
  }
  ResourceFormat best_texture_format() const { return best_texture_format_; }
  bool use_sync_query() const { return use_sync_query_; }
  size_t num_resources() const { return resources_.size(); }

  // Checks whether a resource is in use by a consumer.
  bool InUseByConsumer(ResourceId id);

  bool IsLost(ResourceId id);
  bool AllowOverlay(ResourceId id);

  // Producer interface.

  ResourceType default_resource_type() const { return default_resource_type_; }
  ResourceType GetResourceType(ResourceId id);

  // Creates a resource of the default resource type.
  ResourceId CreateResource(const gfx::Size& size,
                            GLint wrap_mode,
                            TextureUsageHint hint,
                            ResourceFormat format);

  // Creates a resource which is tagged as being managed for GPU memory
  // accounting purposes.
  ResourceId CreateManagedResource(const gfx::Size& size,
                                   GLenum target,
                                   GLint wrap_mode,
                                   TextureUsageHint hint,
                                   ResourceFormat format);

  // You can also explicitly create a specific resource type.
  ResourceId CreateGLTexture(const gfx::Size& size,
                             GLenum target,
                             GLenum texture_pool,
                             GLint wrap_mode,
                             TextureUsageHint hint,
                             ResourceFormat format);

  ResourceId CreateBitmap(const gfx::Size& size, GLint wrap_mode);
  // Wraps an IOSurface into a GL resource.
  ResourceId CreateResourceFromIOSurface(const gfx::Size& size,
                                         unsigned io_surface_id);

  // Wraps an external texture mailbox into a GL resource.
  ResourceId CreateResourceFromTextureMailbox(
      const TextureMailbox& mailbox,
      scoped_ptr<SingleReleaseCallback> release_callback);

  void DeleteResource(ResourceId id);

  // Update pixels from image, copying source_rect (in image) to dest_offset (in
  // the resource).
  void SetPixels(ResourceId id,
                 const uint8_t* image,
                 const gfx::Rect& image_rect,
                 const gfx::Rect& source_rect,
                 const gfx::Vector2d& dest_offset);

  // Check upload status.
  size_t NumBlockingUploads();
  void MarkPendingUploadsAsNonBlocking();
  size_t EstimatedUploadsPerTick();
  void FlushUploads();
  void ReleaseCachedData();
  base::TimeTicks EstimatedUploadCompletionTime(size_t uploads_per_tick);

  // Flush all context operations, kicking uploads and ensuring ordering with
  // respect to other contexts.
  void Flush();

  // Finish all context operations, causing any pending callbacks to be
  // scheduled.
  void Finish();

  // Only flush the command buffer if supported.
  // Returns true if the shallow flush occurred, false otherwise.
  bool ShallowFlushIfSupported();

  // Creates accounting for a child. Returns a child ID.
  int CreateChild(const ReturnCallback& return_callback);

  // Destroys accounting for the child, deleting all accounted resources.
  void DestroyChild(int child);

  // Gets the child->parent resource ID map.
  const ResourceIdMap& GetChildToParentMap(int child) const;

  // Prepares resources to be transfered to the parent, moving them to
  // mailboxes and serializing meta-data into TransferableResources.
  // Resources are not removed from the ResourceProvider, but are marked as
  // "in use".
  void PrepareSendToParent(const ResourceIdArray& resources,
                           TransferableResourceArray* transferable_resources);

  // Receives resources from a child, moving them from mailboxes. Resource IDs
  // passed are in the child namespace, and will be translated to the parent
  // namespace, added to the child->parent map.
  // This adds the resources to the working set in the ResourceProvider without
  // declaring which resources are in use. Use DeclareUsedResourcesFromChild
  // after calling this method to do that. All calls to ReceiveFromChild should
  // be followed by a DeclareUsedResourcesFromChild.
  // NOTE: if the sync_point is set on any TransferableResource, this will
  // wait on it.
  void ReceiveFromChild(
      int child, const TransferableResourceArray& transferable_resources);

  // Once a set of resources have been received, they may or may not be used.
  // This declares what set of resources are currently in use from the child,
  // releasing any other resources back to the child.
  void DeclareUsedResourcesFromChild(
      int child,
      const ResourceIdArray& resources_from_child);

  // Receives resources from the parent, moving them from mailboxes. Resource
  // IDs passed are in the child namespace.
  // NOTE: if the sync_point is set on any TransferableResource, this will
  // wait on it.
  void ReceiveReturnsFromParent(
      const ReturnedResourceArray& transferable_resources);

  // The following lock classes are part of the ResourceProvider API and are
  // needed to read and write the resource contents. The user must ensure
  // that they only use GL locks on GL resources, etc, and this is enforced
  // by assertions.
  class CC_EXPORT ScopedReadLockGL {
   public:
    ScopedReadLockGL(ResourceProvider* resource_provider,
                     ResourceProvider::ResourceId resource_id);
    virtual ~ScopedReadLockGL();

    unsigned texture_id() const { return texture_id_; }

   protected:
    ResourceProvider* resource_provider_;
    ResourceProvider::ResourceId resource_id_;

   private:
    unsigned texture_id_;

    DISALLOW_COPY_AND_ASSIGN(ScopedReadLockGL);
  };

  class CC_EXPORT ScopedSamplerGL : public ScopedReadLockGL {
   public:
    ScopedSamplerGL(ResourceProvider* resource_provider,
                    ResourceProvider::ResourceId resource_id,
                    GLenum filter);
    ScopedSamplerGL(ResourceProvider* resource_provider,
                    ResourceProvider::ResourceId resource_id,
                    GLenum unit,
                    GLenum filter);
    virtual ~ScopedSamplerGL();

    GLenum target() const { return target_; }

   private:
    GLenum unit_;
    GLenum target_;

    DISALLOW_COPY_AND_ASSIGN(ScopedSamplerGL);
  };

  class CC_EXPORT ScopedWriteLockGL {
   public:
    ScopedWriteLockGL(ResourceProvider* resource_provider,
                      ResourceProvider::ResourceId resource_id);
    ~ScopedWriteLockGL();

    unsigned texture_id() const { return texture_id_; }

   private:
    ResourceProvider* resource_provider_;
    ResourceProvider::ResourceId resource_id_;
    unsigned texture_id_;

    DISALLOW_COPY_AND_ASSIGN(ScopedWriteLockGL);
  };

  class CC_EXPORT ScopedReadLockSoftware {
   public:
    ScopedReadLockSoftware(ResourceProvider* resource_provider,
                           ResourceProvider::ResourceId resource_id);
    ~ScopedReadLockSoftware();

    const SkBitmap* sk_bitmap() const {
      DCHECK(valid());
      return &sk_bitmap_;
    }
    GLint wrap_mode() const { return wrap_mode_; }

    bool valid() const { return !!sk_bitmap_.getPixels(); }

   private:
    ResourceProvider* resource_provider_;
    ResourceProvider::ResourceId resource_id_;
    SkBitmap sk_bitmap_;
    GLint wrap_mode_;

    DISALLOW_COPY_AND_ASSIGN(ScopedReadLockSoftware);
  };

  class CC_EXPORT ScopedWriteLockSoftware {
   public:
    ScopedWriteLockSoftware(ResourceProvider* resource_provider,
                            ResourceProvider::ResourceId resource_id);
    ~ScopedWriteLockSoftware();

    SkCanvas* sk_canvas() { return sk_canvas_.get(); }
    bool valid() const { return !!sk_bitmap_.getPixels(); }

   private:
    ResourceProvider* resource_provider_;
    ResourceProvider::ResourceId resource_id_;
    SkBitmap sk_bitmap_;
    scoped_ptr<SkCanvas> sk_canvas_;

    DISALLOW_COPY_AND_ASSIGN(ScopedWriteLockSoftware);
  };

  class Fence : public base::RefCounted<Fence> {
   public:
    Fence() {}
    virtual bool HasPassed() = 0;

   protected:
    friend class base::RefCounted<Fence>;
    virtual ~Fence() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Fence);
  };

  // Returns a canvas for direct rasterization.
  // Call Unmap before the resource can be read or used for compositing.
  // It is used for direct gpu rasterization.
  SkCanvas* MapDirectRasterBuffer(ResourceId id);
  void UnmapDirectRasterBuffer(ResourceId id);

  // Returns a canvas backed by an image buffer. UnmapImageRasterBuffer
  // returns true if canvas was written to while mapped.
  // Rasterizing to the canvas writes the content into the image buffer,
  // which is internally bound to the underlying resource when read.
  // Call Unmap before the resource can be read or used for compositing.
  // It is used by ImageRasterWorkerPool.
  SkCanvas* MapImageRasterBuffer(ResourceId id);
  bool UnmapImageRasterBuffer(ResourceId id);

  // Returns a canvas backed by pixel buffer. UnmapPixelRasterBuffer
  // returns true if canvas was written to while mapped.
  // The pixel buffer needs to be uploaded to the underlying resource
  // using BeginSetPixels before the resouce can be used for compositing.
  // It is used by PixelRasterWorkerPool.
  void AcquirePixelRasterBuffer(ResourceId id);
  void ReleasePixelRasterBuffer(ResourceId id);
  SkCanvas* MapPixelRasterBuffer(ResourceId id);
  bool UnmapPixelRasterBuffer(ResourceId id);

  // Asynchronously update pixels from acquired pixel buffer.
  void BeginSetPixels(ResourceId id);
  void ForceSetPixelsToComplete(ResourceId id);
  bool DidSetPixelsComplete(ResourceId id);

  // For tests only! This prevents detecting uninitialized reads.
  // Use SetPixels or LockForWrite to allocate implicitly.
  void AllocateForTesting(ResourceId id);

  // For tests only!
  void CreateForTesting(ResourceId id);

  GLenum TargetForTesting(ResourceId id);

  // Sets the current read fence. If a resource is locked for read
  // and has read fences enabled, the resource will not allow writes
  // until this fence has passed.
  void SetReadLockFence(Fence* fence) { current_read_lock_fence_ = fence; }

  // Enable read lock fences for a specific resource.
  void EnableReadLockFences(ResourceProvider::ResourceId id, bool enable);

  // Indicates if we can currently lock this resource for write.
  bool CanLockForWrite(ResourceId id);

  // Copy pixels from source to destination.
  void CopyResource(ResourceId source_id, ResourceId dest_id);

  static GLint GetActiveTextureUnit(gpu::gles2::GLES2Interface* gl);

 private:
  class DirectRasterBuffer;
  class ImageRasterBuffer;
  class PixelRasterBuffer;

  struct Resource {
    enum Origin { Internal, External, Delegated };

    Resource();
    ~Resource();
    Resource(unsigned texture_id,
             const gfx::Size& size,
             Origin origin,
             GLenum target,
             GLenum filter,
             GLenum texture_pool,
             GLint wrap_mode,
             TextureUsageHint hint,
             ResourceFormat format);
    Resource(uint8_t* pixels,
             SharedBitmap* bitmap,
             const gfx::Size& size,
             Origin origin,
             GLenum filter,
             GLint wrap_mode);
    Resource(const SharedBitmapId& bitmap_id,
             const gfx::Size& size,
             Origin origin,
             GLenum filter,
             GLint wrap_mode);

    int child_id;
    unsigned gl_id;
    // Pixel buffer used for set pixels without unnecessary copying.
    unsigned gl_pixel_buffer_id;
    // Query used to determine when asynchronous set pixels complete.
    unsigned gl_upload_query_id;
    // Query used to determine when read lock fence has passed.
    unsigned gl_read_lock_query_id;
    TextureMailbox mailbox;
    ReleaseCallback release_callback;
    uint8_t* pixels;
    int lock_for_read_count;
    int imported_count;
    int exported_count;
    bool dirty_image : 1;
    bool locked_for_write : 1;
    bool lost : 1;
    bool marked_for_deletion : 1;
    bool pending_set_pixels : 1;
    bool set_pixels_completion_forced : 1;
    bool allocated : 1;
    bool enable_read_lock_fences : 1;
    bool has_shared_bitmap_id : 1;
    bool allow_overlay : 1;
    scoped_refptr<Fence> read_lock_fence;
    gfx::Size size;
    Origin origin;
    GLenum target;
    // TODO(skyostil): Use a separate sampler object for filter state.
    GLenum original_filter;
    GLenum filter;
    unsigned image_id;
    unsigned bound_image_id;
    GLenum texture_pool;
    GLint wrap_mode;
    TextureUsageHint hint;
    ResourceType type;
    ResourceFormat format;
    SharedBitmapId shared_bitmap_id;
    SharedBitmap* shared_bitmap;
    linked_ptr<DirectRasterBuffer> direct_raster_buffer;
    linked_ptr<ImageRasterBuffer> image_raster_buffer;
    linked_ptr<PixelRasterBuffer> pixel_raster_buffer;
  };
  typedef base::hash_map<ResourceId, Resource> ResourceMap;

  class RasterBuffer {
   public:
    virtual ~RasterBuffer();

    SkCanvas* LockForWrite();
    // Returns true if canvas was written to while locked.
    bool UnlockForWrite();

   protected:
    RasterBuffer(const Resource* resource, ResourceProvider* resource_provider);
    const Resource* resource() const { return resource_; }
    ResourceProvider* resource_provider() const { return resource_provider_; }

    virtual SkCanvas* DoLockForWrite() = 0;
    virtual bool DoUnlockForWrite() = 0;

   private:
    const Resource* resource_;
    ResourceProvider* resource_provider_;
    SkCanvas* locked_canvas_;
    int canvas_save_count_;
  };

  class DirectRasterBuffer : public RasterBuffer {
   public:
    DirectRasterBuffer(const Resource* resource,
                       ResourceProvider* resource_provider,
                       bool use_distance_field_text);
    virtual ~DirectRasterBuffer();

   protected:
    virtual SkCanvas* DoLockForWrite() OVERRIDE;
    virtual bool DoUnlockForWrite() OVERRIDE;
    skia::RefPtr<SkSurface> CreateSurface();

   private:
    skia::RefPtr<SkSurface> surface_;
    uint32_t surface_generation_id_;
    const bool use_distance_field_text_;

    DISALLOW_COPY_AND_ASSIGN(DirectRasterBuffer);
  };

  class BitmapRasterBuffer : public RasterBuffer {
   public:
    virtual ~BitmapRasterBuffer();

   protected:
    BitmapRasterBuffer(const Resource* resource,
                       ResourceProvider* resource_provider);

    virtual SkCanvas* DoLockForWrite() OVERRIDE;
    virtual bool DoUnlockForWrite() OVERRIDE;

    virtual uint8_t* MapBuffer(int* stride) = 0;
    virtual void UnmapBuffer() = 0;

   private:
    uint8_t* mapped_buffer_;
    SkBitmap raster_bitmap_;
    uint32_t raster_bitmap_generation_id_;
    skia::RefPtr<SkCanvas> raster_canvas_;
  };

  class ImageRasterBuffer : public BitmapRasterBuffer {
   public:
    ImageRasterBuffer(const Resource* resource,
                      ResourceProvider* resource_provider);
    virtual ~ImageRasterBuffer();

   protected:
    virtual uint8_t* MapBuffer(int* stride) OVERRIDE;
    virtual void UnmapBuffer() OVERRIDE;

   private:
    DISALLOW_COPY_AND_ASSIGN(ImageRasterBuffer);
  };

  class PixelRasterBuffer : public BitmapRasterBuffer {
   public:
    PixelRasterBuffer(const Resource* resource,
                      ResourceProvider* resource_provider);
    virtual ~PixelRasterBuffer();

   protected:
    virtual uint8_t* MapBuffer(int* stride) OVERRIDE;
    virtual void UnmapBuffer() OVERRIDE;

   private:
    DISALLOW_COPY_AND_ASSIGN(PixelRasterBuffer);
  };

  static bool CompareResourceMapIteratorsByChildId(
      const std::pair<ReturnedResource, ResourceMap::iterator>& a,
      const std::pair<ReturnedResource, ResourceMap::iterator>& b);

  struct Child {
    Child();
    ~Child();

    ResourceIdMap child_to_parent_map;
    ResourceIdMap parent_to_child_map;
    ReturnCallback return_callback;
    ResourceIdSet in_use_resources;
    bool marked_for_deletion;
  };
  typedef base::hash_map<int, Child> ChildMap;

  bool ReadLockFenceHasPassed(const Resource* resource) {
    return !resource->read_lock_fence.get() ||
           resource->read_lock_fence->HasPassed();
  }

  ResourceProvider(OutputSurface* output_surface,
                   SharedBitmapManager* shared_bitmap_manager,
                   int highp_threshold_min,
                   bool use_rgba_4444_texture_format,
                   size_t id_allocation_chunk_size,
                   bool use_distance_field_text);

  void CleanUpGLIfNeeded();

  Resource* GetResource(ResourceId id);
  const Resource* LockForRead(ResourceId id);
  void UnlockForRead(ResourceId id);
  const Resource* LockForWrite(ResourceId id);
  void UnlockForWrite(ResourceId id);
  static void PopulateSkBitmapWithResource(SkBitmap* sk_bitmap,
                                           const Resource* resource);

  void TransferResource(gpu::gles2::GLES2Interface* gl,
                        ResourceId id,
                        TransferableResource* resource);
  enum DeleteStyle {
    Normal,
    ForShutdown,
  };
  void DeleteResourceInternal(ResourceMap::iterator it, DeleteStyle style);
  void DeleteAndReturnUnusedResourcesToChild(ChildMap::iterator child_it,
                                             DeleteStyle style,
                                             const ResourceIdArray& unused);
  void DestroyChildInternal(ChildMap::iterator it, DeleteStyle style);
  void LazyCreate(Resource* resource);
  void LazyAllocate(Resource* resource);

  // TODO(alokp): Move the implementation to PixelRasterBuffer.
  // Acquire pixel buffer for resource. The pixel buffer can be used to
  // set resource pixels without performing unnecessary copying.
  void AcquirePixelBuffer(Resource* resource);
  void ReleasePixelBuffer(Resource* resource);
  // Map/unmap the acquired pixel buffer.
  uint8_t* MapPixelBuffer(const Resource* resource, int* stride);
  void UnmapPixelBuffer(const Resource* resource);

  // TODO(alokp): Move the implementation to ImageRasterBuffer.
  // Acquire and release an image. The image allows direct
  // manipulation of texture memory.
  void AcquireImage(Resource* resource);
  void ReleaseImage(Resource* resource);
  // Maps the acquired image so that its pixels could be modified.
  // Unmap is called when all pixels are set.
  uint8_t* MapImage(const Resource* resource, int* stride);
  void UnmapImage(const Resource* resource);

  void BindImageForSampling(Resource* resource);
  // Binds the given GL resource to a texture target for sampling using the
  // specified filter for both minification and magnification. Returns the
  // texture target used. The resource must be locked for reading.
  GLenum BindForSampling(ResourceProvider::ResourceId resource_id,
                         GLenum unit,
                         GLenum filter);

  // Returns NULL if the output_surface_ does not have a ContextProvider.
  gpu::gles2::GLES2Interface* ContextGL() const;
  class GrContext* GrContext() const;

  OutputSurface* output_surface_;
  SharedBitmapManager* shared_bitmap_manager_;
  bool lost_output_surface_;
  int highp_threshold_min_;
  ResourceId next_id_;
  ResourceMap resources_;
  int next_child_;
  ChildMap children_;

  ResourceType default_resource_type_;
  bool use_texture_storage_ext_;
  bool use_texture_usage_hint_;
  bool use_compressed_texture_etc1_;
  scoped_ptr<TextureUploader> texture_uploader_;
  int max_texture_size_;
  ResourceFormat best_texture_format_;

  base::ThreadChecker thread_checker_;

  scoped_refptr<Fence> current_read_lock_fence_;
  bool use_rgba_4444_texture_format_;

  const size_t id_allocation_chunk_size_;
  scoped_ptr<IdAllocator> texture_id_allocator_;
  scoped_ptr<IdAllocator> buffer_id_allocator_;

  bool use_sync_query_;

  bool use_distance_field_text_;

  DISALLOW_COPY_AND_ASSIGN(ResourceProvider);
};


// TODO(epenner): Move these format conversions to resource_format.h
// once that builds on mac (npapi.h currently #includes OpenGL.h).
inline unsigned BitsPerPixel(ResourceFormat format) {
  DCHECK_LE(format, RESOURCE_FORMAT_MAX);
  static const unsigned format_bits_per_pixel[RESOURCE_FORMAT_MAX + 1] = {
    32,  // RGBA_8888
    16,  // RGBA_4444
    32,  // BGRA_8888
    8,   // LUMINANCE_8
    16,  // RGB_565,
    4    // ETC1
  };
  return format_bits_per_pixel[format];
}

}  // namespace cc

#endif  // CC_RESOURCES_RESOURCE_PROVIDER_H_
