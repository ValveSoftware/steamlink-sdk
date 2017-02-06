// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_channel.h"

#include <utility>

#if defined(OS_WIN)
#include <windows.h>
#endif

#include <algorithm>
#include <deque>
#include <set>
#include <vector>

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/command_executor.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "ipc/ipc_channel.h"
#include "ipc/message_filter.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image_shared_memory.h"
#include "ui/gl/gl_surface.h"

#if defined(OS_POSIX)
#include "ipc/ipc_channel_posix.h"
#endif

namespace gpu {
namespace {

// Number of milliseconds between successive vsync. Many GL commands block
// on vsync, so thresholds for preemption should be multiples of this.
const int64_t kVsyncIntervalMs = 17;

// Amount of time that we will wait for an IPC to be processed before
// preempting. After a preemption, we must wait this long before triggering
// another preemption.
const int64_t kPreemptWaitTimeMs = 2 * kVsyncIntervalMs;

// Once we trigger a preemption, the maximum duration that we will wait
// before clearing the preemption.
const int64_t kMaxPreemptTimeMs = kVsyncIntervalMs;

// Stop the preemption once the time for the longest pending IPC drops
// below this threshold.
const int64_t kStopPreemptThresholdMs = kVsyncIntervalMs;

}  // anonymous namespace

scoped_refptr<GpuChannelMessageQueue> GpuChannelMessageQueue::Create(
    int32_t stream_id,
    GpuStreamPriority stream_priority,
    GpuChannel* channel,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    const scoped_refptr<PreemptionFlag>& preempting_flag,
    const scoped_refptr<PreemptionFlag>& preempted_flag,
    SyncPointManager* sync_point_manager) {
  return new GpuChannelMessageQueue(stream_id, stream_priority, channel,
                                    io_task_runner, preempting_flag,
                                    preempted_flag, sync_point_manager);
}

scoped_refptr<SyncPointOrderData>
GpuChannelMessageQueue::GetSyncPointOrderData() {
  return sync_point_order_data_;
}

GpuChannelMessageQueue::GpuChannelMessageQueue(
    int32_t stream_id,
    GpuStreamPriority stream_priority,
    GpuChannel* channel,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    const scoped_refptr<PreemptionFlag>& preempting_flag,
    const scoped_refptr<PreemptionFlag>& preempted_flag,
    SyncPointManager* sync_point_manager)
    : stream_id_(stream_id),
      stream_priority_(stream_priority),
      enabled_(true),
      scheduled_(true),
      channel_(channel),
      preemption_state_(IDLE),
      max_preemption_time_(
          base::TimeDelta::FromMilliseconds(kMaxPreemptTimeMs)),
      timer_(new base::OneShotTimer),
      sync_point_order_data_(SyncPointOrderData::Create()),
      io_task_runner_(io_task_runner),
      preempting_flag_(preempting_flag),
      preempted_flag_(preempted_flag),
      sync_point_manager_(sync_point_manager) {
  timer_->SetTaskRunner(io_task_runner);
  io_thread_checker_.DetachFromThread();
}

GpuChannelMessageQueue::~GpuChannelMessageQueue() {
  DCHECK(!enabled_);
  DCHECK(channel_messages_.empty());
}

void GpuChannelMessageQueue::Disable() {
  {
    base::AutoLock auto_lock(channel_lock_);
    DCHECK(enabled_);
    enabled_ = false;
  }

  // We guarantee that the queues will no longer be modified after enabled_
  // is set to false, it is now safe to modify the queue without the lock.
  // All public facing modifying functions check enabled_ while all
  // private modifying functions DCHECK(enabled_) to enforce this.
  while (!channel_messages_.empty()) {
    const IPC::Message& msg = channel_messages_.front()->message;
    if (msg.is_sync()) {
      IPC::Message* reply = IPC::SyncMessage::GenerateReply(&msg);
      reply->set_reply_error();
      channel_->Send(reply);
    }
    channel_messages_.pop_front();
  }

  sync_point_order_data_->Destroy();
  sync_point_order_data_ = nullptr;

  io_task_runner_->PostTask(
      FROM_HERE, base::Bind(&GpuChannelMessageQueue::DisableIO, this));
}

void GpuChannelMessageQueue::DisableIO() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  timer_ = nullptr;
}

bool GpuChannelMessageQueue::IsScheduled() const {
  base::AutoLock lock(channel_lock_);
  return scheduled_;
}

void GpuChannelMessageQueue::OnRescheduled(bool scheduled) {
  base::AutoLock lock(channel_lock_);
  DCHECK(enabled_);
  if (scheduled_ == scheduled)
    return;
  scheduled_ = scheduled;
  if (scheduled)
    channel_->PostHandleMessage(this);
  if (preempting_flag_) {
    io_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&GpuChannelMessageQueue::UpdatePreemptionState, this));
  }
}

