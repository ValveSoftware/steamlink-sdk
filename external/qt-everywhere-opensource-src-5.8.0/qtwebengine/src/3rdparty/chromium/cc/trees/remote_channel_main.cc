// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/remote_channel_main.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "cc/proto/base_conversions.h"
#include "cc/proto/compositor_message.pb.h"
#include "cc/proto/compositor_message_to_impl.pb.h"
#include "cc/proto/compositor_message_to_main.pb.h"
#include "cc/proto/gfx_conversions.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/proxy_main.h"

namespace cc {

std::unique_ptr<RemoteChannelMain> RemoteChannelMain::Create(
    RemoteProtoChannel* remote_proto_channel,
    ProxyMain* proxy_main,
    TaskRunnerProvider* task_runner_provider) {
  return base::WrapUnique(new RemoteChannelMain(
      remote_proto_channel, proxy_main, task_runner_provider));
}

RemoteChannelMain::RemoteChannelMain(RemoteProtoChannel* remote_proto_channel,
                                     ProxyMain* proxy_main,
                                     TaskRunnerProvider* task_runner_provider)
    : remote_proto_channel_(remote_proto_channel),
      proxy_main_(proxy_main),
      task_runner_provider_(task_runner_provider),
      initialized_(false),
      weak_factory_(this) {
  TRACE_EVENT0("cc.remote", "RemoteChannelMain::RemoteChannelMain");
  DCHECK(remote_proto_channel_);
  DCHECK(proxy_main_);
  DCHECK(task_runner_provider_);
  DCHECK(task_runner_provider_->IsMainThread());
  remote_proto_channel_->SetProtoReceiver(this);
}

RemoteChannelMain::~RemoteChannelMain() {
  TRACE_EVENT0("cc.remote", "~RemoteChannelMain::RemoteChannelMain");
  DCHECK(task_runner_provider_->IsMainThread());
  DCHECK(!initialized_);

  remote_proto_channel_->SetProtoReceiver(nullptr);
}

void RemoteChannelMain::OnProtoReceived(
    std::unique_ptr<proto::CompositorMessage> proto) {
  TRACE_EVENT0("cc.remote", "RemoteChannelMain::OnProtoReceived");
  DCHECK(task_runner_provider_->IsMainThread());
  DCHECK(proto->has_to_main());

  HandleProto(proto->to_main());
}

void RemoteChannelMain::UpdateTopControlsStateOnImpl(
    TopControlsState constraints,
    TopControlsState current,
    bool animate) {}

void RemoteChannelMain::InitializeOutputSurfaceOnImpl(
    OutputSurface* output_surface) {
  NOTREACHED() << "Should not be called on the server LayerTreeHost";
}

void RemoteChannelMain::InitializeMutatorOnImpl(
    std::unique_ptr<LayerTreeMutator> mutator) {
  // TODO(vollick): add support for CompositorWorker.
  NOTIMPLEMENTED();
}

void RemoteChannelMain::MainThreadHasStoppedFlingingOnImpl() {
  proto::CompositorMessage proto;
  proto::CompositorMessageToImpl* to_impl_proto = proto.mutable_to_impl();
  to_impl_proto->set_message_type(
      proto::CompositorMessageToImpl::MAIN_THREAD_HAS_STOPPED_FLINGING_ON_IMPL);

  SendMessageProto(proto);
}

void RemoteChannelMain::SetInputThrottledUntilCommitOnImpl(bool is_throttled) {}

void RemoteChannelMain::SetDeferCommitsOnImpl(bool defer_commits) {
  TRACE_EVENT1("cc.remote", "RemoteChannelMain::SetDeferCommitsOnImpl",
               "defer_commits", defer_commits);
  proto::CompositorMessage proto;
  proto::CompositorMessageToImpl* to_impl_proto = proto.mutable_to_impl();
  to_impl_proto->set_message_type(
      proto::CompositorMessageToImpl::SET_DEFER_COMMITS);
  proto::SetDeferCommits* defer_commits_message =
      to_impl_proto->mutable_defer_commits_message();
  defer_commits_message->set_defer_commits(defer_commits);

  VLOG(1) << "Sending defer commits: " << defer_commits << " to client.";
  SendMessageProto(proto);
}

void RemoteChannelMain::FinishAllRenderingOnImpl(CompletionEvent* completion) {
  completion->Signal();
}

void RemoteChannelMain::SetVisibleOnImpl(bool visible) {
  NOTIMPLEMENTED() << "Visibility is not controlled by the server";
}

void RemoteChannelMain::ReleaseOutputSurfaceOnImpl(
    CompletionEvent* completion) {
  NOTREACHED() << "Should not be called on the server LayerTreeHost";
  completion->Signal();
}

void RemoteChannelMain::MainFrameWillHappenOnImplForTesting(
    CompletionEvent* completion,
    bool* main_frame_will_happen) {
  // For the LayerTreeTests in remote mode, LayerTreeTest directly calls
  // RemoteChannelImpl::MainFrameWillHappenForTesting and avoids adding a
  // message type for tests to the compositor protocol.
  NOTREACHED();
}

void RemoteChannelMain::SetNeedsRedrawOnImpl(const gfx::Rect& damage_rect) {
  TRACE_EVENT0("cc.remote", "RemoteChannelMain::SetNeedsRedrawOnImpl");
  proto::CompositorMessage proto;
  proto::CompositorMessageToImpl* to_impl_proto = proto.mutable_to_impl();
  to_impl_proto->set_message_type(
      proto::CompositorMessageToImpl::SET_NEEDS_REDRAW);
  proto::SetNeedsRedraw* set_needs_redraw_message =
      to_impl_proto->mutable_set_needs_redraw_message();
  RectToProto(damage_rect, set_needs_redraw_message->mutable_damaged_rect());

  VLOG(1) << "Sending redraw request to client.";
  SendMessageProto(proto);

  // The client will not inform us when the frame buffers are swapped.
  MainThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&RemoteChannelMain::DidCompleteSwapBuffers,
                            weak_factory_.GetWeakPtr()));
}

