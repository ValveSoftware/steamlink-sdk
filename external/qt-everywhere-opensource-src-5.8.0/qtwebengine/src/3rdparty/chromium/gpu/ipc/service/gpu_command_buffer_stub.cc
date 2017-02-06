// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_command_buffer_stub.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/hash.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/gl_context_virtual.h"
#include "gpu/command_buffer/service/gl_state_restorer_impl.h"
#include "gpu/command_buffer/service/image_manager.h"
#include "gpu/command_buffer/service/logger.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/query_manager.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/command_buffer/service/transfer_buffer_manager.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "gpu/ipc/service/gpu_memory_manager.h"
#include "gpu/ipc/service/gpu_memory_tracking.h"
#include "gpu/ipc/service/gpu_watchdog.h"
#include "gpu/ipc/service/image_transport_surface.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/init/gl_factory.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#endif

#if defined(OS_ANDROID)
#include "gpu/ipc/service/stream_texture_android.h"
#endif

namespace gpu {
struct WaitForCommandState {
  WaitForCommandState(int32_t start, int32_t end, IPC::Message* reply)
      : start(start), end(end), reply(reply) {}

  int32_t start;
  int32_t end;
  std::unique_ptr<IPC::Message> reply;
};

namespace {

// The GpuCommandBufferMemoryTracker class provides a bridge between the
// ContextGroup's memory type managers and the GpuMemoryManager class.
class GpuCommandBufferMemoryTracker : public gles2::MemoryTracker {
 public:
  explicit GpuCommandBufferMemoryTracker(GpuChannel* channel,
                                         uint64_t share_group_tracing_guid)
      : tracking_group_(
            channel->gpu_channel_manager()
                ->gpu_memory_manager()
                ->CreateTrackingGroup(channel->GetClientPID(), this)),
        client_tracing_id_(channel->client_tracing_id()),
        client_id_(channel->client_id()),
        share_group_tracing_guid_(share_group_tracing_guid) {}

  void TrackMemoryAllocatedChange(
      size_t old_size, size_t new_size) override {
    tracking_group_->TrackMemoryAllocatedChange(
        old_size, new_size);
  }

  bool EnsureGPUMemoryAvailable(size_t size_needed) override {
    return tracking_group_->EnsureGPUMemoryAvailable(size_needed);
  };

  uint64_t ClientTracingId() const override { return client_tracing_id_; }
  int ClientId() const override { return client_id_; }
  uint64_t ShareGroupTracingGUID() const override {
    return share_group_tracing_guid_;
  }

 private:
  ~GpuCommandBufferMemoryTracker() override {}
  std::unique_ptr<GpuMemoryTrackingGroup> tracking_group_;
  const uint64_t client_tracing_id_;
  const int client_id_;
  const uint64_t share_group_tracing_guid_;

  DISALLOW_COPY_AND_ASSIGN(GpuCommandBufferMemoryTracker);
};

// FastSetActiveURL will shortcut the expensive call to SetActiveURL when the
// url_hash matches.
void FastSetActiveURL(const GURL& url, size_t url_hash, GpuChannel* channel) {
  // Leave the previously set URL in the empty case -- empty URLs are given by
  // BlinkPlatformImpl::createOffscreenGraphicsContext3DProvider. Hopefully the
  // onscreen context URL was set previously and will show up even when a crash
  // occurs during offscreen command processing.
  if (url.is_empty())
    return;
  static size_t g_last_url_hash = 0;
  if (url_hash != g_last_url_hash) {
    g_last_url_hash = url_hash;
    DCHECK(channel && channel->gpu_channel_manager() &&
           channel->gpu_channel_manager()->delegate());
    channel->gpu_channel_manager()->delegate()->SetActiveURL(url);
  }
}

// The first time polling a fence, delay some extra time to allow other
// stubs to process some work, or else the timing of the fences could
// allow a pattern of alternating fast and slow frames to occur.
const int64_t kHandleMoreWorkPeriodMs = 2;
const int64_t kHandleMoreWorkPeriodBusyMs = 1;

// Prevents idle work from being starved.
const int64_t kMaxTimeSinceIdleMs = 10;

class DevToolsChannelData : public base::trace_event::ConvertableToTraceFormat {
 public:
  static std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
  CreateForChannel(GpuChannel* channel);
  ~DevToolsChannelData() override {}

  void AppendAsTraceFormat(std::string* out) const override {
    std::string tmp;
    base::JSONWriter::Write(*value_, &tmp);
    *out += tmp;
  }

 private:
  explicit DevToolsChannelData(base::Value* value) : value_(value) {}
  std::unique_ptr<base::Value> value_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsChannelData);
};

std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
DevToolsChannelData::CreateForChannel(GpuChannel* channel) {
  std::unique_ptr<base::DictionaryValue> res(new base::DictionaryValue);
  res->SetInteger("renderer_pid", channel->GetClientPID());
  res->SetDouble("used_bytes", channel->GetMemoryUsage());
  return base::WrapUnique(new DevToolsChannelData(res.release()));
}

CommandBufferId GetCommandBufferID(int channel_id, int32_t route_id) {
  return CommandBufferId::FromUnsafeValue(
      (static_cast<uint64_t>(channel_id) << 32) | route_id);
}

}  // namespace