uint32_t GpuChannelMessageQueue::GetUnprocessedOrderNum() const {
  return sync_point_order_data_->unprocessed_order_num();
}

uint32_t GpuChannelMessageQueue::GetProcessedOrderNum() const {
  return sync_point_order_data_->processed_order_num();
}

bool GpuChannelMessageQueue::PushBackMessage(const IPC::Message& message) {
  base::AutoLock auto_lock(channel_lock_);
  if (enabled_) {
    if (message.type() == GpuCommandBufferMsg_WaitForTokenInRange::ID ||
        message.type() == GpuCommandBufferMsg_WaitForGetOffsetInRange::ID) {
      channel_->PostHandleOutOfOrderMessage(message);
      return true;
    }

    uint32_t order_num = sync_point_order_data_->GenerateUnprocessedOrderNumber(
        sync_point_manager_);
    std::unique_ptr<GpuChannelMessage> msg(
        new GpuChannelMessage(message, order_num, base::TimeTicks::Now()));

    if (channel_messages_.empty()) {
      DCHECK(scheduled_);
      channel_->PostHandleMessage(this);
    }

    channel_messages_.push_back(std::move(msg));

    if (preempting_flag_)
      UpdatePreemptionStateHelper();

    return true;
  }
  return false;
}

const GpuChannelMessage* GpuChannelMessageQueue::BeginMessageProcessing() {
  base::AutoLock auto_lock(channel_lock_);
  DCHECK(enabled_);
  // If we have been preempted by another channel, just post a task to wake up.
  if (preempted_flag_ && preempted_flag_->IsSet()) {
    channel_->PostHandleMessage(this);
    return nullptr;
  }
  if (channel_messages_.empty())
    return nullptr;
  sync_point_order_data_->BeginProcessingOrderNumber(
      channel_messages_.front()->order_number);
  return channel_messages_.front().get();
}

void GpuChannelMessageQueue::PauseMessageProcessing() {
  base::AutoLock auto_lock(channel_lock_);
  DCHECK(!channel_messages_.empty());

  // If we have been preempted by another channel, just post a task to wake up.
  if (scheduled_)
    channel_->PostHandleMessage(this);

  sync_point_order_data_->PauseProcessingOrderNumber(
      channel_messages_.front()->order_number);
}

void GpuChannelMessageQueue::FinishMessageProcessing() {
  base::AutoLock auto_lock(channel_lock_);
  DCHECK(!channel_messages_.empty());
  DCHECK(scheduled_);

  sync_point_order_data_->FinishProcessingOrderNumber(
      channel_messages_.front()->order_number);
  channel_messages_.pop_front();

  if (!channel_messages_.empty())
    channel_->PostHandleMessage(this);

  if (preempting_flag_) {
    io_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&GpuChannelMessageQueue::UpdatePreemptionState, this));
  }
}

void GpuChannelMessageQueue::UpdatePreemptionState() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(preempting_flag_);
  base::AutoLock lock(channel_lock_);
  UpdatePreemptionStateHelper();
}

void GpuChannelMessageQueue::UpdatePreemptionStateHelper() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(preempting_flag_);
  channel_lock_.AssertAcquired();
  switch (preemption_state_) {
    case IDLE:
      UpdateStateIdle();
      break;
    case WAITING:
      UpdateStateWaiting();
      break;
    case CHECKING:
      UpdateStateChecking();
      break;
    case PREEMPTING:
      UpdateStatePreempting();
      break;
    case WOULD_PREEMPT_DESCHEDULED:
      UpdateStateWouldPreemptDescheduled();
      break;
    default:
      NOTREACHED();
  }
}

void GpuChannelMessageQueue::UpdateStateIdle() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(preempting_flag_);
  channel_lock_.AssertAcquired();
  DCHECK(!timer_->IsRunning());
  if (!channel_messages_.empty())
    TransitionToWaiting();
}

void GpuChannelMessageQueue::UpdateStateWaiting() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(preempting_flag_);
  channel_lock_.AssertAcquired();
  // Transition to CHECKING if timer fired.
  if (!timer_->IsRunning())
    TransitionToChecking();
}

