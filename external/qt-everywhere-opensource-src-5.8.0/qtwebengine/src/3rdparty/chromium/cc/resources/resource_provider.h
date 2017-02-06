// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_RESOURCE_PROVIDER_H_
#define CC_RESOURCES_RESOURCE_PROVIDER_H_

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_provider.h"
#include "cc/base/cc_export.h"
#include "cc/base/resource_id.h"
#include "cc/output/context_provider.h"
#include "cc/output/output_surface.h"
#include "cc/resources/release_callback_impl.h"
#include "cc/resources/resource_format.h"
#include "cc/resources/return_callback.h"
#include "cc/resources/shared_bitmap.h"
#include "cc/resources/single_release_callback_impl.h"
#include "cc/resources/texture_mailbox.h"
#include "cc/resources/transferable_resource.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

class GrContext;
class GrSurface;

namespace gpu {
class GpuMemoryBufferManager;
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
class BlockingTaskRunner;
class IdAllocator;
class SharedBitmap;
class SharedBitmapManager;

// This class is not thread-safe and can only be called from the thread it was
// created on (in practice, the impl thread).
class CC_EXPORT ResourceProvider
    : public base::trace_event::MemoryDumpProvider {
 private:
  struct Resource;

 public:
  using ResourceIdArray = std::vector<ResourceId>;
  using ResourceIdSet = std::unordered_set<ResourceId>;
  using ResourceIdMap = std::unordered_map<ResourceId, ResourceId>;
  enum TextureHint {
    TEXTURE_HINT_DEFAULT = 0x0,
    TEXTURE_HINT_IMMUTABLE = 0x1,
    TEXTURE_HINT_FRAMEBUFFER = 0x2,
    TEXTURE_HINT_IMMUTABLE_FRAMEBUFFER =
        TEXTURE_HINT_IMMUTABLE | TEXTURE_HINT_FRAMEBUFFER
  };
  enum ResourceType {
    RESOURCE_TYPE_GPU_MEMORY_BUFFER,
    RESOURCE_TYPE_GL_TEXTURE,
    RESOURCE_TYPE_BITMAP,
  };

  ResourceProvider(ContextProvider* compositor_context_provider,
                   SharedBitmapManager* shared_bitmap_manager,
                   gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
                   BlockingTaskRunner* blocking_main_thread_task_runner,
                   int highp_threshold_min,
                   size_t id_allocation_chunk_size,
                   bool delegated_sync_points_required,
                   bool use_gpu_memory_buffer_resources,
                   const std::vector<unsigned>& use_image_texture_targets);
  ~ResourceProvider() override;

  void Initialize();

  void DidLoseOutputSurface() { lost_output_surface_ = true; }

  int max_texture_size() const { return max_texture_size_; }
  ResourceFormat best_texture_format() const { return best_texture_format_; }
  ResourceFormat best_render_buffer_format() const {
    return best_render_buffer_format_;
  }
  ResourceFormat YuvResourceFormat(int bits) const;
  bool use_sync_query() const { return use_sync_query_; }
  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager() {
    return gpu_memory_buffer_manager_;
  }
  size_t num_resources() const { return resources_.size(); }

  bool IsResourceFormatSupported(ResourceFormat format) const;

  // Checks whether a resource is in use by a consumer.
  bool InUseByConsumer(ResourceId id);

  bool IsLost(ResourceId id);

  void LoseResourceForTesting(ResourceId id);
  void EnableReadLockFencesForTesting(ResourceId id);

  // Producer interface.

  ResourceType default_resource_type() const { return default_resource_type_; }
  ResourceType GetResourceType(ResourceId id);
  GLenum GetResourceTextureTarget(ResourceId id);
  TextureHint GetTextureHint(ResourceId id);

  // Creates a resource of the default resource type.
  ResourceId CreateResource(const gfx::Size& size,
                            TextureHint hint,
                            ResourceFormat format);

  // Creates a resource for a particular texture target (the distinction between
  // texture targets has no effect in software mode).
  ResourceId CreateGpuMemoryBufferResource(const gfx::Size& size,
                                           TextureHint hint,
                                           ResourceFormat format);

  // Wraps an external texture mailbox into a GL resource.
  ResourceId CreateResourceFromTextureMailbox(
      const TextureMailbox& mailbox,
      std::unique_ptr<SingleReleaseCallbackImpl> release_callback_impl);

  ResourceId CreateResourceFromTextureMailbox(
      const TextureMailbox& mailbox,
      std::unique_ptr<SingleReleaseCallbackImpl> release_callback_impl,
      bool read_lock_fences_enabled);

  void DeleteResource(ResourceId id);

  // Update pixels from image, copying source_rect (in image) to dest_offset (in
  // the resource).
  void CopyToResource(ResourceId id,
                      const uint8_t* image,
                      const gfx::Size& image_size);

  // Generates sync tokesn for resources which need a sync token.
  void GenerateSyncTokenForResource(ResourceId resource_id);
  void GenerateSyncTokenForResources(const ResourceIdArray& resource_ids);

  // Creates accounting for a child. Returns a child ID.
  int CreateChild(const ReturnCallback& return_callback);

  // Destroys accounting for the child, deleting all accounted resources.
  void DestroyChild(int child);

  // Sets whether resources need sync points set on them when returned to this
  // child. Defaults to true.
  void SetChildNeedsSyncTokens(int child, bool needs_sync_tokens);

  // Gets the child->parent resource ID map.
  const ResourceIdMap& GetChildToParentMap(int child) const;

  // Prepares resources to be transfered to the parent, moving them to
  // mailboxes and serializing meta-data into TransferableResources.
  // Resources are not removed from the ResourceProvider, but are marked as
  // "in use".
  void PrepareSendToParent(const ResourceIdArray& resource_ids,
                           TransferableResourceArray* transferable_resources);

  // Receives resources from a child, moving them from mailboxes. Resource IDs
  // passed are in the child namespace, and will be translated to the parent
  // namespace, added to the child->parent map.
  // This adds the resources to the working set in the ResourceProvider without
  // declaring which resources are in use. Use DeclareUsedResourcesFromChild
  // after calling this method to do that. All calls to ReceiveFromChild should
  // be followed by a DeclareUsedResourcesFromChild.
  // NOTE: if the sync_token is set on any TransferableResource, this will
  // wait on it.
  void ReceiveFromChild(
      int child, const TransferableResourceArray& transferable_resources);

  // Once a set of resources have been received, they may or may not be used.
  // This declares what set of resources are currently in use from the child,
  // releasing any other resources back to the child.
  void DeclareUsedResourcesFromChild(int child,
                                     const ResourceIdSet& resources_from_child);

  // Receives resources from the parent, moving them from mailboxes. Resource
  // IDs passed are in the child namespace.
  // NOTE: if the sync_token is set on any TransferableResource, this will
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
                     ResourceId resource_id);
    ~ScopedReadLockGL();

