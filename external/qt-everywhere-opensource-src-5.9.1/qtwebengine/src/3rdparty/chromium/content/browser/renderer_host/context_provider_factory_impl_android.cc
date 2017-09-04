// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/context_provider_factory_impl_android.h"

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "cc/output/context_provider.h"
#include "cc/output/vulkan_in_process_context_provider.h"
#include "cc/surfaces/surface_manager.h"
#include "content/browser/gpu/browser_gpu_memory_buffer_manager.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/common/host_shared_bitmap_manager.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/ipc/client/gpu_channel_host.h"

namespace content {

namespace {

command_buffer_metrics::ContextType ToCommandBufferContextType(
    ui::ContextProviderFactory::ContextType context_type) {
  switch (context_type) {
    case ui::ContextProviderFactory::ContextType::
        BLIMP_RENDER_COMPOSITOR_CONTEXT:
      return command_buffer_metrics::BLIMP_RENDER_COMPOSITOR_CONTEXT;
    case ui::ContextProviderFactory::ContextType::BLIMP_RENDER_WORKER_CONTEXT:
      return command_buffer_metrics::BLIMP_RENDER_WORKER_CONTEXT;
  }
  NOTREACHED();
  return command_buffer_metrics::CONTEXT_TYPE_UNKNOWN;
}

ContextProviderFactoryImpl* instance = nullptr;

}  // namespace

// static
void ContextProviderFactoryImpl::Initialize(
    gpu::GpuChannelEstablishFactory* gpu_channel_factory) {
  DCHECK(!instance);
  instance = new ContextProviderFactoryImpl(gpu_channel_factory);
}

void ContextProviderFactoryImpl::Terminate() {
  DCHECK(instance);
  delete instance;
  instance = nullptr;
}

// static
ContextProviderFactoryImpl* ContextProviderFactoryImpl::GetInstance() {
  return instance;
}

ContextProviderFactoryImpl::ContextProviderFactoryImpl(
    gpu::GpuChannelEstablishFactory* gpu_channel_factory)
    : gpu_channel_factory_(gpu_channel_factory),
      in_handle_pending_requests_(false),
      in_shutdown_(false),
      next_client_id_(1u),
      weak_factory_(this) {
  DCHECK(gpu_channel_factory_);
}

ContextProviderFactoryImpl::~ContextProviderFactoryImpl() {
  in_shutdown_ = true;
  if (!gpu_channel_requests_.empty())
    HandlePendingRequests(nullptr,
                          GpuChannelHostResult::FAILURE_FACTORY_SHUTDOWN);
}

scoped_refptr<cc::VulkanContextProvider>
ContextProviderFactoryImpl::GetSharedVulkanContextProvider() {
  if (!shared_vulkan_context_provider_)
    shared_vulkan_context_provider_ =
        cc::VulkanInProcessContextProvider::Create();

  return shared_vulkan_context_provider_.get();
}

void ContextProviderFactoryImpl::RequestGpuChannelHost(
    GpuChannelHostCallback callback) {
  DCHECK(!in_shutdown_)
      << "The factory is shutting down, can't handle new requests";

  gpu_channel_requests_.push(callback);
  // If the channel is available, the factory will run the callback
  // synchronously so we'll handle this request there.
  EstablishGpuChannel();
}

scoped_refptr<cc::ContextProvider>
ContextProviderFactoryImpl::CreateDisplayContextProvider(
    gpu::SurfaceHandle surface_handle,
    gpu::SharedMemoryLimits shared_memory_limits,
    gpu::gles2::ContextCreationAttribHelper attributes,
    bool support_locking,
    bool automatic_flushes,
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host) {
  DCHECK(surface_handle != gpu::kNullSurfaceHandle);
  return CreateContextProviderInternal(
      command_buffer_metrics::DISPLAY_COMPOSITOR_ONSCREEN_CONTEXT,
      surface_handle, shared_memory_limits, attributes, support_locking,
      automatic_flushes, nullptr, std::move(gpu_channel_host));
}

scoped_refptr<cc::ContextProvider>
ContextProviderFactoryImpl::CreateOffscreenContextProvider(
    ContextType context_type,
    gpu::SharedMemoryLimits shared_memory_limits,
    gpu::gles2::ContextCreationAttribHelper attributes,
    bool support_locking,
    bool automatic_flushes,
    cc::ContextProvider* shared_context_provider,
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host) {
  return CreateContextProviderInternal(
      ToCommandBufferContextType(context_type), gpu::kNullSurfaceHandle,
      shared_memory_limits, attributes, support_locking, automatic_flushes,
      shared_context_provider, std::move(gpu_channel_host));
}

cc::SurfaceManager* ContextProviderFactoryImpl::GetSurfaceManager() {
  if (!surface_manager_)
    surface_manager_ = base::WrapUnique(new cc::SurfaceManager);

  return surface_manager_.get();
}

cc::FrameSinkId ContextProviderFactoryImpl::AllocateFrameSinkId() {
  return cc::FrameSinkId(++next_client_id_, 0 /* sink_id */);
}

gpu::GpuMemoryBufferManager*
ContextProviderFactoryImpl::GetGpuMemoryBufferManager() {
  return BrowserGpuMemoryBufferManager::current();
}

scoped_refptr<cc::ContextProvider>
ContextProviderFactoryImpl::CreateContextProviderInternal(
    command_buffer_metrics::ContextType context_type,
    gpu::SurfaceHandle surface_handle,
    gpu::SharedMemoryLimits shared_memory_limits,
    gpu::gles2::ContextCreationAttribHelper attributes,
    bool support_locking,
    bool automatic_flushes,
    cc::ContextProvider* shared_context_provider,
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host) {
  return make_scoped_refptr(new ContextProviderCommandBuffer(
      std::move(gpu_channel_host), gpu::GPU_STREAM_DEFAULT,
      gpu::GpuStreamPriority::NORMAL, surface_handle,
      GURL(std::string("chrome://gpu/ContextProviderFactoryImpl::") +
           std::string("CompositorContextProvider")),
      automatic_flushes, support_locking, shared_memory_limits, attributes,
      static_cast<ContextProviderCommandBuffer*>(shared_context_provider),
      context_type));
}

void ContextProviderFactoryImpl::HandlePendingRequests(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host,
    GpuChannelHostResult result) {
  DCHECK(!gpu_channel_requests_.empty())
      << "We don't have any pending requests?";
  DCHECK(gpu_channel_host || result != GpuChannelHostResult::SUCCESS);

  // Failure to initialize the channel could result in new requests. Handle
  // them after going through the current list.
  if (in_handle_pending_requests_)
    return;

  {
    base::AutoReset<bool> auto_reset_in_handle_requests(
        &in_handle_pending_requests_, true);

    std::queue<GpuChannelHostCallback> current_gpu_channel_requests;
    current_gpu_channel_requests.swap(gpu_channel_requests_);

    while (!current_gpu_channel_requests.empty()) {
      current_gpu_channel_requests.front().Run(gpu_channel_host, result);
      current_gpu_channel_requests.pop();
    }
  }

  if (!gpu_channel_requests_.empty())
    EstablishGpuChannel();
}

void ContextProviderFactoryImpl::EstablishGpuChannel() {
#if defined(ADDRESS_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(SYZYASAN) || defined(CYGPROFILE_INSTRUMENTATION)
  const int64_t kGpuChannelTimeoutInSeconds = 40;
#else
  // The GPU watchdog timeout is 15 seconds (1.5x the kGpuTimeout value due to
  // logic in GpuWatchdogThread). Make this slightly longer to give the GPU a
  // chance to crash itself before crashing the browser.
  const int64_t kGpuChannelTimeoutInSeconds = 20;
#endif

  // Start the timer first, if the result comes synchronously, we want it to
  // stop in the callback.
  establish_gpu_channel_timeout_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kGpuChannelTimeoutInSeconds),
      this, &ContextProviderFactoryImpl::OnGpuChannelTimeout);

  gpu_channel_factory_->EstablishGpuChannel(
      base::Bind(&ContextProviderFactoryImpl::OnGpuChannelEstablished,
                 weak_factory_.GetWeakPtr()));
}

void ContextProviderFactoryImpl::OnGpuChannelEstablished(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel) {
  establish_gpu_channel_timeout_.Stop();

  // We can queue the Gpu Channel initialization requests multiple times as
  // we get context requests. So we might have already handled any pending
  // requests when this callback runs.
  if (gpu_channel_requests_.empty())
    return;

  if (gpu_channel) {
    HandlePendingRequests(std::move(gpu_channel),
                          GpuChannelHostResult::SUCCESS);
  } else {
    HandlePendingRequests(
        nullptr,
        GpuChannelHostResult::FAILURE_GPU_PROCESS_INITIALIZATION_FAILED);
  }
}

void ContextProviderFactoryImpl::OnGpuChannelTimeout() {
  LOG(FATAL) << "Timed out waiting for GPU channel.";
}

}  // namespace content