void GpuChannelMessageQueue::UpdateStateChecking() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(preempting_flag_);
  channel_lock_.AssertAcquired();
  if (!channel_messages_.empty()) {
    base::TimeTicks time_recv = channel_messages_.front()->time_received;
    base::TimeDelta time_elapsed = base::TimeTicks::Now() - time_recv;
    if (time_elapsed.InMilliseconds() < kPreemptWaitTimeMs) {
      // Schedule another check for when the IPC may go long.
      timer_->Start(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kPreemptWaitTimeMs) - time_elapsed,
          this, &GpuChannelMessageQueue::UpdatePreemptionState);
    } else {
      timer_->Stop();
      if (!scheduled_)
        TransitionToWouldPreemptDescheduled();
      else
        TransitionToPreempting();
    }
  }
}

void GpuChannelMessageQueue::UpdateStatePreempting() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(preempting_flag_);
  channel_lock_.AssertAcquired();
  // We should stop preempting if the timer fired or for other conditions.
  if (!timer_->IsRunning() || ShouldTransitionToIdle()) {
    TransitionToIdle();
  } else if (!scheduled_) {
    // Save the remaining preemption time before stopping the timer.
    max_preemption_time_ = timer_->desired_run_time() - base::TimeTicks::Now();
    timer_->Stop();
    TransitionToWouldPreemptDescheduled();
  }
}

void GpuChannelMessageQueue::UpdateStateWouldPreemptDescheduled() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(preempting_flag_);
  channel_lock_.AssertAcquired();
  DCHECK(!timer_->IsRunning());
  if (ShouldTransitionToIdle()) {
    TransitionToIdle();
  } else if (scheduled_) {
    TransitionToPreempting();
  }
}

bool GpuChannelMessageQueue::ShouldTransitionToIdle() const {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(preempting_flag_);
  channel_lock_.AssertAcquired();
  DCHECK(preemption_state_ == PREEMPTING ||
         preemption_state_ == WOULD_PREEMPT_DESCHEDULED);
  if (channel_messages_.empty()) {
    return true;
  } else {
    base::TimeTicks next_tick = channel_messages_.front()->time_received;
    base::TimeDelta time_elapsed = base::TimeTicks::Now() - next_tick;
    if (time_elapsed.InMilliseconds() < kStopPreemptThresholdMs)
      return true;
  }
  return false;
}

void GpuChannelMessageQueue::TransitionToIdle() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(preempting_flag_);
  channel_lock_.AssertAcquired();
  DCHECK(preemption_state_ == PREEMPTING ||
         preemption_state_ == WOULD_PREEMPT_DESCHEDULED);

  preemption_state_ = IDLE;
  preempting_flag_->Reset();

  max_preemption_time_ = base::TimeDelta::FromMilliseconds(kMaxPreemptTimeMs);
  timer_->Stop();

  TRACE_COUNTER_ID1("gpu", "GpuChannel::Preempting", this, 0);

  UpdateStateIdle();
}

void GpuChannelMessageQueue::TransitionToWaiting() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(preempting_flag_);
  channel_lock_.AssertAcquired();
  DCHECK_EQ(preemption_state_, IDLE);
  DCHECK(!timer_->IsRunning());

  preemption_state_ = WAITING;

  timer_->Start(FROM_HERE,
                base::TimeDelta::FromMilliseconds(kPreemptWaitTimeMs), this,
                &GpuChannelMessageQueue::UpdatePreemptionState);
}

void GpuChannelMessageQueue::TransitionToChecking() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(preempting_flag_);
  channel_lock_.AssertAcquired();
  DCHECK_EQ(preemption_state_, WAITING);
  DCHECK(!timer_->IsRunning());

  preemption_state_ = CHECKING;

  UpdateStateChecking();
}

void GpuChannelMessageQueue::TransitionToPreempting() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(preempting_flag_);
  channel_lock_.AssertAcquired();
  DCHECK(preemption_state_ == CHECKING ||
         preemption_state_ == WOULD_PREEMPT_DESCHEDULED);
  DCHECK(scheduled_);

  preemption_state_ = PREEMPTING;
  preempting_flag_->Set();
  TRACE_COUNTER_ID1("gpu", "GpuChannel::Preempting", this, 1);

  DCHECK_LE(max_preemption_time_,
            base::TimeDelta::FromMilliseconds(kMaxPreemptTimeMs));
  timer_->Start(FROM_HERE, max_preemption_time_, this,
                &GpuChannelMessageQueue::UpdatePreemptionState);
}

void GpuChannelMessageQueue::TransitionToWouldPreemptDescheduled() {
  DCHECK(io_thread_checker_.CalledOnValidThread());
  DCHECK(preempting_flag_);
  channel_lock_.AssertAcquired();
  DCHECK(preemption_state_ == CHECKING || preemption_state_ == PREEMPTING);
  DCHECK(!scheduled_);

  preemption_state_ = WOULD_PREEMPT_DESCHEDULED;
  preempting_flag_->Reset();
  TRACE_COUNTER_ID1("gpu", "GpuChannel::Preempting", this, 0);
}

