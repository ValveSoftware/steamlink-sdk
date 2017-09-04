// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/app/session/blimp_client_session.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "blimp/client/core/contents/ime_feature.h"
#include "blimp/client/core/contents/navigation_feature.h"
#include "blimp/client/core/contents/tab_control_feature.h"
#include "blimp/client/core/geolocation/geolocation_feature.h"
#include "blimp/client/core/render_widget/render_widget_feature.h"
#include "blimp/client/core/session/client_network_components.h"
#include "blimp/client/core/session/cross_thread_network_event_observer.h"
#include "blimp/client/core/switches/blimp_client_switches.h"
#include "blimp/common/blob_cache/in_memory_blob_cache.h"
#include "blimp/net/blimp_message_thread_pipe.h"
#include "blimp/net/blimp_stats.h"
#include "blimp/net/blob_channel/blob_channel_receiver.h"
#include "blimp/net/blob_channel/helium_blob_receiver_delegate.h"
#include "blimp/net/thread_pipe_manager.h"
#include "device/geolocation/geolocation_delegate.h"
#include "device/geolocation/location_arbitrator.h"
#include "url/gurl.h"

namespace blimp {
namespace client {
namespace {

void DropConnectionOnIOThread(ClientNetworkComponents* net_components) {
  net_components->GetBrowserConnectionHandler()->DropCurrentConnection();
}

}  // namespace

BlimpClientSession::BlimpClientSession(const GURL& assigner_endpoint)
    : io_thread_("BlimpIOThread"),
      geolocation_feature_(base::MakeUnique<GeolocationFeature>(
          base::MakeUnique<device::LocationArbitrator>(
              base::MakeUnique<device::GeolocationDelegate>()))),
      tab_control_feature_(new TabControlFeature),
      navigation_feature_(new NavigationFeature),
      ime_feature_(new ImeFeature),
      render_widget_feature_(new RenderWidgetFeature),
      weak_factory_(this) {
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  io_thread_.StartWithOptions(options);
  net_components_.reset(new ClientNetworkComponents(
      base::MakeUnique<CrossThreadNetworkEventObserver>(
          weak_factory_.GetWeakPtr(), base::SequencedTaskRunnerHandle::Get())));

  assignment_source_.reset(new AssignmentSource(
      assigner_endpoint, io_thread_.task_runner(), io_thread_.task_runner()));

  std::unique_ptr<HeliumBlobReceiverDelegate> blob_delegate(
      new HeliumBlobReceiverDelegate);
  blob_delegate_ = blob_delegate.get();
  blob_receiver_ = BlobChannelReceiver::Create(
      base::WrapUnique(new InMemoryBlobCache), std::move(blob_delegate));

  RegisterFeatures();

  blob_image_processor_.set_blob_receiver(blob_receiver_.get());
  blob_image_processor_.set_error_delegate(this);

  // Initialize must only be posted after the RegisterFeature calls have
  // completed.
  io_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&ClientNetworkComponents::Initialize,
                            base::Unretained(net_components_.get())));
}

BlimpClientSession::~BlimpClientSession() {
  io_thread_.task_runner()->DeleteSoon(FROM_HERE, net_components_.release());
}

void BlimpClientSession::Connect(const std::string& client_auth_token) {
  VLOG(1) << "Trying to get assignment.";
  assignment_source_->GetAssignment(
      client_auth_token, base::Bind(&BlimpClientSession::ConnectWithAssignment,
                                    weak_factory_.GetWeakPtr()));
}

void BlimpClientSession::ConnectWithAssignment(AssignmentRequestResult result,
                                               const Assignment& assignment) {
  OnAssignmentConnectionAttempted(result, assignment);

  if (result != ASSIGNMENT_REQUEST_RESULT_OK) {
    LOG(ERROR) << "Assignment failed, reason: " << result;
    return;
  }

  VLOG(1) << "Assignment succeeded";

  io_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&ClientNetworkComponents::ConnectWithAssignment,
                 base::Unretained(net_components_.get()), assignment));
}

void BlimpClientSession::OnAssignmentConnectionAttempted(
    AssignmentRequestResult result,
    const Assignment& assignment) {}

void BlimpClientSession::RegisterFeatures() {
  thread_pipe_manager_ = base::MakeUnique<ThreadPipeManager>(
      io_thread_.task_runner(), net_components_->GetBrowserConnectionHandler());

  // Register features' message senders and receivers.
  geolocation_feature_->set_outgoing_message_processor(
      thread_pipe_manager_->RegisterFeature(BlimpMessage::kGeolocation,
                                            geolocation_feature_.get()));
  tab_control_feature_->set_outgoing_message_processor(
      thread_pipe_manager_->RegisterFeature(BlimpMessage::kTabControl,
                                            tab_control_feature_.get()));
  navigation_feature_->set_outgoing_message_processor(
      thread_pipe_manager_->RegisterFeature(BlimpMessage::kNavigation,
                                            navigation_feature_.get()));
  render_widget_feature_->set_outgoing_input_message_processor(
      thread_pipe_manager_->RegisterFeature(BlimpMessage::kInput,
                                            render_widget_feature_.get()));
  render_widget_feature_->set_outgoing_compositor_message_processor(
      thread_pipe_manager_->RegisterFeature(BlimpMessage::kCompositor,
                                            render_widget_feature_.get()));
  thread_pipe_manager_->RegisterFeature(BlimpMessage::kBlobChannel,
                                        blob_delegate_);

  // Client will not send send any RenderWidget messages, so don't save the
  // outgoing BlimpMessageProcessor in the RenderWidgetFeature.
  thread_pipe_manager_->RegisterFeature(BlimpMessage::kRenderWidget,
                                        render_widget_feature_.get());

  ime_feature_->set_outgoing_message_processor(
      thread_pipe_manager_->RegisterFeature(BlimpMessage::kIme,
                                            ime_feature_.get()));
}

void BlimpClientSession::DropConnection() {
  io_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&DropConnectionOnIOThread, net_components_.get()));
}

void BlimpClientSession::OnConnected() {}

void BlimpClientSession::OnDisconnected(int result) {}

void BlimpClientSession::OnImageDecodeError() {
  DropConnection();
}

TabControlFeature* BlimpClientSession::GetTabControlFeature() const {
  return tab_control_feature_.get();
}

NavigationFeature* BlimpClientSession::GetNavigationFeature() const {
  return navigation_feature_.get();
}

ImeFeature* BlimpClientSession::GetImeFeature() const {
  return ime_feature_.get();
}

RenderWidgetFeature* BlimpClientSession::GetRenderWidgetFeature() const {
  return render_widget_feature_.get();
}

}  // namespace client
}  // namespace blimp
