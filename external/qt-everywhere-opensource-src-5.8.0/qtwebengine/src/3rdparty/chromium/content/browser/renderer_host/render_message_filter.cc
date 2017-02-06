// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_message_filter.h"

#include <errno.h>
#include <string.h>
#include <map>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/macros.h"
#include "base/numerics/safe_math.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "base/threading/worker_pool.h"
#include "build/build_config.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/cache_storage/cache_storage_cache.h"
#include "content/browser/cache_storage/cache_storage_cache_handle.h"
#include "content/browser/cache_storage/cache_storage_context_impl.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/browser/download/download_stats.h"
#include "content/browser/gpu/browser_gpu_memory_buffer_manager.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/media/media_internals.h"
#include "content/browser/renderer_host/pepper/pepper_security_helper.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_widget_helper.h"
#include "content/browser/resource_context_impl.h"
#include "content/common/cache_storage/cache_storage_types.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/child_process_messages.h"
#include "content/common/content_constants_internal.h"
#include "content/common/host_shared_bitmap_manager.h"
#include "content/common/render_process_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/common/url_constants.h"
#include "gpu/ipc/client/gpu_memory_buffer_impl.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_platform_file.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_manager.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_log_event.h"
#include "net/base/io_buffer.h"
#include "net/base/keygen_handler.h"
#include "net/base/mime_util.h"
#include "net/base/request_priority.h"
#include "net/http/http_cache.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "ppapi/shared_impl/file_type_conversion.h"
#include "url/gurl.h"

#if defined(OS_MACOSX)
#include "content/common/mac/font_descriptor.h"
#endif

#if defined(OS_WIN)
#include "content/common/font_cache_dispatcher_win.h"
#endif

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

#if defined(OS_ANDROID)
#include "content/browser/media/android/media_throttler.h"
#endif

#if defined(OS_MACOSX)
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#endif

namespace content {
namespace {

const uint32_t kFilteredMessageClasses[] = {
    ChildProcessMsgStart, RenderProcessMsgStart, ViewMsgStart,
};

#if defined(OS_MACOSX)
void ResizeHelperHandleMsgOnUIThread(int render_process_id,
                                     const IPC::Message& message) {
  RenderProcessHost* host = RenderProcessHost::FromID(render_process_id);
  if (host)
    host->OnMessageReceived(message);
}

void ResizeHelperPostMsgToUIThread(int render_process_id,
                                   const IPC::Message& msg) {
  ui::WindowResizeHelperMac::Get()->task_runner()->PostDelayedTask(
      FROM_HERE,
      base::Bind(ResizeHelperHandleMsgOnUIThread, render_process_id, msg),
      base::TimeDelta());
}
#endif

void NoOpCacheStorageErrorCallback(
    std::unique_ptr<CacheStorageCacheHandle> cache_handle,
    CacheStorageError error) {}

}  // namespace

RenderMessageFilter::RenderMessageFilter(
    int render_process_id,
    BrowserContext* browser_context,
    net::URLRequestContextGetter* request_context,
    RenderWidgetHelper* render_widget_helper,
    media::AudioManager* audio_manager,
    MediaInternals* media_internals,
    DOMStorageContextWrapper* dom_storage_context,
    CacheStorageContextImpl* cache_storage_context)
    : BrowserMessageFilter(kFilteredMessageClasses,
                           arraysize(kFilteredMessageClasses)),
      resource_dispatcher_host_(ResourceDispatcherHostImpl::Get()),
      bitmap_manager_client_(HostSharedBitmapManager::current()),
      request_context_(request_context),
      resource_context_(browser_context->GetResourceContext()),
      render_widget_helper_(render_widget_helper),
      dom_storage_context_(dom_storage_context),
      gpu_process_id_(0),
      render_process_id_(render_process_id),
      audio_manager_(audio_manager),
      media_internals_(media_internals),
      cache_storage_context_(cache_storage_context),
      weak_ptr_factory_(this) {
  DCHECK(request_context_.get());

  if (render_widget_helper)
    render_widget_helper_->Init(render_process_id_, resource_dispatcher_host_);
}

RenderMessageFilter::~RenderMessageFilter() {
  // This function should be called on the IO thread.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserGpuMemoryBufferManager* gpu_memory_buffer_manager =
      BrowserGpuMemoryBufferManager::current();
  if (gpu_memory_buffer_manager)
    gpu_memory_buffer_manager->ProcessRemoved(PeerHandle(), render_process_id_);
  HostDiscardableSharedMemoryManager::current()->ProcessRemoved(
      render_process_id_);
}

bool RenderMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderMessageFilter, message)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GenerateRoutingID, OnGenerateRoutingID)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CreateWindow, OnCreateWindow)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CreateWidget, OnCreateWidget)
    IPC_MESSAGE_HANDLER(ViewHostMsg_CreateFullscreenWidget,
                        OnCreateFullscreenWidget)