std::unique_ptr<GpuCommandBufferStub> GpuCommandBufferStub::Create(
    GpuChannel* channel,
    GpuCommandBufferStub* share_command_buffer_stub,
    const GPUCreateCommandBufferConfig& init_params,
    int32_t route_id,
    std::unique_ptr<base::SharedMemory> shared_state_shm) {
  std::unique_ptr<GpuCommandBufferStub> stub(
      new GpuCommandBufferStub(channel, init_params, route_id));
  if (!stub->Initialize(share_command_buffer_stub, init_params,
                        std::move(shared_state_shm)))
    return nullptr;
  return stub;
}

GpuCommandBufferStub::GpuCommandBufferStub(
    GpuChannel* channel,
    const GPUCreateCommandBufferConfig& init_params,
    int32_t route_id)
    : channel_(channel),
      initialized_(false),
      surface_handle_(init_params.surface_handle),
      use_virtualized_gl_context_(false),
      command_buffer_id_(GetCommandBufferID(channel->client_id(), route_id)),
      stream_id_(init_params.stream_id),
      route_id_(route_id),
      last_flush_count_(0),
      waiting_for_sync_point_(false),
      previous_processed_num_(0),
      active_url_(init_params.active_url),
      active_url_hash_(base::Hash(active_url_.possibly_invalid_spec())) {}

GpuCommandBufferStub::~GpuCommandBufferStub() {
  Destroy();
}

GpuMemoryManager* GpuCommandBufferStub::GetMemoryManager() const {
    return channel()->gpu_channel_manager()->gpu_memory_manager();
}

bool GpuCommandBufferStub::OnMessageReceived(const IPC::Message& message) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
               "GPUTask",
               "data",
               DevToolsChannelData::CreateForChannel(channel()));
  FastSetActiveURL(active_url_, active_url_hash_, channel_);

  bool have_context = false;
  // Ensure the appropriate GL context is current before handling any IPC
  // messages directed at the command buffer. This ensures that the message
  // handler can assume that the context is current (not necessary for
  // RetireSyncPoint or WaitSyncPoint).
  if (decoder_.get() &&
      message.type() != GpuCommandBufferMsg_SetGetBuffer::ID &&
      message.type() != GpuCommandBufferMsg_WaitForTokenInRange::ID &&
      message.type() != GpuCommandBufferMsg_WaitForGetOffsetInRange::ID &&
      message.type() != GpuCommandBufferMsg_RegisterTransferBuffer::ID &&
      message.type() != GpuCommandBufferMsg_DestroyTransferBuffer::ID) {
    if (!MakeCurrent())
      return false;
    have_context = true;
  }

  // Always use IPC_MESSAGE_HANDLER_DELAY_REPLY for synchronous message handlers
  // here. This is so the reply can be delayed if the scheduler is unscheduled.
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuCommandBufferStub, message)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_SetGetBuffer,
                                    OnSetGetBuffer);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_TakeFrontBuffer, OnTakeFrontBuffer);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_ReturnFrontBuffer,
                        OnReturnFrontBuffer);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_WaitForTokenInRange,
                                    OnWaitForTokenInRange);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_WaitForGetOffsetInRange,
                                    OnWaitForGetOffsetInRange);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_AsyncFlush, OnAsyncFlush);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_RegisterTransferBuffer,
                        OnRegisterTransferBuffer);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_DestroyTransferBuffer,
                        OnDestroyTransferBuffer);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_WaitSyncToken,
                        OnWaitSyncToken)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_SignalSyncToken,
                        OnSignalSyncToken)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_SignalQuery,
                        OnSignalQuery)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_CreateImage, OnCreateImage);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_DestroyImage, OnDestroyImage);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_CreateStreamTexture,
                        OnCreateStreamTexture)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  CheckCompleteWaits();

  // Ensure that any delayed work that was created will be handled.
  if (have_context) {
    if (executor_)
      executor_->ProcessPendingQueries();
    ScheduleDelayedWork(
        base::TimeDelta::FromMilliseconds(kHandleMoreWorkPeriodMs));
  }

  return handled;
}

bool GpuCommandBufferStub::Send(IPC::Message* message) {
  return channel_->Send(message);
}

bool GpuCommandBufferStub::IsScheduled() {
  return (!executor_.get() || executor_->scheduled());
}

void GpuCommandBufferStub::PollWork() {
  // Post another delayed task if we have not yet reached the time at which
  // we should process delayed work.
  base::TimeTicks current_time = base::TimeTicks::Now();
  DCHECK(!process_delayed_work_time_.is_null());
  if (process_delayed_work_time_ > current_time) {
    channel_->task_runner()->PostDelayedTask(
        FROM_HERE, base::Bind(&GpuCommandBufferStub::PollWork, AsWeakPtr()),
        process_delayed_work_time_ - current_time);
    return;
  }
  process_delayed_work_time_ = base::TimeTicks();

  PerformWork();
}

