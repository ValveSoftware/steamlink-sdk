// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/context/blimp_client_context_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "blimp/client/core/compositor/blimp_compositor_dependencies.h"
#include "blimp/client/core/compositor/blob_channel_feature.h"
#include "blimp/client/core/contents/blimp_contents_impl.h"
#include "blimp/client/core/contents/blimp_contents_manager.h"
#include "blimp/client/core/contents/ime_feature.h"
#include "blimp/client/core/contents/navigation_feature.h"
#include "blimp/client/core/contents/tab_control_feature.h"
#include "blimp/client/core/feedback/blimp_feedback_data.h"
#include "blimp/client/core/geolocation/geolocation_feature.h"
#include "blimp/client/core/render_widget/render_widget_feature.h"
#include "blimp/client/core/session/cross_thread_network_event_observer.h"
#include "blimp/client/core/settings/settings.h"
#include "blimp/client/core/settings/settings_feature.h"
#include "blimp/client/core/switches/blimp_client_switches.h"
#include "blimp/client/public/blimp_client_context_delegate.h"
#include "blimp/client/public/compositor/compositor_dependencies.h"
#include "components/prefs/pref_service.h"
#include "device/geolocation/geolocation_delegate.h"
#include "device/geolocation/location_arbitrator.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_ANDROID)
#include "blimp/client/core/context/android/blimp_client_context_impl_android.h"
#include "blimp/client/core/settings/android/settings_android.h"
#endif  // OS_ANDROID

namespace blimp {
namespace client {

namespace {

const char kDefaultAssignerUrl[] =
    "https://blimp-pa.googleapis.com/v1/assignment";

void DropConnectionOnIOThread(ClientNetworkComponents* net_components) {
  net_components->GetBrowserConnectionHandler()->DropCurrentConnection();
}

}  // namespace

// This function is declared in //blimp/client/public/blimp_client_context.h,
// and either this function or the one in
// //blimp/client/core/dummy_blimp_client_context.cc should be linked in to
// any binary using BlimpClientContext::Create.
// static
BlimpClientContext* BlimpClientContext::Create(
    scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> file_thread_task_runner,
    std::unique_ptr<CompositorDependencies> compositor_dependencies,
    PrefService* local_state) {
#if defined(OS_ANDROID)
  auto settings = base::MakeUnique<SettingsAndroid>(local_state);
  return new BlimpClientContextImplAndroid(
      io_thread_task_runner, file_thread_task_runner,
      std::move(compositor_dependencies), std::move(settings));
#else
  auto settings = base::MakeUnique<Settings>(local_state);
  return new BlimpClientContextImpl(
      io_thread_task_runner, file_thread_task_runner,
      std::move(compositor_dependencies), std::move(settings));
#endif  // defined(OS_ANDROID)
}

// This function is declared in //blimp/client/public/blimp_client_context.h
// and either this function or the one in
// //blimp/client/core/dummy_blimp_client_context.cc should be linked in to
// any binary using BlimpClientContext::RegisterPrefs.
// static
void BlimpClientContext::RegisterPrefs(PrefRegistrySimple* registry) {
  Settings::RegisterPrefs(registry);
}

// This function is declared in //blimp/client/public/blimp_client_context.h
// and either this function or the one in
// //blimp/client/core/dummy_blimp_client_context.cc should be linked in to
// any binary using BlimpClientContext::ApplyBlimpSwitches.
// static
void BlimpClientContext::ApplyBlimpSwitches(CommandLinePrefStore* store) {
  Settings::ApplyBlimpSwitches(store);
}

BlimpClientContextImpl::BlimpClientContextImpl(
    scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> file_thread_task_runner,
    std::unique_ptr<CompositorDependencies> compositor_dependencies,
    std::unique_ptr<Settings> settings)
    : BlimpClientContext(),
      io_thread_task_runner_(io_thread_task_runner),
      file_thread_task_runner_(file_thread_task_runner),
      blimp_compositor_dependencies_(
          base::MakeUnique<BlimpCompositorDependencies>(
              std::move(compositor_dependencies))),
      settings_(std::move(settings)),
      blob_channel_feature_(new BlobChannelFeature(this)),
      geolocation_feature_(base::MakeUnique<GeolocationFeature>(
          base::MakeUnique<device::LocationArbitrator>(
              base::MakeUnique<device::GeolocationDelegate>()))),
      ime_feature_(new ImeFeature),
      navigation_feature_(new NavigationFeature),
      render_widget_feature_(new RenderWidgetFeature),
      settings_feature_(base::MakeUnique<SettingsFeature>(settings_.get())),
      tab_control_feature_(new TabControlFeature),
      blimp_contents_manager_(
          new BlimpContentsManager(blimp_compositor_dependencies_.get(),
                                   ime_feature_.get(),
                                   navigation_feature_.get(),
                                   render_widget_feature_.get(),
                                   tab_control_feature_.get())),
      weak_factory_(this) {
  net_components_.reset(new ClientNetworkComponents(
      base::MakeUnique<CrossThreadNetworkEventObserver>(
          connection_status_.GetWeakPtr(),
          base::SequencedTaskRunnerHandle::Get())));

  // The |thread_pipe_manager_| must be set up correctly before features are
  // registered.
  thread_pipe_manager_ = base::MakeUnique<ThreadPipeManager>(
      io_thread_task_runner_, net_components_->GetBrowserConnectionHandler());

  RegisterFeatures();
  settings_feature_->PushSettings();

  connection_status_.AddObserver(this);

  // Initialize must only be posted after the features have been
  // registered.
  io_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&ClientNetworkComponents::Initialize,
                            base::Unretained(net_components_.get())));