#if defined(OS_MACOSX)
    // On Mac, the IPCs ViewHostMsg_SwapCompositorFrame, ViewHostMsg_UpdateRect,
    // and GpuCommandBufferMsg_SwapBuffersCompleted need to be handled in a
    // nested message loop during resize.
    IPC_MESSAGE_HANDLER_GENERIC(
        ViewHostMsg_SwapCompositorFrame,
        ResizeHelperPostMsgToUIThread(render_process_id_, message))
    IPC_MESSAGE_HANDLER_GENERIC(
        ViewHostMsg_UpdateRect,
        ResizeHelperPostMsgToUIThread(render_process_id_, message))
#endif
    // NB: The SyncAllocateSharedMemory, SyncAllocateGpuMemoryBuffer, and
    // DeletedGpuMemoryBuffer IPCs are handled here for renderer processes. For
    // non-renderer child processes, they are handled in ChildProcessHostImpl.
    IPC_MESSAGE_HANDLER_DELAY_REPLY(
        ChildProcessHostMsg_SyncAllocateSharedMemory, OnAllocateSharedMemory)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(
        ChildProcessHostMsg_SyncAllocateSharedBitmap, OnAllocateSharedBitmap)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(
        ChildProcessHostMsg_SyncAllocateGpuMemoryBuffer,
        OnAllocateGpuMemoryBuffer)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ChildProcessHostMsg_EstablishGpuChannel,
                                    OnEstablishGpuChannel)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ChildProcessHostMsg_HasGpuProcess,
                                    OnHasGpuProcess)
    IPC_MESSAGE_HANDLER(ChildProcessHostMsg_DeletedGpuMemoryBuffer,
                        OnDeletedGpuMemoryBuffer)
    IPC_MESSAGE_HANDLER(ChildProcessHostMsg_AllocatedSharedBitmap,
                        OnAllocatedSharedBitmap)
    IPC_MESSAGE_HANDLER(ChildProcessHostMsg_DeletedSharedBitmap,
                        OnDeletedSharedBitmap)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(
        ChildProcessHostMsg_SyncAllocateLockedDiscardableSharedMemory,
        OnAllocateLockedDiscardableSharedMemory)
    IPC_MESSAGE_HANDLER(ChildProcessHostMsg_DeletedDiscardableSharedMemory,
                        OnDeletedDiscardableSharedMemory)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(RenderProcessHostMsg_Keygen, OnKeygen)
    IPC_MESSAGE_HANDLER(RenderProcessHostMsg_DidGenerateCacheableMetadata,
                        OnCacheableMetadataAvailable)
    IPC_MESSAGE_HANDLER(
        RenderProcessHostMsg_DidGenerateCacheableMetadataInCacheStorage,
        OnCacheableMetadataAvailableForCacheStorage)
    IPC_MESSAGE_HANDLER(ViewHostMsg_GetAudioHardwareConfig,
                        OnGetAudioHardwareConfig)
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(RenderProcessHostMsg_LoadFont, OnLoadFont)
#elif defined(OS_WIN)
    IPC_MESSAGE_HANDLER(RenderProcessHostMsg_PreCacheFontCharacters,
                        OnPreCacheFontCharacters)