GpuChannelMessageFilter::GpuChannelMessageFilter()
    : sender_(nullptr), peer_pid_(base::kNullProcessId) {}

GpuChannelMessageFilter::~GpuChannelMessageFilter() {}

void GpuChannelMessageFilter::OnFilterAdded(IPC::Sender* sender) {
  DCHECK(!sender_);
  sender_ = sender;
  for (scoped_refptr<IPC::MessageFilter>& filter : channel_filters_) {
    filter->OnFilterAdded(sender_);
  }
}

void GpuChannelMessageFilter::OnFilterRemoved() {
  DCHECK(sender_);
  for (scoped_refptr<IPC::MessageFilter>& filter : channel_filters_) {
    filter->OnFilterRemoved();
  }
  sender_ = nullptr;
  peer_pid_ = base::kNullProcessId;
}

void GpuChannelMessageFilter::OnChannelConnected(int32_t peer_pid) {
  DCHECK(peer_pid_ == base::kNullProcessId);
  peer_pid_ = peer_pid;
  for (scoped_refptr<IPC::MessageFilter>& filter : channel_filters_) {
    filter->OnChannelConnected(peer_pid);
  }
}

void GpuChannelMessageFilter::OnChannelError() {
  for (scoped_refptr<IPC::MessageFilter>& filter : channel_filters_) {
    filter->OnChannelError();
  }
}

void GpuChannelMessageFilter::OnChannelClosing() {
  for (scoped_refptr<IPC::MessageFilter>& filter : channel_filters_) {
    filter->OnChannelClosing();
  }
}

void GpuChannelMessageFilter::AddChannelFilter(
    scoped_refptr<IPC::MessageFilter> filter) {
  channel_filters_.push_back(filter);
  if (sender_)
    filter->OnFilterAdded(sender_);
  if (peer_pid_ != base::kNullProcessId)
    filter->OnChannelConnected(peer_pid_);
}

void GpuChannelMessageFilter::RemoveChannelFilter(
    scoped_refptr<IPC::MessageFilter> filter) {
  if (sender_)
    filter->OnFilterRemoved();
  channel_filters_.erase(
      std::find(channel_filters_.begin(), channel_filters_.end(), filter));
}

// This gets called from the main thread and assumes that all messages which
// lead to creation of a new route are synchronous messages.
// TODO(sunnyps): Create routes (and streams) on the IO thread so that we can
// make the CreateCommandBuffer/VideoDecoder/VideoEncoder messages asynchronous.
void GpuChannelMessageFilter::AddRoute(
    int32_t route_id,
    const scoped_refptr<GpuChannelMessageQueue>& queue) {
  base::AutoLock lock(routes_lock_);
  routes_.insert(std::make_pair(route_id, queue));
}

void GpuChannelMessageFilter::RemoveRoute(int32_t route_id) {
  base::AutoLock lock(routes_lock_);
  routes_.erase(route_id);
}

bool GpuChannelMessageFilter::OnMessageReceived(const IPC::Message& message) {
  DCHECK(sender_);

  if (message.should_unblock() || message.is_reply())
    return MessageErrorHandler(message, "Unexpected message type");

  if (message.type() == GpuChannelMsg_Nop::ID) {
    IPC::Message* reply = IPC::SyncMessage::GenerateReply(&message);
    Send(reply);
    return true;
  }

  for (scoped_refptr<IPC::MessageFilter>& filter : channel_filters_) {
    if (filter->OnMessageReceived(message))
      return true;
  }

  scoped_refptr<GpuChannelMessageQueue> message_queue =
      LookupStreamByRoute(message.routing_id());

  if (!message_queue)
    return MessageErrorHandler(message, "Could not find message queue");

  if (!message_queue->PushBackMessage(message))
    return MessageErrorHandler(message, "Channel destroyed");

  return true;
}

bool GpuChannelMessageFilter::Send(IPC::Message* message) {
  return sender_->Send(message);
}

scoped_refptr<GpuChannelMessageQueue>
GpuChannelMessageFilter::LookupStreamByRoute(int32_t route_id) {
  base::AutoLock lock(routes_lock_);
  auto it = routes_.find(route_id);
  if (it != routes_.end())
    return it->second;
  return nullptr;
}

bool GpuChannelMessageFilter::MessageErrorHandler(const IPC::Message& message,
                                                  const char* error_msg) {
  DLOG(ERROR) << error_msg;
  if (message.is_sync()) {
    IPC::Message* reply = IPC::SyncMessage::GenerateReply(&message);
    reply->set_reply_error();
    Send(reply);
  }
  return true;
}