void RemoteChannelMain::SetNeedsCommitOnImpl() {
  TRACE_EVENT0("cc.remote", "RemoteChannelMain::SetNeedsCommitOnImpl");
  proto::CompositorMessage proto;
  proto::CompositorMessageToImpl* to_impl_proto = proto.mutable_to_impl();
  to_impl_proto->set_message_type(
      proto::CompositorMessageToImpl::SET_NEEDS_COMMIT);

  VLOG(1) << "Sending commit request to client.";
  SendMessageProto(proto);
}

void RemoteChannelMain::BeginMainFrameAbortedOnImpl(
    CommitEarlyOutReason reason,
    base::TimeTicks main_thread_start_time) {
  TRACE_EVENT1("cc.remote", "RemoteChannelMain::BeginMainFrameAbortedOnImpl",
               "reason", CommitEarlyOutReasonToString(reason));
  proto::CompositorMessage proto;
  proto::CompositorMessageToImpl* to_impl_proto = proto.mutable_to_impl();
  to_impl_proto->set_message_type(
      proto::CompositorMessageToImpl::BEGIN_MAIN_FRAME_ABORTED);
  proto::BeginMainFrameAborted* begin_main_frame_aborted_message =
      to_impl_proto->mutable_begin_main_frame_aborted_message();
  CommitEarlyOutReasonToProtobuf(
      reason, begin_main_frame_aborted_message->mutable_reason());

  VLOG(1) << "Sending BeginMainFrameAborted message to client with reason: "
          << CommitEarlyOutReasonToString(reason);
  SendMessageProto(proto);
}

void RemoteChannelMain::StartCommitOnImpl(
    CompletionEvent* completion,
    LayerTreeHost* layer_tree_host,
    base::TimeTicks main_thread_start_time,
    bool hold_commit_for_activation) {
  TRACE_EVENT0("cc.remote", "RemoteChannelMain::StartCommitOnImpl");
  proto::CompositorMessage proto;
  proto::CompositorMessageToImpl* to_impl_proto = proto.mutable_to_impl();
  to_impl_proto->set_message_type(proto::CompositorMessageToImpl::START_COMMIT);
  proto::StartCommit* start_commit_message =
      to_impl_proto->mutable_start_commit_message();
  std::vector<std::unique_ptr<SwapPromise>> swap_promises;
  layer_tree_host->ToProtobufForCommit(
      start_commit_message->mutable_layer_tree_host(), &swap_promises);

  VLOG(1) << "Sending commit message to client. Commit bytes size: "
          << proto.ByteSize();
  SendMessageProto(proto);

  // Activate the swap promises after the commit is queued.
  // In the threaded compositor, activation implies that the pending tree on the
  // impl thread has been activated. While the impl thread for the remote
  // compositor is on the client, the embedder still expects to receive these
  // events in order to drive decisions that depend on impl frame production.
  // So we dispatch these events after a commit message is sent to the client.
  // Sending the commit message implies that a visual update has been queued for
  // the client, which is the closest we can come to offering an indicator akin
  // to an impl frame queued for display.
  for (const auto& swap_promise : swap_promises)
    swap_promise->DidActivate();

  // In order to avoid incurring the overhead for the client to send us a
  // message for when a frame to be committed is drawn we inform the embedder
  // that the draw was successful immediately after sending the commit message.
  // Since the compositing state may be used by the embedder to throttle
  // commit/draw requests, it is better to allow them to propagate rather than
  // incurring a round-trip to get Acks for draw from the client for each frame.

  // This is done as a separate PostTask to ensure that these calls run after
  // LayerTreeHostClient::DidCommit and stay consistent with the
  // behaviour in the threaded compositor.
  MainThreadTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&RemoteChannelMain::DidCommitAndDrawFrame,
                            weak_factory_.GetWeakPtr()));

  completion->Signal();
}