#endif
    IPC_MESSAGE_HANDLER(ViewHostMsg_MediaLogEvents, OnMediaLogEvents)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void RenderMessageFilter::OnDestruct() const {
  const_cast<RenderMessageFilter*>(this)->resource_context_ = nullptr;
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

void RenderMessageFilter::OverrideThreadForMessage(const IPC::Message& message,
                                                   BrowserThread::ID* thread) {
  if (message.type() == ViewHostMsg_MediaLogEvents::ID)
    *thread = BrowserThread::UI;
}

base::TaskRunner* RenderMessageFilter::OverrideTaskRunnerForMessage(
    const IPC::Message& message) {
  // Always query audio device parameters on the audio thread.
  if (message.type() == ViewHostMsg_GetAudioHardwareConfig::ID)
    return audio_manager_->GetTaskRunner();
  return NULL;
}

void RenderMessageFilter::OnCreateWindow(
    const ViewHostMsg_CreateWindow_Params& params,
    ViewHostMsg_CreateWindow_Reply* reply) {
  bool no_javascript_access;

  bool can_create_window =
      GetContentClient()->browser()->CanCreateWindow(
          params.opener_url,
          params.opener_top_level_frame_url,
          params.opener_security_origin,
          params.window_container_type,
          params.target_url,
          params.referrer,
          params.disposition,
          params.features,
          params.user_gesture,
          params.opener_suppressed,
          resource_context_,
          render_process_id_,
          params.opener_id,
          params.opener_render_frame_id,
          &no_javascript_access);

  if (!can_create_window) {
    reply->route_id = MSG_ROUTING_NONE;
    reply->main_frame_route_id = MSG_ROUTING_NONE;
    reply->main_frame_widget_route_id = MSG_ROUTING_NONE;
    reply->cloned_session_storage_namespace_id = 0;
    return;
  }

  // This will clone the sessionStorage for namespace_id_to_clone.
  scoped_refptr<SessionStorageNamespaceImpl> cloned_namespace =
      new SessionStorageNamespaceImpl(dom_storage_context_.get(),
                                      params.session_storage_namespace_id);
  reply->cloned_session_storage_namespace_id = cloned_namespace->id();

  render_widget_helper_->CreateNewWindow(
      params, no_javascript_access, PeerHandle(), &reply->route_id,
      &reply->main_frame_route_id, &reply->main_frame_widget_route_id,
      cloned_namespace.get());
}

void RenderMessageFilter::OnCreateWidget(int opener_id,
                                         blink::WebPopupType popup_type,
                                         int* route_id) {
  render_widget_helper_->CreateNewWidget(opener_id, popup_type, route_id);
}

void RenderMessageFilter::OnCreateFullscreenWidget(int opener_id,
                                                   int* route_id) {
  render_widget_helper_->CreateNewFullscreenWidget(opener_id, route_id);
}

void RenderMessageFilter::OnGenerateRoutingID(int* route_id) {
  *route_id = render_widget_helper_->GetNextRoutingID();
}

void RenderMessageFilter::OnGetAudioHardwareConfig(
    media::AudioParameters* input_params,
    media::AudioParameters* output_params) {
  DCHECK(input_params);
  DCHECK(output_params);
  *output_params = audio_manager_->GetDefaultOutputStreamParameters();

  // TODO(henrika): add support for all available input devices.
  *input_params = audio_manager_->GetInputStreamParameters(
      media::AudioDeviceDescription::kDefaultDeviceId);
}

#if defined(OS_MACOSX)

void RenderMessageFilter::OnLoadFont(const FontDescriptor& font,
                                          IPC::Message* reply_msg) {
  FontLoader::Result* result = new FontLoader::Result;

  BrowserThread::PostTaskAndReply(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&FontLoader::LoadFont, font, result),
      base::Bind(&RenderMessageFilter::SendLoadFontReply, this, reply_msg,
                 base::Owned(result)));
}