  UMA_HISTOGRAM_BOOLEAN("Blimp.Supported", true);
}

BlimpClientContextImpl::~BlimpClientContextImpl() {
  io_thread_task_runner_->DeleteSoon(FROM_HERE, net_components_.release());
  connection_status_.RemoveObserver(this);
}

void BlimpClientContextImpl::SetDelegate(BlimpClientContextDelegate* delegate) {
  DCHECK(!delegate_ || !delegate);
  delegate_ = delegate;

  // TODO(xingliu): Pass the IdentityProvider needed by |assignment_fetcher_|
  // in the constructor, see crbug/661848.
  if (delegate_) {
    assignment_fetcher_ = base::MakeUnique<AssignmentFetcher>(
        io_thread_task_runner_, file_thread_task_runner_,
        delegate_->CreateIdentityProvider(), GetAssignerURL(),
        base::Bind(&BlimpClientContextImpl::OnAssignmentReceived,
                   weak_factory_.GetWeakPtr()),
        base::Bind(&BlimpClientContextDelegate::OnAuthenticationError,
                   base::Unretained(delegate_)));
  }
}

std::unique_ptr<BlimpContents> BlimpClientContextImpl::CreateBlimpContents(
    gfx::NativeWindow window) {
  std::unique_ptr<BlimpContents> blimp_contents =
      blimp_contents_manager_->CreateBlimpContents(window);
  if (blimp_contents)
    delegate_->AttachBlimpContentsHelpers(blimp_contents.get());
  return blimp_contents;
}

void BlimpClientContextImpl::Connect() {
  DCHECK(delegate_);
  DCHECK(assignment_fetcher_);
  assignment_fetcher_->Fetch();
}

void BlimpClientContextImpl::ConnectWithAssignment(
    const Assignment& assignment) {
  io_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ClientNetworkComponents::ConnectWithAssignment,
                 base::Unretained(net_components_.get()), assignment));
}

std::unordered_map<std::string, std::string>
BlimpClientContextImpl::CreateFeedbackData() {
  IdentitySource* identity_source = GetIdentitySource();
  DCHECK(identity_source);
  return CreateBlimpFeedbackData(blimp_contents_manager_.get(),
                                 identity_source->GetActiveUsername());
}

IdentitySource* BlimpClientContextImpl::GetIdentitySource() {
  DCHECK(delegate_);
  DCHECK(assignment_fetcher_);
  return assignment_fetcher_->GetIdentitySource();
}

ConnectionStatus* BlimpClientContextImpl::GetConnectionStatus() {
  return &connection_status_;
}

GURL BlimpClientContextImpl::GetAssignerURL() {
  return GURL(kDefaultAssignerUrl);
}

void BlimpClientContextImpl::OnAssignmentReceived(
    AssignmentRequestResult result,
    const Assignment& assignment) {
  VLOG(1) << "Assignment result: " << result;

  // Cache engine info.
  connection_status_.OnAssignmentResult(result, assignment);

  // Inform the embedder of the assignment result.
  if (delegate_) {
    delegate_->OnAssignmentConnectionAttempted(result, assignment);
  }

  if (result != ASSIGNMENT_REQUEST_RESULT_OK) {
    LOG(ERROR) << "Assignment failed, reason: " << result;
    return;
  }

  ConnectWithAssignment(assignment);
}

void BlimpClientContextImpl::RegisterFeatures() {
  // Register features' message senders and receivers.
  thread_pipe_manager_->RegisterFeature(BlimpMessage::kBlobChannel,
                                        blob_channel_feature_.get());
  geolocation_feature_->set_outgoing_message_processor(
      thread_pipe_manager_->RegisterFeature(BlimpMessage::kGeolocation,
                                            geolocation_feature_.get()));
  ime_feature_->set_outgoing_message_processor(
      thread_pipe_manager_->RegisterFeature(BlimpMessage::kIme,
                                            ime_feature_.get()));
  navigation_feature_->set_outgoing_message_processor(
      thread_pipe_manager_->RegisterFeature(BlimpMessage::kNavigation,
                                            navigation_feature_.get()));
  render_widget_feature_->set_outgoing_input_message_processor(
      thread_pipe_manager_->RegisterFeature(BlimpMessage::kInput,
                                            render_widget_feature_.get()));
  render_widget_feature_->set_outgoing_compositor_message_processor(
      thread_pipe_manager_->RegisterFeature(BlimpMessage::kCompositor,
                                            render_widget_feature_.get()));
  thread_pipe_manager_->RegisterFeature(BlimpMessage::kRenderWidget,
                                        render_widget_feature_.get());
  settings_feature_->set_outgoing_message_processor(
      thread_pipe_manager_->RegisterFeature(BlimpMessage::kSettings,
                                            settings_feature_.get()));
  tab_control_feature_->set_outgoing_message_processor(
      thread_pipe_manager_->RegisterFeature(BlimpMessage::kTabControl,
                                            tab_control_feature_.get()));
}

void BlimpClientContextImpl::DropConnection() {
  io_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&DropConnectionOnIOThread, net_components_.get()));
}

void BlimpClientContextImpl::OnImageDecodeError() {
  // Currently we just drop the connection on image decoding error.
  DropConnection();
}

void BlimpClientContextImpl::OnConnected() {
  if (delegate_) {
    delegate_->OnConnected();
  }
}

void BlimpClientContextImpl::OnDisconnected(int result) {
  if (delegate_) {
    if (result >= 0) {
      delegate_->OnEngineDisconnected(result);
    } else {
      delegate_->OnNetworkDisconnected(result);
    }
  }
}

}  // namespace client
}  // namespace blimp