void GpuCommandBufferStub::PerformWork() {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::PerformWork");

  FastSetActiveURL(active_url_, active_url_hash_, channel_);
  if (decoder_.get() && !MakeCurrent())
    return;

  if (executor_) {
    uint32_t current_unprocessed_num =
        channel()->gpu_channel_manager()->GetUnprocessedOrderNum();
    // We're idle when no messages were processed or scheduled.
    bool is_idle = (previous_processed_num_ == current_unprocessed_num);
    if (!is_idle && !last_idle_time_.is_null()) {
      base::TimeDelta time_since_idle =
          base::TimeTicks::Now() - last_idle_time_;
      base::TimeDelta max_time_since_idle =
          base::TimeDelta::FromMilliseconds(kMaxTimeSinceIdleMs);

      // Force idle when it's been too long since last time we were idle.
      if (time_since_idle > max_time_since_idle)
        is_idle = true;
    }

    if (is_idle) {
      last_idle_time_ = base::TimeTicks::Now();
      executor_->PerformIdleWork();
    }

    executor_->ProcessPendingQueries();
    executor_->PerformPollingWork();
  }

  ScheduleDelayedWork(
      base::TimeDelta::FromMilliseconds(kHandleMoreWorkPeriodBusyMs));
}

bool GpuCommandBufferStub::HasUnprocessedCommands() {
  if (command_buffer_) {
    CommandBuffer::State state = command_buffer_->GetLastState();
    return command_buffer_->GetPutOffset() != state.get_offset &&
        !error::IsError(state.error);
  }
  return false;
}

void GpuCommandBufferStub::ScheduleDelayedWork(base::TimeDelta delay) {
  bool has_more_work = executor_.get() && (executor_->HasPendingQueries() ||
                                           executor_->HasMoreIdleWork() ||
                                           executor_->HasPollingWork());
  if (!has_more_work) {
    last_idle_time_ = base::TimeTicks();
    return;
  }

  base::TimeTicks current_time = base::TimeTicks::Now();
  // |process_delayed_work_time_| is set if processing of delayed work is
  // already scheduled. Just update the time if already scheduled.
  if (!process_delayed_work_time_.is_null()) {
    process_delayed_work_time_ = current_time + delay;
    return;
  }

  // Idle when no messages are processed between now and when
  // PollWork is called.
  previous_processed_num_ =
      channel()->gpu_channel_manager()->GetProcessedOrderNum();
  if (last_idle_time_.is_null())
    last_idle_time_ = current_time;

  // IsScheduled() returns true after passing all unschedule fences
  // and this is when we can start performing idle work. Idle work
  // is done synchronously so we can set delay to 0 and instead poll
  // for more work at the rate idle work is performed. This also ensures
  // that idle work is done as efficiently as possible without any
  // unnecessary delays.
  if (executor_.get() && executor_->scheduled() &&
      executor_->HasMoreIdleWork()) {
    delay = base::TimeDelta();
  }

  process_delayed_work_time_ = current_time + delay;
  channel_->task_runner()->PostDelayedTask(
      FROM_HERE, base::Bind(&GpuCommandBufferStub::PollWork, AsWeakPtr()),
      delay);
}

bool GpuCommandBufferStub::MakeCurrent() {
  if (decoder_->MakeCurrent())
    return true;
  DLOG(ERROR) << "Context lost because MakeCurrent failed.";
  command_buffer_->SetContextLostReason(decoder_->GetContextLostReason());
  command_buffer_->SetParseError(error::kLostContext);
  CheckContextLost();
  return false;
}

void GpuCommandBufferStub::Destroy() {
  if (wait_for_token_) {
    Send(wait_for_token_->reply.release());
    wait_for_token_.reset();
  }
  if (wait_for_get_offset_) {
    Send(wait_for_get_offset_->reply.release());
    wait_for_get_offset_.reset();
  }

  if (initialized_) {
    GpuChannelManager* gpu_channel_manager = channel_->gpu_channel_manager();
    // If we are currently shutting down the GPU process to help with recovery
    // (exit_on_context_lost workaround), then don't tell the browser about
    // offscreen context destruction here since it's not client-invoked, and
    // might bypass the 3D API blocking logic.
    if ((surface_handle_ == gpu::kNullSurfaceHandle) && !active_url_.is_empty()
        && !gpu_channel_manager->is_exiting_for_lost_context()) {
      gpu_channel_manager->delegate()->DidDestroyOffscreenContext(active_url_);
    }
  }

  if (decoder_)
    decoder_->set_engine(NULL);

  // The scheduler has raw references to the decoder and the command buffer so
  // destroy it before those.
  executor_.reset();

  sync_point_client_.reset();

  bool have_context = false;
  if (decoder_ && decoder_->GetGLContext()) {
    // Try to make the context current regardless of whether it was lost, so we
    // don't leak resources.
    have_context = decoder_->GetGLContext()->MakeCurrent(surface_.get());
  }
  FOR_EACH_OBSERVER(DestructionObserver,
                    destruction_observers_,
                    OnWillDestroyStub());

  if (decoder_) {
    decoder_->Destroy(have_context);
    decoder_.reset();
  }

  command_buffer_.reset();

  // Remove this after crbug.com/248395 is sorted out.
  surface_ = NULL;
}