void RenderMessageFilter::SendLoadFontReply(IPC::Message* reply,
                                                 FontLoader::Result* result) {
  base::SharedMemoryHandle handle;
  if (result->font_data_size == 0 || result->font_id == 0) {
    result->font_data_size = 0;
    result->font_id = 0;
    handle = base::SharedMemory::NULLHandle();
  } else {
    result->font_data.GiveToProcess(base::GetCurrentProcessHandle(), &handle);
  }
  RenderProcessHostMsg_LoadFont::WriteReplyParams(
      reply, result->font_data_size, handle, result->font_id);
  Send(reply);
}

#elif defined(OS_WIN)

void RenderMessageFilter::OnPreCacheFontCharacters(
    const LOGFONT& font,
    const base::string16& str) {
  // TODO(scottmg): pdf/ppapi still require the renderer to be able to precache
  // GDI fonts (http://crbug.com/383227), even when using DirectWrite.
  // Eventually this shouldn't be added and should be moved to
  // FontCacheDispatcher too. http://crbug.com/356346.

  // First, comments from FontCacheDispatcher::OnPreCacheFont do apply here too.
  // Except that for True Type fonts,
  // GetTextMetrics will not load the font in memory.
  // The only way windows seem to load properly, it is to create a similar
  // device (like the one in which we print), then do an ExtTextOut,
  // as we do in the printing thread, which is sandboxed.
  HDC hdc = CreateEnhMetaFile(NULL, NULL, NULL, NULL);
  HFONT font_handle = CreateFontIndirect(&font);
  DCHECK(NULL != font_handle);

  HGDIOBJ old_font = SelectObject(hdc, font_handle);
  DCHECK(NULL != old_font);

  ExtTextOut(hdc, 0, 0, ETO_GLYPH_INDEX, 0, str.c_str(), str.length(), NULL);

  SelectObject(hdc, old_font);
  DeleteObject(font_handle);

  HENHMETAFILE metafile = CloseEnhMetaFile(hdc);

  if (metafile)
    DeleteEnhMetaFile(metafile);
}


#endif  // OS_*

void RenderMessageFilter::AllocateSharedMemoryOnFileThread(
    uint32_t buffer_size,
    IPC::Message* reply_msg) {
  base::SharedMemoryHandle handle;
  ChildProcessHostImpl::AllocateSharedMemory(buffer_size, PeerHandle(),
                                             &handle);
  ChildProcessHostMsg_SyncAllocateSharedMemory::WriteReplyParams(reply_msg,
                                                                 handle);
  Send(reply_msg);
}

void RenderMessageFilter::OnAllocateSharedMemory(uint32_t buffer_size,
                                                 IPC::Message* reply_msg) {
  BrowserThread::PostTask(
      BrowserThread::FILE_USER_BLOCKING, FROM_HERE,
      base::Bind(&RenderMessageFilter::AllocateSharedMemoryOnFileThread, this,
                 buffer_size, reply_msg));
}

void RenderMessageFilter::AllocateSharedBitmapOnFileThread(
    uint32_t buffer_size,
    const cc::SharedBitmapId& id,
    IPC::Message* reply_msg) {
  base::SharedMemoryHandle handle;
  bitmap_manager_client_.AllocateSharedBitmapForChild(PeerHandle(), buffer_size,
                                                      id, &handle);
  ChildProcessHostMsg_SyncAllocateSharedBitmap::WriteReplyParams(reply_msg,
                                                                 handle);
  Send(reply_msg);
}