    unsigned texture_id() const { return texture_id_; }
    GLenum target() const { return target_; }
    const gfx::Size& size() const { return size_; }

   private:
    ResourceProvider* resource_provider_;
    ResourceId resource_id_;
    unsigned texture_id_;
    GLenum target_;
    gfx::Size size_;

    DISALLOW_COPY_AND_ASSIGN(ScopedReadLockGL);
  };

  class CC_EXPORT ScopedSamplerGL {
   public:
    ScopedSamplerGL(ResourceProvider* resource_provider,
                    ResourceId resource_id,
                    GLenum filter);
    ScopedSamplerGL(ResourceProvider* resource_provider,
                    ResourceId resource_id,
                    GLenum unit,
                    GLenum filter);
    ~ScopedSamplerGL();

    unsigned texture_id() const { return resource_lock_.texture_id(); }
    GLenum target() const { return target_; }

   private:
    ScopedReadLockGL resource_lock_;
    GLenum unit_;
    GLenum target_;

    DISALLOW_COPY_AND_ASSIGN(ScopedSamplerGL);
  };

  class CC_EXPORT ScopedWriteLockGL {
   public:
    ScopedWriteLockGL(ResourceProvider* resource_provider,
                      ResourceId resource_id,
                      bool create_mailbox);
    ~ScopedWriteLockGL();

    unsigned texture_id() const { return texture_id_; }
    GLenum target() const { return target_; }
    ResourceFormat format() const { return format_; }
    const gfx::Size& size() const { return size_; }

    const TextureMailbox& mailbox() const { return mailbox_; }

    void set_sync_token(const gpu::SyncToken& sync_token) {
      sync_token_ = sync_token;
    }

    void set_synchronized(bool synchronized) { synchronized_ = synchronized; }

   private:
    ResourceProvider* resource_provider_;
    ResourceId resource_id_;
    unsigned texture_id_;
    GLenum target_;
    ResourceFormat format_;
    gfx::Size size_;
    TextureMailbox mailbox_;
    gpu::SyncToken sync_token_;
    bool synchronized_;
    base::ThreadChecker thread_checker_;

    DISALLOW_COPY_AND_ASSIGN(ScopedWriteLockGL);
  };

  class CC_EXPORT ScopedTextureProvider {
   public:
    ScopedTextureProvider(gpu::gles2::GLES2Interface* gl,
                          ScopedWriteLockGL* resource_lock,
                          bool use_mailbox);
    ~ScopedTextureProvider();

    unsigned texture_id() const { return texture_id_; }

   private:
    gpu::gles2::GLES2Interface* gl_;
    bool use_mailbox_;
    unsigned texture_id_;

    DISALLOW_COPY_AND_ASSIGN(ScopedTextureProvider);
  };

  class CC_EXPORT ScopedSkSurfaceProvider {
   public:
    ScopedSkSurfaceProvider(ContextProvider* context_provider,
                            ScopedWriteLockGL* resource_lock,
                            bool use_mailbox,
                            bool use_distance_field_text,
                            bool can_use_lcd_text,
                            int msaa_sample_count);
    ~ScopedSkSurfaceProvider();

    SkSurface* sk_surface() { return sk_surface_.get(); }

   private:
    ScopedTextureProvider texture_provider_;
    sk_sp<SkSurface> sk_surface_;

    DISALLOW_COPY_AND_ASSIGN(ScopedSkSurfaceProvider);
  };

  class CC_EXPORT ScopedReadLockSoftware {
   public:
    ScopedReadLockSoftware(ResourceProvider* resource_provider,
                           ResourceId resource_id);
    ~ScopedReadLockSoftware();

    const SkBitmap* sk_bitmap() const {
      DCHECK(valid());
      return &sk_bitmap_;
    }

    bool valid() const { return !!sk_bitmap_.getPixels(); }

   private:
    ResourceProvider* resource_provider_;
    ResourceId resource_id_;
    SkBitmap sk_bitmap_;

    DISALLOW_COPY_AND_ASSIGN(ScopedReadLockSoftware);
  };

  class CC_EXPORT ScopedReadLockSkImage {
   public:
    ScopedReadLockSkImage(ResourceProvider* resource_provider,
                          ResourceId resource_id);
    ~ScopedReadLockSkImage();

    const SkImage* sk_image() const { return sk_image_.get(); }

    bool valid() const { return !!sk_image_; }

   private:
    ResourceProvider* resource_provider_;
    ResourceId resource_id_;
    sk_sp<SkImage> sk_image_;

    DISALLOW_COPY_AND_ASSIGN(ScopedReadLockSkImage);
  };

  class CC_EXPORT ScopedWriteLockSoftware {
   public:
    ScopedWriteLockSoftware(ResourceProvider* resource_provider,
                            ResourceId resource_id);
    ~ScopedWriteLockSoftware();

    SkBitmap& sk_bitmap() { return sk_bitmap_; }
    bool valid() const { return !!sk_bitmap_.getPixels(); }

   private:
    ResourceProvider* resource_provider_;
    ResourceId resource_id_;
    SkBitmap sk_bitmap_;
    base::ThreadChecker thread_checker_;

    DISALLOW_COPY_AND_ASSIGN(ScopedWriteLockSoftware);
  };

  class CC_EXPORT ScopedWriteLockGpuMemoryBuffer {
   public:
    ScopedWriteLockGpuMemoryBuffer(ResourceProvider* resource_provider,
                                   ResourceId resource_id);
    ~ScopedWriteLockGpuMemoryBuffer();

    gfx::GpuMemoryBuffer* GetGpuMemoryBuffer();

   private:
    ResourceProvider* resource_provider_;
    ResourceId resource_id_;
    ResourceFormat format_;
    gfx::Size size_;
    std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer_;
    base::ThreadChecker thread_checker_;

    DISALLOW_COPY_AND_ASSIGN(ScopedWriteLockGpuMemoryBuffer);
  };

  class Fence : public base::RefCounted<Fence> {
   public:
    Fence() {}

    virtual void Set() = 0;
    virtual bool HasPassed() = 0;
    virtual void Wait() = 0;

   protected:
    friend class base::RefCounted<Fence>;
    virtual ~Fence() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Fence);
  };

  class SynchronousFence : public ResourceProvider::Fence {
   public:
    explicit SynchronousFence(gpu::gles2::GLES2Interface* gl);

    // Overridden from Fence:
    void Set() override;
    bool HasPassed() override;
    void Wait() override;

    // Returns true if fence has been set but not yet synchornized.
    bool has_synchronized() const { return has_synchronized_; }

   private:
    ~SynchronousFence() override;

    void Synchronize();

    gpu::gles2::GLES2Interface* gl_;
    bool has_synchronized_;

    DISALLOW_COPY_AND_ASSIGN(SynchronousFence);
  };

  // Acquire pixel buffer for resource. The pixel buffer can be used to
  // set resource pixels without performing unnecessary copying.
  void AcquirePixelBuffer(ResourceId resource);
  void ReleasePixelBuffer(ResourceId resource);
  // Map/unmap the acquired pixel buffer.
  uint8_t* MapPixelBuffer(ResourceId id, int* stride);
  void UnmapPixelBuffer(ResourceId id);
  // Asynchronously update pixels from acquired pixel buffer.
  void BeginSetPixels(ResourceId id);
  void ForceSetPixelsToComplete(ResourceId id);
  bool DidSetPixelsComplete(ResourceId id);

  // For tests only! This prevents detecting uninitialized reads.
  // Use SetPixels or LockForWrite to allocate implicitly.
  void AllocateForTesting(ResourceId id);

  // For tests only!
  void CreateForTesting(ResourceId id);

  // Sets the current read fence. If a resource is locked for read
  // and has read fences enabled, the resource will not allow writes
  // until this fence has passed.
  void SetReadLockFence(Fence* fence) { current_read_lock_fence_ = fence; }

  // Indicates if we can currently lock this resource for write.
  bool CanLockForWrite(ResourceId id);

  // Indicates if this resource may be used for a hardware overlay plane.
  bool IsOverlayCandidate(ResourceId id);

  void WaitSyncTokenIfNeeded(ResourceId id);

  static GLint GetActiveTextureUnit(gpu::gles2::GLES2Interface* gl);

  void ValidateResource(ResourceId id) const;

  GLenum GetImageTextureTarget(ResourceFormat format);

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  int tracing_id() const { return tracing_id_; }

 private:
  struct Resource {
    enum Origin { INTERNAL, EXTERNAL, DELEGATED };
    enum SynchronizationState {
      // The LOCALLY_USED state is the state each resource defaults to when
      // constructed or modified or read. This state indicates that the
      // resource has not been properly synchronized and it would be an error
      // to send this resource to a parent, child, or client.
      LOCALLY_USED,

      // The NEEDS_WAIT state is the state that indicates a resource has been
      // modified but it also has an associated sync token assigned to it.
      // The sync token has not been waited on with the local context. When
      // a sync token arrives from an external resource (such as a child or
      // parent), it is automatically initialized as NEEDS_WAIT as well
      // since we still need to wait on it before the resource is synchronized
      // on the current context. It is an error to use the resource locally for
      // reading or writing if the resource is in this state.
      NEEDS_WAIT,

      // The SYNCHRONIZED state indicates that the resource has been properly
      // synchronized locally. This can either synchronized externally (such
      // as the case of software rasterized bitmaps), or synchronized
      // internally using a sync token that has been waited upon. In the
      // former case where the resource was synchronized externally, a
      // corresponding sync token will not exist. In the latter case which was
      // synchronized from the NEEDS_WAIT state, a corresponding sync token will
      // exist which is assocaited with the resource. This sync token is still
      // valid and still associated with the resource and can be passed as an
      // external resource for others to wait on.
      SYNCHRONIZED,
    };

    ~Resource();
    Resource(unsigned texture_id,
             const gfx::Size& size,
             Origin origin,
             GLenum target,
             GLenum filter,
             TextureHint hint,
             ResourceType type,
             ResourceFormat format);
    Resource(uint8_t* pixels,
             SharedBitmap* bitmap,
             const gfx::Size& size,
             Origin origin,
             GLenum filter);
    Resource(const SharedBitmapId& bitmap_id,
             const gfx::Size& size,
             Origin origin,
             GLenum filter);
    Resource(Resource&& other);

    bool needs_sync_token() const { return needs_sync_token_; }

    SynchronizationState synchronization_state() const {
      return synchronization_state_;
    }

    const TextureMailbox& mailbox() const { return mailbox_; }
    void set_mailbox(const TextureMailbox& mailbox);

    void SetLocallyUsed();
    void SetSynchronized();
    void UpdateSyncToken(const gpu::SyncToken& sync_token);
    int8_t* GetSyncTokenData();
    void WaitSyncToken(gpu::gles2::GLES2Interface* gl);

    int child_id;
    unsigned gl_id;
    // Pixel buffer used for set pixels without unnecessary copying.
    unsigned gl_pixel_buffer_id;
    // Query used to determine when asynchronous set pixels complete.
    unsigned gl_upload_query_id;
    // Query used to determine when read lock fence has passed.
    unsigned gl_read_lock_query_id;
    ReleaseCallbackImpl release_callback_impl;
    uint8_t* pixels;
    int lock_for_read_count;
    int imported_count;
    int exported_count;
    bool dirty_image : 1;
    bool locked_for_write : 1;
    bool lost : 1;
    bool marked_for_deletion : 1;
    bool allocated : 1;
    bool read_lock_fences_enabled : 1;
    bool has_shared_bitmap_id : 1;
    bool is_overlay_candidate : 1;
    scoped_refptr<Fence> read_lock_fence;
    gfx::Size size;
    Origin origin;
    GLenum target;
    // TODO(skyostil): Use a separate sampler object for filter state.
    GLenum original_filter;
    GLenum filter;
    unsigned image_id;
    unsigned bound_image_id;
    TextureHint hint;
    ResourceType type;
    ResourceFormat format;
    SharedBitmapId shared_bitmap_id;
    SharedBitmap* shared_bitmap;
    std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer;

   private:
    SynchronizationState synchronization_state_ = SYNCHRONIZED;
    bool needs_sync_token_ = false;
    TextureMailbox mailbox_;

    DISALLOW_COPY_AND_ASSIGN(Resource);
  };
  using ResourceMap = std::unordered_map<ResourceId, Resource>;

  struct Child {
    Child();
    Child(const Child& other);
    ~Child();

    ResourceIdMap child_to_parent_map;
    ResourceIdMap parent_to_child_map;
    ReturnCallback return_callback;
    bool marked_for_deletion;
    bool needs_sync_tokens;
  };
  using ChildMap = std::unordered_map<int, Child>;

  bool ReadLockFenceHasPassed(const Resource* resource) {
    return !resource->read_lock_fence.get() ||
           resource->read_lock_fence->HasPassed();
  }

  ResourceId CreateGLTexture(const gfx::Size& size,
                             TextureHint hint,
                             ResourceType type,
                             ResourceFormat format);
  ResourceId CreateBitmap(const gfx::Size& size);
  Resource* InsertResource(ResourceId id, Resource resource);
  Resource* GetResource(ResourceId id);
  const Resource* LockForRead(ResourceId id);
  void UnlockForRead(ResourceId id);
  Resource* LockForWrite(ResourceId id);
  void UnlockForWrite(Resource* resource);

  static void PopulateSkBitmapWithResource(SkBitmap* sk_bitmap,
                                           const Resource* resource);

  void CreateMailboxAndBindResource(gpu::gles2::GLES2Interface* gl,
                                    Resource* resource);

  void TransferResource(Resource* source,
                        ResourceId id,
                        TransferableResource* resource);
  enum DeleteStyle {
    NORMAL,
    FOR_SHUTDOWN,
  };
  void DeleteResourceInternal(ResourceMap::iterator it, DeleteStyle style);
  void DeleteAndReturnUnusedResourcesToChild(ChildMap::iterator child_it,
                                             DeleteStyle style,
                                             const ResourceIdArray& unused);
  void DestroyChildInternal(ChildMap::iterator it, DeleteStyle style);
  void LazyCreate(Resource* resource);
  void LazyAllocate(Resource* resource);
  void LazyCreateImage(Resource* resource);

  void BindImageForSampling(Resource* resource);
  // Binds the given GL resource to a texture target for sampling using the
  // specified filter for both minification and magnification. Returns the
  // texture target used. The resource must be locked for reading.
  GLenum BindForSampling(ResourceId resource_id, GLenum unit, GLenum filter);

  // Returns null if we do not have a ContextProvider.
  gpu::gles2::GLES2Interface* ContextGL() const;
  bool IsGLContextLost() const;

  ContextProvider* compositor_context_provider_;
  SharedBitmapManager* shared_bitmap_manager_;
  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager_;
  BlockingTaskRunner* blocking_main_thread_task_runner_;
  bool lost_output_surface_;
  int highp_threshold_min_;
  ResourceId next_id_;
  ResourceMap resources_;
  int next_child_;
  ChildMap children_;

  const bool delegated_sync_points_required_;

  ResourceType default_resource_type_;
  bool use_texture_storage_ext_;
  bool use_texture_format_bgra_;
  bool use_texture_usage_hint_;
  bool use_compressed_texture_etc1_;
  ResourceFormat yuv_resource_format_;
  ResourceFormat yuv_highbit_resource_format_;
  int max_texture_size_;
  ResourceFormat best_texture_format_;
  ResourceFormat best_render_buffer_format_;

  base::ThreadChecker thread_checker_;

  scoped_refptr<Fence> current_read_lock_fence_;

  const size_t id_allocation_chunk_size_;
  std::unique_ptr<IdAllocator> texture_id_allocator_;
  std::unique_ptr<IdAllocator> buffer_id_allocator_;

  bool use_sync_query_;
  std::vector<unsigned> use_image_texture_targets_;

  // A process-unique ID used for disambiguating memory dumps from different
  // resource providers.
  int tracing_id_;

  DISALLOW_COPY_AND_ASSIGN(ResourceProvider);
};

}  // namespace cc

#endif  // CC_RESOURCES_RESOURCE_PROVIDER_H_