bool GpuCommandBufferStub::Initialize(
    GpuCommandBufferStub* share_command_buffer_stub,
    const GPUCreateCommandBufferConfig& init_params,
    std::unique_ptr<base::SharedMemory> shared_state_shm) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::Initialize");
  FastSetActiveURL(active_url_, active_url_hash_, channel_);

  GpuChannelManager* manager = channel_->gpu_channel_manager();
  DCHECK(manager);

  if (share_command_buffer_stub) {
    context_group_ = share_command_buffer_stub->context_group_;
    DCHECK(context_group_->bind_generates_resource() ==
           init_params.attribs.bind_generates_resource);
  } else {
    scoped_refptr<gles2::FeatureInfo> feature_info =
        new gles2::FeatureInfo(manager->gpu_driver_bug_workarounds());
    gpu::GpuMemoryBufferFactory* gmb_factory =
        channel_->gpu_channel_manager()->gpu_memory_buffer_factory();
    context_group_ = new gles2::ContextGroup(
        manager->gpu_preferences(), channel_->mailbox_manager(),
        new GpuCommandBufferMemoryTracker(channel_,
                                          command_buffer_id_.GetUnsafeValue()),
        manager->shader_translator_cache(),
        manager->framebuffer_completeness_cache(), feature_info,
        init_params.attribs.bind_generates_resource,
        gmb_factory ? gmb_factory->AsImageFactory() : nullptr);
  }

#if defined(OS_MACOSX)
  // Virtualize PreferIntegratedGpu contexts by default on OS X to prevent
  // performance regressions when enabling FCM.
  // http://crbug.com/180463
  if (init_params.attribs.gpu_preference == gl::PreferIntegratedGpu)
    use_virtualized_gl_context_ = true;
#endif

  use_virtualized_gl_context_ |=
      context_group_->feature_info()->workarounds().use_virtualized_gl_contexts;

  // MailboxManagerSync synchronization correctness currently depends on having
  // only a single context. See crbug.com/510243 for details.
  use_virtualized_gl_context_ |= channel_->mailbox_manager()->UsesSync();

  gl::GLSurface::Format surface_format = gl::GLSurface::SURFACE_DEFAULT;
  bool offscreen = (surface_handle_ == kNullSurfaceHandle);
  gl::GLSurface* default_surface = manager->GetDefaultOffscreenSurface();
  if (!default_surface) {
    DLOG(ERROR) << "Failed to create default offscreen surface.";
    return false;
  }
#if defined(OS_ANDROID)
  if (init_params.attribs.red_size <= 5 &&
      init_params.attribs.green_size <= 6 &&
      init_params.attribs.blue_size <= 5 &&
      init_params.attribs.alpha_size == 0)
    surface_format = gl::GLSurface::SURFACE_RGB565;
  // We can only use virtualized contexts for onscreen command buffers if their
  // config is compatible with the offscreen ones - otherwise MakeCurrent fails.
  if (surface_format != default_surface->GetFormat() && !offscreen)
    use_virtualized_gl_context_ = false;