void RenderMessageFilter::OnAllocateSharedBitmap(uint32_t buffer_size,
                                                 const cc::SharedBitmapId& id,
                                                 IPC::Message* reply_msg) {
  BrowserThread::PostTask(
      BrowserThread::FILE_USER_BLOCKING,
      FROM_HERE,
      base::Bind(&RenderMessageFilter::AllocateSharedBitmapOnFileThread,
                 this,
                 buffer_size,
                 id,
                 reply_msg));
}

void RenderMessageFilter::OnAllocatedSharedBitmap(
    size_t buffer_size,
    const base::SharedMemoryHandle& handle,
    const cc::SharedBitmapId& id) {
  bitmap_manager_client_.ChildAllocatedSharedBitmap(buffer_size, handle, id);
}

void RenderMessageFilter::OnDeletedSharedBitmap(const cc::SharedBitmapId& id) {
  bitmap_manager_client_.ChildDeletedSharedBitmap(id);
}

void RenderMessageFilter::AllocateLockedDiscardableSharedMemoryOnFileThread(
    uint32_t size,
    DiscardableSharedMemoryId id,
    IPC::Message* reply_msg) {
  base::SharedMemoryHandle handle;
  HostDiscardableSharedMemoryManager::current()
      ->AllocateLockedDiscardableSharedMemoryForChild(
          PeerHandle(), render_process_id_, size, id, &handle);
  ChildProcessHostMsg_SyncAllocateLockedDiscardableSharedMemory::
      WriteReplyParams(reply_msg, handle);
  Send(reply_msg);
}

void RenderMessageFilter::OnAllocateLockedDiscardableSharedMemory(
    uint32_t size,
    DiscardableSharedMemoryId id,
    IPC::Message* reply_msg) {
  BrowserThread::PostTask(
      BrowserThread::FILE_USER_BLOCKING, FROM_HERE,
      base::Bind(&RenderMessageFilter::
                     AllocateLockedDiscardableSharedMemoryOnFileThread,
                 this, size, id, reply_msg));
}

void RenderMessageFilter::DeletedDiscardableSharedMemoryOnFileThread(
    DiscardableSharedMemoryId id) {
  HostDiscardableSharedMemoryManager::current()
      ->ChildDeletedDiscardableSharedMemory(id, render_process_id_);
}

void RenderMessageFilter::OnDeletedDiscardableSharedMemory(
    DiscardableSharedMemoryId id) {
  BrowserThread::PostTask(
      BrowserThread::FILE_USER_BLOCKING, FROM_HERE,
      base::Bind(
          &RenderMessageFilter::DeletedDiscardableSharedMemoryOnFileThread,
          this, id));
}

void RenderMessageFilter::OnCacheableMetadataAvailable(
    const GURL& url,
    base::Time expected_response_time,
    const std::vector<char>& data) {
  net::HttpCache* cache = request_context_->GetURLRequestContext()->
      http_transaction_factory()->GetCache();
  if (!cache)
    return;

  // Use the same priority for the metadata write as for script
  // resources (see defaultPriorityForResourceType() in WebKit's
  // CachedResource.cpp). Note that WebURLRequest::PriorityMedium
  // corresponds to net::LOW (see ConvertWebKitPriorityToNetPriority()
  // in weburlloader_impl.cc).
  const net::RequestPriority kPriority = net::LOW;
  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(data.size()));
  if (!data.empty())
    memcpy(buf->data(), &data.front(), data.size());
  cache->WriteMetadata(url, kPriority, expected_response_time, buf.get(),
                       data.size());
}

void RenderMessageFilter::OnCacheableMetadataAvailableForCacheStorage(
    const GURL& url,
    base::Time expected_response_time,
    const std::vector<char>& data,
    const url::Origin& cache_storage_origin,
    const std::string& cache_storage_cache_name) {
  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(data.size()));
  if (!data.empty())
    memcpy(buf->data(), &data.front(), data.size());

  cache_storage_context_->cache_manager()->OpenCache(
      GURL(cache_storage_origin.Serialize()), cache_storage_cache_name,
      base::Bind(&RenderMessageFilter::OnCacheStorageOpenCallback,
                 weak_ptr_factory_.GetWeakPtr(), url, expected_response_time,
                 buf, data.size()));
}