GpuChannel::GpuChannel(GpuChannelManager* gpu_channel_manager,
                       SyncPointManager* sync_point_manager,
                       GpuWatchdog* watchdog,
                       gl::GLShareGroup* share_group,
                       gles2::MailboxManager* mailbox,
                       PreemptionFlag* preempting_flag,
                       PreemptionFlag* preempted_flag,
                       base::SingleThreadTaskRunner* task_runner,
                       base::SingleThreadTaskRunner* io_task_runner,
                       int32_t client_id,
                       uint64_t client_tracing_id,
                       bool allow_view_command_buffers,
                       bool allow_real_time_streams)
    : gpu_channel_manager_(gpu_channel_manager),
      sync_point_manager_(sync_point_manager),
      unhandled_message_listener_(nullptr),
      channel_id_(IPC::Channel::GenerateVerifiedChannelID("gpu")),
      preempting_flag_(preempting_flag),
      preempted_flag_(preempted_flag),
      client_id_(client_id),
      client_tracing_id_(client_tracing_id),
      task_runner_(task_runner),
      io_task_runner_(io_task_runner),
      share_group_(share_group),
      mailbox_manager_(mailbox),
      watchdog_(watchdog),
      allow_view_command_buffers_(allow_view_command_buffers),
      allow_real_time_streams_(allow_real_time_streams),
      weak_factory_(this) {
  DCHECK(gpu_channel_manager);
  DCHECK(client_id);

  filter_ = new GpuChannelMessageFilter();

  scoped_refptr<GpuChannelMessageQueue> control_queue =
      CreateStream(GPU_STREAM_DEFAULT, GpuStreamPriority::HIGH);
  AddRouteToStream(MSG_ROUTING_CONTROL, GPU_STREAM_DEFAULT);
}

GpuChannel::~GpuChannel() {
  // Clear stubs first because of dependencies.
  stubs_.clear();

  for (auto& kv : streams_)
    kv.second->Disable();

  if (preempting_flag_.get())
    preempting_flag_->Reset();
}

IPC::ChannelHandle GpuChannel::Init(base::WaitableEvent* shutdown_event) {
  DCHECK(shutdown_event);
  DCHECK(!channel_);

  IPC::ChannelHandle channel_handle(channel_id_);

  channel_ =
      IPC::SyncChannel::Create(channel_handle, IPC::Channel::MODE_SERVER, this,
                               io_task_runner_, false, shutdown_event);

#if defined(OS_POSIX)
  // On POSIX, pass the renderer-side FD. Also mark it as auto-close so
  // that it gets closed after it has been sent.
  base::ScopedFD renderer_fd = channel_->TakeClientFileDescriptor();
  DCHECK(renderer_fd.is_valid());
  channel_handle.socket = base::FileDescriptor(std::move(renderer_fd));
#endif

  channel_->AddFilter(filter_.get());

  return channel_handle;
}

void GpuChannel::SetUnhandledMessageListener(IPC::Listener* listener) {
  unhandled_message_listener_ = listener;
}

base::WeakPtr<GpuChannel> GpuChannel::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

base::ProcessId GpuChannel::GetClientPID() const {
  return channel_->GetPeerPID();
}

uint32_t GpuChannel::GetProcessedOrderNum() const {
  uint32_t processed_order_num = 0;
  for (auto& kv : streams_) {
    processed_order_num =
        std::max(processed_order_num, kv.second->GetProcessedOrderNum());
  }
  return processed_order_num;
}

uint32_t GpuChannel::GetUnprocessedOrderNum() const {
  uint32_t unprocessed_order_num = 0;
  for (auto& kv : streams_) {
    unprocessed_order_num =
        std::max(unprocessed_order_num, kv.second->GetUnprocessedOrderNum());
  }
  return unprocessed_order_num;
}

bool GpuChannel::OnMessageReceived(const IPC::Message& msg) {
  // All messages should be pushed to channel_messages_ and handled separately.
  NOTREACHED();
  return false;
}

void GpuChannel::OnChannelError() {
  gpu_channel_manager_->RemoveChannel(client_id_);
}

bool GpuChannel::Send(IPC::Message* message) {
  // The GPU process must never send a synchronous IPC message to the renderer
  // process. This could result in deadlock.
  DCHECK(!message->is_sync());

  DVLOG(1) << "sending message @" << message << " on channel @" << this
           << " with type " << message->type();

  if (!channel_) {
    delete message;
    return false;
  }

  return channel_->Send(message);
}

void GpuChannel::OnStreamRescheduled(int32_t stream_id, bool scheduled) {
  scoped_refptr<GpuChannelMessageQueue> queue = LookupStream(stream_id);
  DCHECK(queue);
  queue->OnRescheduled(scheduled);
}

