// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/remoting_cdm_factory.h"

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "media/base/cdm_config.h"
#include "media/remoting/remoting_cdm.h"

namespace media {

RemotingCdmFactory::RemotingCdmFactory(
    std::unique_ptr<CdmFactory> default_cdm_factory,
    mojom::RemoterFactory* remoter_factory,
    std::unique_ptr<RemotingSinkObserver> sink_observer)
    : default_cdm_factory_(std::move(default_cdm_factory)),
      remoter_factory_(remoter_factory),
      sink_observer_(std::move(sink_observer)),
      weak_factory_(this) {
  DCHECK(default_cdm_factory_);
  DCHECK(remoter_factory_);
  DCHECK(sink_observer_);
}

RemotingCdmFactory::~RemotingCdmFactory() {}

std::unique_ptr<RemotingCdmController>
RemotingCdmFactory::CreateRemotingCdmController() {
  mojom::RemotingSourcePtr remoting_source;
  mojom::RemotingSourceRequest remoting_source_request =
      mojo::GetProxy(&remoting_source);
  mojom::RemoterPtr remoter;
  remoter_factory_->Create(std::move(remoting_source),
                           mojo::GetProxy(&remoter));
  scoped_refptr<RemotingSourceImpl> remoting_source_impl =
      new RemotingSourceImpl(std::move(remoting_source_request),
                             std::move(remoter));
  // HACK: Copy-over the sink availability status from |sink_observer_| before
  // the RemotingCdmController would naturally get the notification. This is to
  // avoid the possible delay on OnSinkAvailable() call from browser.
  if (sink_observer_->is_sink_available())
    remoting_source_impl->OnSinkAvailable();
  return base::MakeUnique<RemotingCdmController>(remoting_source_impl);
}

// TODO(xjz): Replace the callbacks with an interface. http://crbug.com/657940.
void RemotingCdmFactory::Create(
    const std::string& key_system,
    const GURL& security_origin,
    const CdmConfig& cdm_config,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb,
    const CdmCreatedCB& cdm_created_cb) {
  if (!sink_observer_->is_sink_available()) {
    CreateCdm(key_system, security_origin, cdm_config, session_message_cb,
              session_closed_cb, session_keys_change_cb,
              session_expiration_update_cb, cdm_created_cb, nullptr, false);
    return;
  }

  std::unique_ptr<RemotingCdmController> remoting_cdm_controller =
      CreateRemotingCdmController();
  // Get the raw pointer of |remoting_cdm_controller| as it will be invalidated
  // before calling |ShouldCreateRemotingCdm()| by base::Passed().
  RemotingCdmController* remoting_cdm_controller_ptr =
      remoting_cdm_controller.get();
  remoting_cdm_controller_ptr->ShouldCreateRemotingCdm(base::Bind(
      &RemotingCdmFactory::CreateCdm, weak_factory_.GetWeakPtr(), key_system,
      security_origin, cdm_config, session_message_cb, session_closed_cb,
      session_keys_change_cb, session_expiration_update_cb, cdm_created_cb,
      base::Passed(&remoting_cdm_controller)));
}

void RemotingCdmFactory::CreateCdm(
    const std::string& key_system,
    const GURL& security_origin,
    const CdmConfig& cdm_config,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb,
    const CdmCreatedCB& cdm_created_cb,
    std::unique_ptr<RemotingCdmController> remoting_cdm_controller,
    bool is_remoting) {
  if (is_remoting) {
    VLOG(1) << "Create remoting CDM.";
    // TODO(xjz): Merge this with erickung's implementation to create remoting
    // CDM.
    NOTIMPLEMENTED();
  } else {
    VLOG(1) << "Create default CDM.";
    default_cdm_factory_->Create(key_system, security_origin, cdm_config,
                                 session_message_cb, session_closed_cb,
                                 session_keys_change_cb,
                                 session_expiration_update_cb, cdm_created_cb);
  }
}

}  // namespace media