#endif

  command_buffer_.reset(new CommandBufferService(
      context_group_->transfer_buffer_manager()));

  decoder_.reset(gles2::GLES2Decoder::Create(context_group_.get()));
  executor_.reset(new CommandExecutor(command_buffer_.get(), decoder_.get(),
                                      decoder_.get()));
  sync_point_client_ = channel_->sync_point_manager()->CreateSyncPointClient(
      channel_->GetSyncPointOrderData(stream_id_),
      CommandBufferNamespace::GPU_IO, command_buffer_id_);

  executor_->SetPreemptByFlag(channel_->preempted_flag());

  decoder_->set_engine(executor_.get());

  if (offscreen) {
    surface_ = default_surface;
  } else {
    surface_ = ImageTransportSurface::CreateNativeSurface(
        manager, this, surface_handle_, surface_format);
    if (!surface_ || !surface_->Initialize(surface_format)) {
      surface_ = nullptr;
      DLOG(ERROR) << "Failed to create surface.";
      return false;
    }
  }

  scoped_refptr<gl::GLContext> context;
  gl::GLShareGroup* gl_share_group = channel_->share_group();
  if (use_virtualized_gl_context_ && gl_share_group) {
    context = gl_share_group->GetSharedContext();
    if (!context.get()) {
      context = gl::init::CreateGLContext(gl_share_group, default_surface,
                                          init_params.attribs.gpu_preference);
      if (!context.get()) {
        DLOG(ERROR) << "Failed to create shared context for virtualization.";
        return false;
      }
      // Ensure that context creation did not lose track of the intended
      // gl_share_group.
      DCHECK(context->share_group() == gl_share_group);
      gl_share_group->SetSharedContext(context.get());
    }
    // This should be either:
    // (1) a non-virtual GL context, or
    // (2) a mock context.
    DCHECK(context->GetHandle() ||
           gl::GetGLImplementation() == gl::kGLImplementationMockGL);
    context = new GLContextVirtual(
        gl_share_group, context.get(), decoder_->AsWeakPtr());
    if (!context->Initialize(surface_.get(),
                             init_params.attribs.gpu_preference)) {
      // The real context created above for the default offscreen surface
      // might not be compatible with this surface.
      context = NULL;
      DLOG(ERROR) << "Failed to initialize virtual GL context.";
      return false;
    }
  }
  if (!context.get()) {
    context = gl::init::CreateGLContext(gl_share_group, surface_.get(),
                                        init_params.attribs.gpu_preference);
  }
  if (!context.get()) {
    DLOG(ERROR) << "Failed to create context.";
    return false;
  }

  if (!context->MakeCurrent(surface_.get())) {
    LOG(ERROR) << "Failed to make context current.";
    return false;
  }

  if (!context->GetGLStateRestorer()) {
    context->SetGLStateRestorer(
        new GLStateRestorerImpl(decoder_->AsWeakPtr()));
  }

  if (!context_group_->has_program_cache() &&
      !context_group_->feature_info()->workarounds().disable_program_cache) {
    context_group_->set_program_cache(manager->program_cache());
  }

  // Initialize the decoder with either the view or pbuffer GLContext.
  if (!decoder_->Initialize(surface_, context, offscreen,
                            gpu::gles2::DisallowedFeatures(),
                            init_params.attribs)) {
    DLOG(ERROR) << "Failed to initialize decoder.";
    return false;
  }

  if (manager->gpu_preferences().enable_gpu_service_logging) {
    decoder_->set_log_commands(true);
  }

  decoder_->GetLogger()->SetMsgCallback(
      base::Bind(&GpuCommandBufferStub::SendConsoleMessage,
                 base::Unretained(this)));
  decoder_->SetShaderCacheCallback(
      base::Bind(&GpuCommandBufferStub::SendCachedShader,
                 base::Unretained(this)));
  decoder_->SetFenceSyncReleaseCallback(base::Bind(
      &GpuCommandBufferStub::OnFenceSyncRelease, base::Unretained(this)));
  decoder_->SetWaitFenceSyncCallback(base::Bind(
      &GpuCommandBufferStub::OnWaitFenceSync, base::Unretained(this)));
  decoder_->SetDescheduleUntilFinishedCallback(
      base::Bind(&GpuCommandBufferStub::OnDescheduleUntilFinished,
                 base::Unretained(this)));
  decoder_->SetRescheduleAfterFinishedCallback(
      base::Bind(&GpuCommandBufferStub::OnRescheduleAfterFinished,
                 base::Unretained(this)));

  command_buffer_->SetPutOffsetChangeCallback(
      base::Bind(&GpuCommandBufferStub::PutChanged, base::Unretained(this)));
  command_buffer_->SetGetBufferChangeCallback(base::Bind(
      &CommandExecutor::SetGetBuffer, base::Unretained(executor_.get())));
  command_buffer_->SetParseErrorCallback(
      base::Bind(&GpuCommandBufferStub::OnParseError, base::Unretained(this)));

  if (channel_->watchdog()) {
    executor_->SetCommandProcessedCallback(base::Bind(
        &GpuCommandBufferStub::OnCommandProcessed, base::Unretained(this)));
  }

  const size_t kSharedStateSize = sizeof(CommandBufferSharedState);
  if (!shared_state_shm->Map(kSharedStateSize)) {
    DLOG(ERROR) << "Failed to map shared state buffer.";
    return false;
  }
  command_buffer_->SetSharedStateBuffer(MakeBackingFromSharedMemory(
      std::move(shared_state_shm), kSharedStateSize));

  if (offscreen && !active_url_.is_empty())
    manager->delegate()->DidCreateOffscreenContext(active_url_);

  initialized_ = true;
  return true;
}

void GpuCommandBufferStub::OnCreateStreamTexture(uint32_t texture_id,
                                                 int32_t stream_id,
                                                 bool* succeeded) {
#if defined(OS_ANDROID)
  *succeeded = StreamTexture::Create(this, texture_id, stream_id);
#else
  *succeeded = false;
#endif
}

void GpuCommandBufferStub::SetLatencyInfoCallback(
    const LatencyInfoCallback& callback) {
  latency_info_callback_ = callback;
}

void GpuCommandBufferStub::OnSetGetBuffer(int32_t shm_id,
                                          IPC::Message* reply_message) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnSetGetBuffer");
  if (command_buffer_)
    command_buffer_->SetGetBuffer(shm_id);
  Send(reply_message);
}

void GpuCommandBufferStub::OnTakeFrontBuffer(const Mailbox& mailbox) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnTakeFrontBuffer");
  if (!decoder_) {
    LOG(ERROR) << "Can't take front buffer before initialization.";
    return;
  }

  decoder_->TakeFrontBuffer(mailbox);
}

void GpuCommandBufferStub::OnReturnFrontBuffer(const Mailbox& mailbox,
                                               bool is_lost) {
  decoder_->ReturnFrontBuffer(mailbox, is_lost);
}

void GpuCommandBufferStub::OnParseError() {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnParseError");
  DCHECK(command_buffer_.get());
  CommandBuffer::State state = command_buffer_->GetLastState();
  IPC::Message* msg = new GpuCommandBufferMsg_Destroyed(
      route_id_, state.context_lost_reason, state.error);
  msg->set_unblock(true);
  Send(msg);

  // Tell the browser about this context loss as well, so it can
  // determine whether client APIs like WebGL need to be immediately
  // blocked from automatically running.
  GpuChannelManager* gpu_channel_manager = channel_->gpu_channel_manager();
  gpu_channel_manager->delegate()->DidLoseContext(
      (surface_handle_ == kNullSurfaceHandle), state.context_lost_reason,
      active_url_);

  CheckContextLost();
}