GpuCommandBufferStub* GpuChannel::LookupCommandBuffer(int32_t route_id) {
  return stubs_.get(route_id);
}

void GpuChannel::LoseAllContexts() {
  gpu_channel_manager_->LoseAllContexts();
}

void GpuChannel::MarkAllContextsLost() {
  for (auto& kv : stubs_)
    kv.second->MarkContextLost();
}

bool GpuChannel::AddRoute(int32_t route_id,
                          int32_t stream_id,
                          IPC::Listener* listener) {
  if (router_.AddRoute(route_id, listener)) {
    AddRouteToStream(route_id, stream_id);
    return true;
  }
  return false;
}

void GpuChannel::RemoveRoute(int32_t route_id) {
  router_.RemoveRoute(route_id);
  RemoveRouteFromStream(route_id);
}

bool GpuChannel::OnControlMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuChannel, msg)
    IPC_MESSAGE_HANDLER(GpuChannelMsg_CreateCommandBuffer,
                        OnCreateCommandBuffer)
    IPC_MESSAGE_HANDLER(GpuChannelMsg_DestroyCommandBuffer,
                        OnDestroyCommandBuffer)
    IPC_MESSAGE_HANDLER(GpuChannelMsg_GetDriverBugWorkArounds,
                        OnGetDriverBugWorkArounds)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

scoped_refptr<SyncPointOrderData> GpuChannel::GetSyncPointOrderData(
    int32_t stream_id) {
  auto it = streams_.find(stream_id);
  DCHECK(it != streams_.end());
  DCHECK(it->second);
  return it->second->GetSyncPointOrderData();
}

void GpuChannel::PostHandleMessage(
    const scoped_refptr<GpuChannelMessageQueue>& queue) {
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&GpuChannel::HandleMessage,
                                    weak_factory_.GetWeakPtr(), queue));
}

void GpuChannel::PostHandleOutOfOrderMessage(const IPC::Message& msg) {
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&GpuChannel::HandleOutOfOrderMessage,
                                    weak_factory_.GetWeakPtr(), msg));
}

void GpuChannel::HandleMessage(
    const scoped_refptr<GpuChannelMessageQueue>& message_queue) {
  const GpuChannelMessage* channel_msg =
      message_queue->BeginMessageProcessing();
  if (!channel_msg)
    return;

  const IPC::Message& msg = channel_msg->message;
  int32_t routing_id = msg.routing_id();
  GpuCommandBufferStub* stub = stubs_.get(routing_id);

  DCHECK(!stub || stub->IsScheduled());

  DVLOG(1) << "received message @" << &msg << " on channel @" << this
           << " with type " << msg.type();

  HandleMessageHelper(msg);

  // If we get descheduled or yield while processing a message.
  if ((stub && stub->HasUnprocessedCommands()) ||
      !message_queue->IsScheduled()) {
    DCHECK((uint32_t)GpuCommandBufferMsg_AsyncFlush::ID == msg.type() ||
           (uint32_t)GpuCommandBufferMsg_WaitSyncToken::ID == msg.type());
    message_queue->PauseMessageProcessing();
  } else {
    message_queue->FinishMessageProcessing();
  }
}

void GpuChannel::HandleMessageHelper(const IPC::Message& msg) {
  int32_t routing_id = msg.routing_id();

  bool handled = false;
  if (routing_id == MSG_ROUTING_CONTROL) {
    handled = OnControlMessageReceived(msg);
  } else {
    handled = router_.RouteMessage(msg);
  }

  if (!handled && unhandled_message_listener_)
    handled = unhandled_message_listener_->OnMessageReceived(msg);

  // Respond to sync messages even if router failed to route.
  if (!handled && msg.is_sync()) {
    IPC::Message* reply = IPC::SyncMessage::GenerateReply(&msg);
    reply->set_reply_error();
    Send(reply);
  }
}

void GpuChannel::HandleOutOfOrderMessage(const IPC::Message& msg) {
  HandleMessageHelper(msg);
}

void GpuChannel::HandleMessageForTesting(const IPC::Message& msg) {
  HandleMessageHelper(msg);
}

scoped_refptr<GpuChannelMessageQueue> GpuChannel::CreateStream(
    int32_t stream_id,
    GpuStreamPriority stream_priority) {
  DCHECK(streams_.find(stream_id) == streams_.end());
  scoped_refptr<GpuChannelMessageQueue> queue = GpuChannelMessageQueue::Create(
      stream_id, stream_priority, this, io_task_runner_,
      (stream_id == GPU_STREAM_DEFAULT) ? preempting_flag_ : nullptr,
      preempted_flag_, sync_point_manager_);
  streams_.insert(std::make_pair(stream_id, queue));
  streams_to_num_routes_.insert(std::make_pair(stream_id, 0));
  return queue;
}

