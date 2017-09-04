// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_MESSAGE_FILTER_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/shared_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "cc/resources/shared_bitmap_manager.h"
#include "components/discardable_memory/service/discardable_shared_memory_manager.h"
#include "content/common/cache_storage/cache_storage_types.h"
#include "content/common/host_shared_bitmap_manager.h"
#include "content/common/render_message_filter.mojom.h"
#include "content/public/browser/browser_associated_interface.h"
#include "content/public/browser/browser_message_filter.h"
#include "gpu/config/gpu_info.h"
#include "ipc/message_filter.h"
#include "third_party/WebKit/public/web/WebPopupType.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/surface/transport_dib.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#if defined(OS_MACOSX)
#include "content/common/mac/font_loader.h"
#endif

#if defined(OS_ANDROID)
#include "base/threading/worker_pool.h"
#endif

class GURL;
struct FontDescriptor;

namespace base {
class SharedMemory;
}

namespace gfx {
struct GpuMemoryBufferHandle;
}

namespace gpu {
struct SyncToken;
}

namespace media {
struct MediaLogEvent;
}

namespace net {
class IOBuffer;
class KeygenHandler;
class URLRequestContext;
class URLRequestContextGetter;
}

namespace url {
class Origin;
}

namespace content {
class BrowserContext;
class CacheStorageContextImpl;
class CacheStorageCacheHandle;
class DOMStorageContextWrapper;
class MediaInternals;
class RenderWidgetHelper;
class ResourceContext;
class ResourceDispatcherHostImpl;

// This class filters out incoming IPC messages for the renderer process on the
// IPC thread.
class CONTENT_EXPORT RenderMessageFilter
    : public BrowserMessageFilter,
      public BrowserAssociatedInterface<mojom::RenderMessageFilter>,
      public mojom::RenderMessageFilter {
 public:
  // Create the filter.
  RenderMessageFilter(int render_process_id,
                      BrowserContext* browser_context,
                      net::URLRequestContextGetter* request_context,
                      RenderWidgetHelper* render_widget_helper,
                      MediaInternals* media_internals,
                      DOMStorageContextWrapper* dom_storage_context,
                      CacheStorageContextImpl* cache_storage_context);

  // BrowserMessageFilter methods:
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnDestruct() const override;
  void OverrideThreadForMessage(const IPC::Message& message,
                                BrowserThread::ID* thread) override;

  int render_process_id() const { return render_process_id_; }

 protected:
  ~RenderMessageFilter() override;

 private:
  friend class BrowserThread;
  friend class base::DeleteHelper<RenderMessageFilter>;

  void OnGetProcessMemorySizes(size_t* private_bytes, size_t* shared_bytes);

#if defined(OS_MACOSX)
  // Messages for OOP font loading.
  void OnLoadFont(const FontDescriptor& font, IPC::Message* reply_msg);
  void SendLoadFontReply(IPC::Message* reply, FontLoader::Result* result);
#endif

  // mojom::RenderMessageFilter:
  void GenerateRoutingID(const GenerateRoutingIDCallback& routing_id) override;
  void CreateNewWindow(mojom::CreateNewWindowParamsPtr params,
                       const CreateNewWindowCallback& callback) override;
  void CreateNewWidget(int32_t opener_id,
                       blink::WebPopupType popup_type,
                       const CreateNewWidgetCallback& callback) override;
  void CreateFullscreenWidget(
      int opener_id,
      const CreateFullscreenWidgetCallback& callback) override;

  // Message handlers called on the browser IO thread:
  void OnEstablishGpuChannel(IPC::Message* reply);
  void OnHasGpuProcess(IPC::Message* reply);
  // Helper callbacks for the message handlers.
  void EstablishChannelCallback(std::unique_ptr<IPC::Message> reply,
                                const IPC::ChannelHandle& channel,
                                const gpu::GPUInfo& gpu_info);
  void GetGpuProcessHandlesCallback(
      std::unique_ptr<IPC::Message> reply,
      const std::list<base::ProcessHandle>& handles);
  // Used to ask the browser to allocate a block of shared memory for the
  // renderer to send back data in, since shared memory can't be created
  // in the renderer on POSIX due to the sandbox.
  void AllocateSharedBitmapOnFileThread(uint32_t buffer_size,
                                        const cc::SharedBitmapId& id,
                                        IPC::Message* reply_msg);
  void OnAllocateSharedBitmap(uint32_t buffer_size,
                              const cc::SharedBitmapId& id,
                              IPC::Message* reply_msg);
  void OnAllocatedSharedBitmap(size_t buffer_size,
                               const base::SharedMemoryHandle& handle,
                               const cc::SharedBitmapId& id);
  void OnDeletedSharedBitmap(const cc::SharedBitmapId& id);
  void OnResolveProxy(const GURL& url, IPC::Message* reply_msg);

  // Browser side discardable shared memory allocation.
  void AllocateLockedDiscardableSharedMemoryOnFileThread(
      uint32_t size,
      discardable_memory::DiscardableSharedMemoryId id,
      IPC::Message* reply_message);
  void OnAllocateLockedDiscardableSharedMemory(
      uint32_t size,
      discardable_memory::DiscardableSharedMemoryId id,
      IPC::Message* reply_message);
  void DeletedDiscardableSharedMemoryOnFileThread(
      discardable_memory::DiscardableSharedMemoryId id);
  void OnDeletedDiscardableSharedMemory(
      discardable_memory::DiscardableSharedMemoryId id);

#if defined(OS_LINUX)
  void SetThreadPriorityOnFileThread(base::PlatformThreadId ns_tid,
                                     base::ThreadPriority priority);
  void OnSetThreadPriority(base::PlatformThreadId ns_tid,
                           base::ThreadPriority priority);
#endif

  void OnCacheableMetadataAvailable(const GURL& url,
                                    base::Time expected_response_time,
                                    const std::vector<char>& data);
  void OnCacheableMetadataAvailableForCacheStorage(
      const GURL& url,
      base::Time expected_response_time,
      const std::vector<char>& data,
      const url::Origin& cache_storage_origin,
      const std::string& cache_storage_cache_name);
  void OnCacheStorageOpenCallback(
      const GURL& url,
      base::Time expected_response_time,
      scoped_refptr<net::IOBuffer> buf,
      int buf_len,
      std::unique_ptr<CacheStorageCacheHandle> cache_handle,
      CacheStorageError error);
  void OnKeygen(uint32_t key_size_index,
                const std::string& challenge_string,
                const GURL& url,
                const GURL& top_origin,
                IPC::Message* reply_msg);
  void PostKeygenToWorkerThread(
      IPC::Message* reply_msg,
      std::unique_ptr<net::KeygenHandler> keygen_handler);
  void OnKeygenOnWorkerThread(
      std::unique_ptr<net::KeygenHandler> keygen_handler,
      IPC::Message* reply_msg);
  void OnMediaLogEvents(const std::vector<media::MediaLogEvent>&);

  bool CheckBenchmarkingEnabled() const;
  bool CheckPreparsedJsCachingEnabled() const;

  void OnAllocateGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                                 uint32_t width,
                                 uint32_t height,
                                 gfx::BufferFormat format,
                                 gfx::BufferUsage usage,
                                 IPC::Message* reply);
  void GpuMemoryBufferAllocated(IPC::Message* reply,
                                const gfx::GpuMemoryBufferHandle& handle);
  void OnDeletedGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                                const gpu::SyncToken& sync_token);

  // Cached resource request dispatcher host, guaranteed to be non-null. We do
  // not own it; it is managed by the BrowserProcess, which has a wider scope
  // than we do.
  ResourceDispatcherHostImpl* resource_dispatcher_host_;

  HostSharedBitmapManagerClient bitmap_manager_client_;

  // Contextual information to be used for requests created here.
  scoped_refptr<net::URLRequestContextGetter> request_context_;

  // The ResourceContext which is to be used on the IO thread.
  ResourceContext* resource_context_;

  scoped_refptr<RenderWidgetHelper> render_widget_helper_;

  scoped_refptr<DOMStorageContextWrapper> dom_storage_context_;

  int gpu_process_id_;
  int render_process_id_;

  MediaInternals* media_internals_;
  CacheStorageContextImpl* cache_storage_context_;

  base::WeakPtrFactory<RenderMessageFilter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_MESSAGE_FILTER_H_