void GpuCommandBufferStub::OnWaitForTokenInRange(int32_t start,
                                                 int32_t end,
                                                 IPC::Message* reply_message) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnWaitForTokenInRange");
  DCHECK(command_buffer_.get());
  CheckContextLost();
  if (wait_for_token_)
    LOG(ERROR) << "Got WaitForToken command while currently waiting for token.";
  wait_for_token_ =
      base::WrapUnique(new WaitForCommandState(start, end, reply_message));
  CheckCompleteWaits();
}

void GpuCommandBufferStub::OnWaitForGetOffsetInRange(
    int32_t start,
    int32_t end,
    IPC::Message* reply_message) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnWaitForGetOffsetInRange");
  DCHECK(command_buffer_.get());
  CheckContextLost();
  if (wait_for_get_offset_) {
    LOG(ERROR)
        << "Got WaitForGetOffset command while currently waiting for offset.";
  }
  wait_for_get_offset_ =
      base::WrapUnique(new WaitForCommandState(start, end, reply_message));
  CheckCompleteWaits();
}

void GpuCommandBufferStub::CheckCompleteWaits() {
  if (wait_for_token_ || wait_for_get_offset_) {
    CommandBuffer::State state = command_buffer_->GetLastState();
    if (wait_for_token_ &&
        (CommandBuffer::InRange(
             wait_for_token_->start, wait_for_token_->end, state.token) ||
         state.error != error::kNoError)) {
      ReportState();
      GpuCommandBufferMsg_WaitForTokenInRange::WriteReplyParams(
          wait_for_token_->reply.get(), state);
      Send(wait_for_token_->reply.release());
      wait_for_token_.reset();
    }
    if (wait_for_get_offset_ &&
        (CommandBuffer::InRange(wait_for_get_offset_->start,
                                     wait_for_get_offset_->end,
                                     state.get_offset) ||
         state.error != error::kNoError)) {
      ReportState();
      GpuCommandBufferMsg_WaitForGetOffsetInRange::WriteReplyParams(
          wait_for_get_offset_->reply.get(), state);
      Send(wait_for_get_offset_->reply.release());
      wait_for_get_offset_.reset();
    }
  }
}

void GpuCommandBufferStub::OnAsyncFlush(
    int32_t put_offset,
    uint32_t flush_count,
    const std::vector<ui::LatencyInfo>& latency_info) {
  TRACE_EVENT1(
      "gpu", "GpuCommandBufferStub::OnAsyncFlush", "put_offset", put_offset);
  DCHECK(command_buffer_);

  // We received this message out-of-order. This should not happen but is here
  // to catch regressions. Ignore the message.
  DVLOG_IF(0, flush_count - last_flush_count_ >= 0x8000000U)
      << "Received a Flush message out-of-order";

  if (flush_count > last_flush_count_ &&
      ui::LatencyInfo::Verify(latency_info,
                              "GpuCommandBufferStub::OnAsyncFlush") &&
      !latency_info_callback_.is_null()) {
    latency_info_callback_.Run(latency_info);
  }

  last_flush_count_ = flush_count;
  CommandBuffer::State pre_state = command_buffer_->GetLastState();
  command_buffer_->Flush(put_offset);
  CommandBuffer::State post_state = command_buffer_->GetLastState();

  if (pre_state.get_offset != post_state.get_offset)
    ReportState();

#if defined(OS_ANDROID)
  GpuChannelManager* manager = channel_->gpu_channel_manager();
  manager->DidAccessGpu();
#endif
}

void GpuCommandBufferStub::OnRegisterTransferBuffer(
    int32_t id,
    base::SharedMemoryHandle transfer_buffer,
    uint32_t size) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnRegisterTransferBuffer");

  // Take ownership of the memory and map it into this process.
  // This validates the size.
  std::unique_ptr<base::SharedMemory> shared_memory(
      new base::SharedMemory(transfer_buffer, false));
  if (!shared_memory->Map(size)) {
    DVLOG(0) << "Failed to map shared memory.";
    return;
  }

  if (command_buffer_) {
    command_buffer_->RegisterTransferBuffer(
        id, MakeBackingFromSharedMemory(std::move(shared_memory), size));
  }
}

void GpuCommandBufferStub::OnDestroyTransferBuffer(int32_t id) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnDestroyTransferBuffer");

  if (command_buffer_)
    command_buffer_->DestroyTransferBuffer(id);
}

void GpuCommandBufferStub::OnCommandProcessed() {
  DCHECK(channel_->watchdog());
  channel_->watchdog()->CheckArmed();
}

void GpuCommandBufferStub::ReportState() { command_buffer_->UpdateState(); }

void GpuCommandBufferStub::PutChanged() {
  FastSetActiveURL(active_url_, active_url_hash_, channel_);
  executor_->PutChanged();
}