void RenderMessageFilter::OnCacheStorageOpenCallback(
    const GURL& url,
    base::Time expected_response_time,
    scoped_refptr<net::IOBuffer> buf,
    int buf_len,
    std::unique_ptr<CacheStorageCacheHandle> cache_handle,
    CacheStorageError error) {
  if (error != CACHE_STORAGE_OK || !cache_handle || !cache_handle->value())
    return;
  CacheStorageCache* cache = cache_handle->value();
  if (!cache)
    return;
  cache->WriteSideData(base::Bind(&NoOpCacheStorageErrorCallback,
                                  base::Passed(std::move(cache_handle))),
                       url, expected_response_time, buf, buf_len);
}

void RenderMessageFilter::OnKeygen(uint32_t key_size_index,
                                   const std::string& challenge_string,
                                   const GURL& url,
                                   const GURL& top_origin,
                                   IPC::Message* reply_msg) {
  if (!resource_context_)
    return;

  // Map displayed strings indicating level of keysecurity in the <keygen>
  // menu to the key size in bits. (See SSLKeyGeneratorChromium.cpp in WebCore.)
  int key_size_in_bits;
  switch (key_size_index) {
    case 0:
      key_size_in_bits = 2048;
      break;
    case 1:
      key_size_in_bits = 1024;
      break;
    default:
      DCHECK(false) << "Illegal key_size_index " << key_size_index;
      RenderProcessHostMsg_Keygen::WriteReplyParams(reply_msg, std::string());
      Send(reply_msg);
      return;
  }

  if (!GetContentClient()->browser()->AllowKeygen(top_origin,
                                                  resource_context_)) {
    RenderProcessHostMsg_Keygen::WriteReplyParams(reply_msg, std::string());
    Send(reply_msg);
    return;
  }

  resource_context_->CreateKeygenHandler(
      key_size_in_bits,
      challenge_string,
      url,
      base::Bind(
          &RenderMessageFilter::PostKeygenToWorkerThread, this, reply_msg));
}

void RenderMessageFilter::PostKeygenToWorkerThread(
    IPC::Message* reply_msg,
    std::unique_ptr<net::KeygenHandler> keygen_handler) {
  VLOG(1) << "Dispatching keygen task to worker pool.";
  // Dispatch to worker pool, so we do not block the IO thread.
  if (!base::WorkerPool::PostTask(
           FROM_HERE,
           base::Bind(&RenderMessageFilter::OnKeygenOnWorkerThread,
                      this,
                      base::Passed(&keygen_handler),
                      reply_msg),
           true)) {
    NOTREACHED() << "Failed to dispatch keygen task to worker pool";
    RenderProcessHostMsg_Keygen::WriteReplyParams(reply_msg, std::string());
    Send(reply_msg);
  }
}

void RenderMessageFilter::OnKeygenOnWorkerThread(
    std::unique_ptr<net::KeygenHandler> keygen_handler,
    IPC::Message* reply_msg) {
  DCHECK(reply_msg);

  // Generate a signed public key and challenge, then send it back.
  RenderProcessHostMsg_Keygen::WriteReplyParams(
      reply_msg,
      keygen_handler->GenKeyAndSignChallenge());
  Send(reply_msg);
}

void RenderMessageFilter::OnMediaLogEvents(
    const std::vector<media::MediaLogEvent>& events) {
  // OnMediaLogEvents() is always dispatched to the UI thread for handling.
  // See OverrideThreadForMessage().
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (media_internals_)
    media_internals_->OnMediaEvents(render_process_id_, events);
}