void RemoteChannelMain::SynchronouslyInitializeImpl(
    LayerTreeHost* layer_tree_host,
    std::unique_ptr<BeginFrameSource> external_begin_frame_source) {
  TRACE_EVENT0("cc.remote", "RemoteChannelMain::SynchronouslyInitializeImpl");
  DCHECK(!initialized_);

  proto::CompositorMessage proto;
  proto::CompositorMessageToImpl* to_impl_proto = proto.mutable_to_impl();
  to_impl_proto->set_message_type(
      proto::CompositorMessageToImpl::INITIALIZE_IMPL);
  proto::InitializeImpl* initialize_impl_proto =
      to_impl_proto->mutable_initialize_impl_message();
  proto::LayerTreeSettings* settings_proto =
      initialize_impl_proto->mutable_layer_tree_settings();
  layer_tree_host->settings().ToProtobuf(settings_proto);

  VLOG(1) << "Sending initialize message to client";
  SendMessageProto(proto);
  initialized_ = true;
}

void RemoteChannelMain::SynchronouslyCloseImpl() {
  TRACE_EVENT0("cc.remote", "RemoteChannelMain::SynchronouslyCloseImpl");
  DCHECK(initialized_);
  proto::CompositorMessage proto;
  proto::CompositorMessageToImpl* to_impl_proto = proto.mutable_to_impl();
  to_impl_proto->set_message_type(proto::CompositorMessageToImpl::CLOSE_IMPL);

  VLOG(1) << "Sending close message to client.";
  SendMessageProto(proto);
  initialized_ = false;
}

void RemoteChannelMain::SendMessageProto(
    const proto::CompositorMessage& proto) {
  TRACE_EVENT0("cc.remote", "RemoteChannelMain::SendMessageProto");
  remote_proto_channel_->SendCompositorProto(proto);
}

void RemoteChannelMain::HandleProto(
    const proto::CompositorMessageToMain& proto) {
  TRACE_EVENT0("cc.remote", "RemoteChannelMain::HandleProto");
  DCHECK(proto.has_message_type());

  switch (proto.message_type()) {
    case proto::CompositorMessageToMain::UNKNOWN:
      NOTIMPLEMENTED() << "Ignoring message proto of unknown type";
      break;
    case proto::CompositorMessageToMain::BEGIN_MAIN_FRAME: {
      TRACE_EVENT0("cc.remote", "RemoteChannelMain::BeginMainFrame");
      VLOG(1) << "Received BeginMainFrame request from client.";
      const proto::BeginMainFrame& begin_main_frame_message =
          proto.begin_main_frame_message();
      std::unique_ptr<BeginMainFrameAndCommitState> begin_main_frame_state;
      begin_main_frame_state.reset(new BeginMainFrameAndCommitState);
      begin_main_frame_state->FromProtobuf(
          begin_main_frame_message.begin_main_frame_state());
      proxy_main_->BeginMainFrame(std::move(begin_main_frame_state));
    } break;
  }
}

void RemoteChannelMain::DidCommitAndDrawFrame() {
  proxy_main_->DidCommitAndDrawFrame();
  DidCompleteSwapBuffers();
}

void RemoteChannelMain::DidCompleteSwapBuffers() {
  proxy_main_->DidCompleteSwapBuffers();
}

base::SingleThreadTaskRunner* RemoteChannelMain::MainThreadTaskRunner() const {
  return task_runner_provider_->MainThreadTaskRunner();
}

}  // namespace cc