void GpuCommandBufferStub::PullTextureUpdates(
    CommandBufferNamespace namespace_id,
    CommandBufferId command_buffer_id,
    uint32_t release) {
  gles2::MailboxManager* mailbox_manager =
      context_group_->mailbox_manager();
  if (mailbox_manager->UsesSync() && MakeCurrent()) {
    SyncToken sync_token(namespace_id, 0, command_buffer_id, release);
    mailbox_manager->PullTextureUpdates(sync_token);
  }
}

void GpuCommandBufferStub::OnWaitSyncToken(const SyncToken& sync_token) {
  OnWaitFenceSync(sync_token.namespace_id(), sync_token.command_buffer_id(),
                  sync_token.release_count());
}

void GpuCommandBufferStub::OnSignalSyncToken(const SyncToken& sync_token,
                                             uint32_t id) {
  scoped_refptr<SyncPointClientState> release_state =
      channel_->sync_point_manager()->GetSyncPointClientState(
          sync_token.namespace_id(), sync_token.command_buffer_id());

  if (release_state) {
    sync_point_client_->Wait(release_state.get(), sync_token.release_count(),
                             base::Bind(&GpuCommandBufferStub::OnSignalAck,
                                        this->AsWeakPtr(), id));
  } else {
    OnSignalAck(id);
  }
}

void GpuCommandBufferStub::OnSignalAck(uint32_t id) {
  Send(new GpuCommandBufferMsg_SignalAck(route_id_, id));
}

void GpuCommandBufferStub::OnSignalQuery(uint32_t query_id, uint32_t id) {
  if (decoder_) {
    gles2::QueryManager* query_manager = decoder_->GetQueryManager();
    if (query_manager) {
      gles2::QueryManager::Query* query =
          query_manager->GetQuery(query_id);
      if (query) {
        query->AddCallback(
          base::Bind(&GpuCommandBufferStub::OnSignalAck,
                     this->AsWeakPtr(),
                     id));
        return;
      }
    }
  }
  // Something went wrong, run callback immediately.
  OnSignalAck(id);
}

void GpuCommandBufferStub::OnFenceSyncRelease(uint64_t release) {
  if (sync_point_client_->client_state()->IsFenceSyncReleased(release)) {
    DLOG(ERROR) << "Fence Sync has already been released.";
    return;
  }

  gles2::MailboxManager* mailbox_manager =
      context_group_->mailbox_manager();
  if (mailbox_manager->UsesSync() && MakeCurrent()) {
    SyncToken sync_token(CommandBufferNamespace::GPU_IO, 0,
                              command_buffer_id_, release);
    mailbox_manager->PushTextureUpdates(sync_token);
  }

  sync_point_client_->ReleaseFenceSync(release);
}

void GpuCommandBufferStub::OnDescheduleUntilFinished() {
  DCHECK(executor_->scheduled());
  DCHECK(executor_->HasPollingWork());

  executor_->SetScheduled(false);
  channel_->OnStreamRescheduled(stream_id_, false);
}

void GpuCommandBufferStub::OnRescheduleAfterFinished() {
  DCHECK(!executor_->scheduled());

  executor_->SetScheduled(true);
  channel_->OnStreamRescheduled(stream_id_, true);
}

bool GpuCommandBufferStub::OnWaitFenceSync(
    CommandBufferNamespace namespace_id,
    CommandBufferId command_buffer_id,
    uint64_t release) {
  DCHECK(!waiting_for_sync_point_);
  DCHECK(executor_->scheduled());

  scoped_refptr<SyncPointClientState> release_state =
      channel_->sync_point_manager()->GetSyncPointClientState(
          namespace_id, command_buffer_id);

  if (!release_state)
    return true;

  if (release_state->IsFenceSyncReleased(release)) {
    PullTextureUpdates(namespace_id, command_buffer_id, release);
    return true;
  }

  TRACE_EVENT_ASYNC_BEGIN1("gpu", "WaitFenceSync", this, "GpuCommandBufferStub",
                           this);
  waiting_for_sync_point_ = true;
  sync_point_client_->WaitNonThreadSafe(
      release_state.get(), release, channel_->task_runner(),
      base::Bind(&GpuCommandBufferStub::OnWaitFenceSyncCompleted,
                 this->AsWeakPtr(), namespace_id, command_buffer_id, release));

  if (!waiting_for_sync_point_)
    return true;

  executor_->SetScheduled(false);
  channel_->OnStreamRescheduled(stream_id_, false);
  return false;
}

void GpuCommandBufferStub::OnWaitFenceSyncCompleted(
    CommandBufferNamespace namespace_id,
    CommandBufferId command_buffer_id,
    uint64_t release) {
  DCHECK(waiting_for_sync_point_);
  TRACE_EVENT_ASYNC_END1("gpu", "WaitFenceSync", this, "GpuCommandBufferStub",
                         this);
  PullTextureUpdates(namespace_id, command_buffer_id, release);
  waiting_for_sync_point_ = false;
  executor_->SetScheduled(true);
  channel_->OnStreamRescheduled(stream_id_, true);
}