void RenderMessageFilter::OnAllocateGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                                                    uint32_t width,
                                                    uint32_t height,
                                                    gfx::BufferFormat format,
                                                    gfx::BufferUsage usage,
                                                    IPC::Message* reply) {
  DCHECK(BrowserGpuMemoryBufferManager::current());

  base::CheckedNumeric<int> size = width;
  size *= height;
  if (!size.IsValid()) {
    GpuMemoryBufferAllocated(reply, gfx::GpuMemoryBufferHandle());
    return;
  }

  BrowserGpuMemoryBufferManager::current()
      ->AllocateGpuMemoryBufferForChildProcess(
          id, gfx::Size(width, height), format, usage, PeerHandle(),
          render_process_id_,
          base::Bind(&RenderMessageFilter::GpuMemoryBufferAllocated, this,
                     reply));
}

void RenderMessageFilter::GpuMemoryBufferAllocated(
    IPC::Message* reply,
    const gfx::GpuMemoryBufferHandle& handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ChildProcessHostMsg_SyncAllocateGpuMemoryBuffer::WriteReplyParams(reply,
                                                                    handle);
  Send(reply);
}

void RenderMessageFilter::OnEstablishGpuChannel(
    CauseForGpuLaunch cause_for_gpu_launch,
    IPC::Message* reply_ptr) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::unique_ptr<IPC::Message> reply(reply_ptr);

#if defined(OS_WIN) && defined(ARCH_CPU_X86_64)
  // TODO(jbauman): Remove this when we know why renderer processes are
  // hanging on x86-64. https://crbug.com/577127
  if (!GpuDataManagerImpl::GetInstance()->CanUseGpuBrowserCompositor()) {
    reply->set_reply_error();
    Send(reply.release());
    return;
  }
#endif

  GpuProcessHost* host = GpuProcessHost::FromID(gpu_process_id_);
  if (!host) {
    host = GpuProcessHost::Get(GpuProcessHost::GPU_PROCESS_KIND_SANDBOXED,
                               cause_for_gpu_launch);
    if (!host) {
      reply->set_reply_error();
      Send(reply.release());
      return;
    }

    gpu_process_id_ = host->host_id();
  }

  bool preempts = false;
  bool allow_view_command_buffers = false;
  bool allow_real_time_streams = false;
  host->EstablishGpuChannel(
      render_process_id_,
      ChildProcessHostImpl::ChildProcessUniqueIdToTracingProcessId(
          render_process_id_),
      preempts, allow_view_command_buffers, allow_real_time_streams,
      base::Bind(&RenderMessageFilter::EstablishChannelCallback,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(&reply)));
}

void RenderMessageFilter::OnHasGpuProcess(IPC::Message* reply_ptr) {
  std::unique_ptr<IPC::Message> reply(reply_ptr);
  GpuProcessHost::GetProcessHandles(
      base::Bind(&RenderMessageFilter::GetGpuProcessHandlesCallback,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(&reply)));
}

void RenderMessageFilter::EstablishChannelCallback(
    std::unique_ptr<IPC::Message> reply,
    const IPC::ChannelHandle& channel,
    const gpu::GPUInfo& gpu_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ChildProcessHostMsg_EstablishGpuChannel::WriteReplyParams(
      reply.get(), render_process_id_, channel, gpu_info);
  Send(reply.release());
}

void RenderMessageFilter::GetGpuProcessHandlesCallback(
    std::unique_ptr<IPC::Message> reply,
    const std::list<base::ProcessHandle>& handles) {
  bool has_gpu_process = handles.size() > 0;
  ChildProcessHostMsg_HasGpuProcess::WriteReplyParams(reply.get(),
                                                      has_gpu_process);
  Send(reply.release());
}

void RenderMessageFilter::OnDeletedGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gpu::SyncToken& sync_token) {
  DCHECK(BrowserGpuMemoryBufferManager::current());

  BrowserGpuMemoryBufferManager::current()->ChildProcessDeletedGpuMemoryBuffer(
      id, PeerHandle(), render_process_id_, sync_token);
}

}  // namespace content