scoped_refptr<GpuChannelMessageQueue> GpuChannel::LookupStream(
    int32_t stream_id) {
  auto stream_it = streams_.find(stream_id);
  if (stream_it != streams_.end())
    return stream_it->second;
  return nullptr;
}

void GpuChannel::DestroyStreamIfNecessary(
    const scoped_refptr<GpuChannelMessageQueue>& queue) {
  int32_t stream_id = queue->stream_id();
  if (streams_to_num_routes_[stream_id] == 0) {
    queue->Disable();
    streams_to_num_routes_.erase(stream_id);
    streams_.erase(stream_id);
  }
}

void GpuChannel::AddRouteToStream(int32_t route_id, int32_t stream_id) {
  DCHECK(streams_.find(stream_id) != streams_.end());
  DCHECK(routes_to_streams_.find(route_id) == routes_to_streams_.end());
  streams_to_num_routes_[stream_id]++;
  routes_to_streams_.insert(std::make_pair(route_id, stream_id));
  filter_->AddRoute(route_id, streams_[stream_id]);
}

void GpuChannel::RemoveRouteFromStream(int32_t route_id) {
  DCHECK(routes_to_streams_.find(route_id) != routes_to_streams_.end());
  int32_t stream_id = routes_to_streams_[route_id];
  DCHECK(streams_.find(stream_id) != streams_.end());
  routes_to_streams_.erase(route_id);
  streams_to_num_routes_[stream_id]--;
  filter_->RemoveRoute(route_id);
  DestroyStreamIfNecessary(streams_[stream_id]);
}

#if defined(OS_ANDROID)
const GpuCommandBufferStub* GpuChannel::GetOneStub() const {
  for (const auto& kv : stubs_) {
    const GpuCommandBufferStub* stub = kv.second;
    if (stub->decoder() && !stub->decoder()->WasContextLost())
      return stub;
  }
  return nullptr;
}
#endif

void GpuChannel::OnCreateCommandBuffer(
    const GPUCreateCommandBufferConfig& init_params,
    int32_t route_id,
    base::SharedMemoryHandle shared_state_handle,
    bool* result,
    gpu::Capabilities* capabilities) {
  TRACE_EVENT2("gpu", "GpuChannel::OnCreateCommandBuffer", "route_id", route_id,
               "offscreen", (init_params.surface_handle == kNullSurfaceHandle));
  std::unique_ptr<base::SharedMemory> shared_state_shm(
      new base::SharedMemory(shared_state_handle, false));
  std::unique_ptr<GpuCommandBufferStub> stub =
      CreateCommandBuffer(init_params, route_id, std::move(shared_state_shm));
  if (stub) {
    *result = true;
    *capabilities = stub->decoder()->GetCapabilities();
    stubs_.set(route_id, std::move(stub));
  } else {
    *result = false;
    *capabilities = gpu::Capabilities();
  }
}

std::unique_ptr<GpuCommandBufferStub> GpuChannel::CreateCommandBuffer(
    const GPUCreateCommandBufferConfig& init_params,
    int32_t route_id,
    std::unique_ptr<base::SharedMemory> shared_state_shm) {
  if (init_params.surface_handle != kNullSurfaceHandle &&
      !allow_view_command_buffers_) {
    DLOG(ERROR) << "GpuChannel::CreateCommandBuffer(): attempt to create a "
                   "view context on a non-priviledged channel";
    return nullptr;
  }

  int32_t share_group_id = init_params.share_group_id;
  GpuCommandBufferStub* share_group = stubs_.get(share_group_id);

  if (!share_group && share_group_id != MSG_ROUTING_NONE) {
    DLOG(ERROR)
        << "GpuChannel::CreateCommandBuffer(): invalid share group id";
    return nullptr;
  }

  int32_t stream_id = init_params.stream_id;
  if (share_group && stream_id != share_group->stream_id()) {
    DLOG(ERROR) << "GpuChannel::CreateCommandBuffer(): stream id does not "
                   "match share group stream id";
    return nullptr;
  }

  GpuStreamPriority stream_priority = init_params.stream_priority;
  if (!allow_real_time_streams_ &&
      stream_priority == GpuStreamPriority::REAL_TIME) {
    DLOG(ERROR) << "GpuChannel::CreateCommandBuffer(): real time stream "
                   "priority not allowed";
    return nullptr;
  }

  if (share_group && !share_group->decoder()) {
    // This should catch test errors where we did not Initialize the
    // share_group's CommandBuffer.
    DLOG(ERROR) << "GpuChannel::CreateCommandBuffer(): shared context was "
                   "not initialized";
    return nullptr;
  }

  if (share_group && share_group->decoder()->WasContextLost()) {
    DLOG(ERROR) << "GpuChannel::CreateCommandBuffer(): shared context was "
                   "already lost";
    return nullptr;
  }

  scoped_refptr<GpuChannelMessageQueue> queue = LookupStream(stream_id);
  if (!queue)
    queue = CreateStream(stream_id, stream_priority);

  std::unique_ptr<GpuCommandBufferStub> stub(GpuCommandBufferStub::Create(
      this, share_group, init_params, route_id, std::move(shared_state_shm)));

  if (!stub) {
    DestroyStreamIfNecessary(queue);
    return nullptr;
  }

  if (!AddRoute(route_id, stream_id, stub.get())) {
    DestroyStreamIfNecessary(queue);
    DLOG(ERROR) << "GpuChannel::CreateCommandBuffer(): failed to add route";
    return nullptr;
  }

  return stub;
}