void GpuCommandBufferStub::OnCreateImage(
    const GpuCommandBufferMsg_CreateImage_Params& params) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnCreateImage");
  const int32_t id = params.id;
  const gfx::GpuMemoryBufferHandle& handle = params.gpu_memory_buffer;
  const gfx::Size& size = params.size;
  const gfx::BufferFormat& format = params.format;
  const uint32_t internalformat = params.internal_format;
  const uint64_t image_release_count = params.image_release_count;

  if (!decoder_)
    return;

  gles2::ImageManager* image_manager = decoder_->GetImageManager();
  DCHECK(image_manager);
  if (image_manager->LookupImage(id)) {
    LOG(ERROR) << "Image already exists with same ID.";
    return;
  }

  if (!gpu::IsGpuMemoryBufferFormatSupported(format,
                                             decoder_->GetCapabilities())) {
    LOG(ERROR) << "Format is not supported.";
    return;
  }

  if (!gpu::IsImageSizeValidForGpuMemoryBufferFormat(size, format)) {
    LOG(ERROR) << "Invalid image size for format.";
    return;
  }

  if (!gpu::IsImageFormatCompatibleWithGpuMemoryBufferFormat(internalformat,
                                                             format)) {
    LOG(ERROR) << "Incompatible image format.";
    return;
  }

  scoped_refptr<gl::GLImage> image = channel()->CreateImageForGpuMemoryBuffer(
      handle, size, format, internalformat, surface_handle_);
  if (!image.get())
    return;

  image_manager->AddImage(image.get(), id);
  if (image_release_count) {
    DLOG_IF(ERROR,
            image_release_count !=
                sync_point_client_->client_state()->fence_sync_release() + 1)
        << "Client released fences out of order.";
    sync_point_client_->ReleaseFenceSync(image_release_count);
  }
}

void GpuCommandBufferStub::OnDestroyImage(int32_t id) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnDestroyImage");

  if (!decoder_)
    return;

  gles2::ImageManager* image_manager = decoder_->GetImageManager();
  DCHECK(image_manager);
  if (!image_manager->LookupImage(id)) {
    LOG(ERROR) << "Image with ID doesn't exist.";
    return;
  }

  image_manager->RemoveImage(id);
}

void GpuCommandBufferStub::SendConsoleMessage(int32_t id,
                                              const std::string& message) {
  GPUCommandBufferConsoleMessage console_message;
  console_message.id = id;
  console_message.message = message;
  IPC::Message* msg = new GpuCommandBufferMsg_ConsoleMsg(
      route_id_, console_message);
  msg->set_unblock(true);
  Send(msg);
}

void GpuCommandBufferStub::SendCachedShader(
    const std::string& key, const std::string& shader) {
  channel_->CacheShader(key, shader);
}

void GpuCommandBufferStub::AddDestructionObserver(
    DestructionObserver* observer) {
  destruction_observers_.AddObserver(observer);
}

void GpuCommandBufferStub::RemoveDestructionObserver(
    DestructionObserver* observer) {
  destruction_observers_.RemoveObserver(observer);
}

const gles2::FeatureInfo* GpuCommandBufferStub::GetFeatureInfo() const {
  return context_group_->feature_info();
}

gles2::MemoryTracker* GpuCommandBufferStub::GetMemoryTracker() const {
  return context_group_->memory_tracker();
}

bool GpuCommandBufferStub::CheckContextLost() {
  DCHECK(command_buffer_);
  CommandBuffer::State state = command_buffer_->GetLastState();
  bool was_lost = state.error == error::kLostContext;

  if (was_lost) {
    bool was_lost_by_robustness =
        decoder_ && decoder_->WasContextLostByRobustnessExtension();

    // Work around issues with recovery by allowing a new GPU process to launch.
    if ((was_lost_by_robustness ||
         context_group_->feature_info()->workarounds().exit_on_context_lost)) {
      channel_->gpu_channel_manager()->MaybeExitOnContextLost();
    }

    // Lose all other contexts if the reset was triggered by the robustness
    // extension instead of being synthetic.
    if (was_lost_by_robustness &&
        (gl::GLContext::LosesAllContextsOnContextLost() ||
         use_virtualized_gl_context_)) {
      channel_->LoseAllContexts();
    }
  }

  CheckCompleteWaits();
  return was_lost;
}

void GpuCommandBufferStub::MarkContextLost() {
  if (!command_buffer_ ||
      command_buffer_->GetLastState().error == error::kLostContext)
    return;

  command_buffer_->SetContextLostReason(error::kUnknown);
  if (decoder_)
    decoder_->MarkContextLost(error::kUnknown);
  command_buffer_->SetParseError(error::kLostContext);
}

void GpuCommandBufferStub::SendSwapBuffersCompleted(
    const GpuCommandBufferMsg_SwapBuffersCompleted_Params& params) {
  Send(new GpuCommandBufferMsg_SwapBuffersCompleted(route_id_, params));
}

void GpuCommandBufferStub::SendUpdateVSyncParameters(base::TimeTicks timebase,
                                                     base::TimeDelta interval) {
  Send(new GpuCommandBufferMsg_UpdateVSyncParameters(route_id_, timebase,
                                                     interval));
}

}  // namespace gpu