void GpuChannel::OnDestroyCommandBuffer(int32_t route_id) {
  TRACE_EVENT1("gpu", "GpuChannel::OnDestroyCommandBuffer",
               "route_id", route_id);

  std::unique_ptr<GpuCommandBufferStub> stub = stubs_.take_and_erase(route_id);
  // In case the renderer is currently blocked waiting for a sync reply from the
  // stub, we need to make sure to reschedule the correct stream here.
  if (stub && !stub->IsScheduled()) {
    // This stub won't get a chance to reschedule the stream so do that now.
    OnStreamRescheduled(stub->stream_id(), true);
  }

  RemoveRoute(route_id);
}

void GpuChannel::OnGetDriverBugWorkArounds(
    std::vector<std::string>* gpu_driver_bug_workarounds) {
  gpu_driver_bug_workarounds->clear();
#define GPU_OP(type, name)                                     \
  if (gpu_channel_manager_->gpu_driver_bug_workarounds().name) \
    gpu_driver_bug_workarounds->push_back(#name);
  GPU_DRIVER_BUG_WORKAROUNDS(GPU_OP)
#undef GPU_OP
}

void GpuChannel::CacheShader(const std::string& key,
                             const std::string& shader) {
  gpu_channel_manager_->delegate()->StoreShaderToDisk(client_id_, key, shader);
}

void GpuChannel::AddFilter(IPC::MessageFilter* filter) {
  io_task_runner_->PostTask(
      FROM_HERE, base::Bind(&GpuChannelMessageFilter::AddChannelFilter,
                            filter_, make_scoped_refptr(filter)));
}

void GpuChannel::RemoveFilter(IPC::MessageFilter* filter) {
  io_task_runner_->PostTask(
      FROM_HERE, base::Bind(&GpuChannelMessageFilter::RemoveChannelFilter,
                            filter_, make_scoped_refptr(filter)));
}

uint64_t GpuChannel::GetMemoryUsage() {
  // Collect the unique memory trackers in use by the |stubs_|.
  std::set<gles2::MemoryTracker*> unique_memory_trackers;
  for (auto& kv : stubs_)
    unique_memory_trackers.insert(kv.second->GetMemoryTracker());

  // Sum the memory usage for all unique memory trackers.
  uint64_t size = 0;
  for (auto* tracker : unique_memory_trackers) {
    size += gpu_channel_manager()->gpu_memory_manager()->GetTrackerMemoryUsage(
        tracker);
  }

  return size;
}

scoped_refptr<gl::GLImage> GpuChannel::CreateImageForGpuMemoryBuffer(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    uint32_t internalformat,
    SurfaceHandle surface_handle) {
  switch (handle.type) {
    case gfx::SHARED_MEMORY_BUFFER: {
      if (!base::IsValueInRangeForNumericType<size_t>(handle.stride))
        return nullptr;
      scoped_refptr<gl::GLImageSharedMemory> image(
          new gl::GLImageSharedMemory(size, internalformat));
      if (!image->Initialize(handle.handle, handle.id, format, handle.offset,
                             handle.stride)) {
        return nullptr;
      }

      return image;
    }
    default: {
      GpuChannelManager* manager = gpu_channel_manager();
      if (!manager->gpu_memory_buffer_factory())
        return nullptr;

      return manager->gpu_memory_buffer_factory()
          ->AsImageFactory()
          ->CreateImageForGpuMemoryBuffer(handle, size, format, internalformat,
                                          client_id_, surface_handle);
    }
  }
}

}  // namespace gpu
