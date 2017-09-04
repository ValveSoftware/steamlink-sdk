// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_REMOTING_CDM_FACTORY_H_
#define MEDIA_REMOTING_REMOTING_CDM_FACTORY_H_

#include "base/memory/weak_ptr.h"
#include "media/base/cdm_factory.h"
#include "media/mojo/interfaces/remoting.mojom.h"
#include "media/remoting/remoting_cdm_controller.h"
#include "media/remoting/remoting_sink_observer.h"

namespace media {

// TODO(xjz): Merge this with Eric's implementation.
class RemotingCdmFactory : public CdmFactory {
 public:
  // |remoter_factory| is expected to outlive this class.
  // |sink_observer| monitors the remoting sink availablity, which is used to
  // initialize RemotingSourceImpl when created to avoid possible delay of
  // OnSinkAvailable() call from browser.
  RemotingCdmFactory(std::unique_ptr<CdmFactory> default_cdm_factory,
                     mojom::RemoterFactory* remoter_factory,
                     std::unique_ptr<RemotingSinkObserver> sink_observer);
  ~RemotingCdmFactory() override;

  void Create(const std::string& key_system,
              const GURL& security_origin,
              const CdmConfig& cdm_config,
              const SessionMessageCB& session_message_cb,
              const SessionClosedCB& session_closed_cb,
              const SessionKeysChangeCB& session_keys_change_cb,
              const SessionExpirationUpdateCB& session_expiration_update_cb,
              const CdmCreatedCB& cdm_created_cb) override;

 private:
  std::unique_ptr<RemotingCdmController> CreateRemotingCdmController();
  void CreateCdm(const std::string& key_system,
                 const GURL& security_origin,
                 const CdmConfig& cdm_config,
                 const SessionMessageCB& session_message_cb,
                 const SessionClosedCB& session_closed_cb,
                 const SessionKeysChangeCB& session_keys_change_cb,
                 const SessionExpirationUpdateCB& session_expiration_update_cb,
                 const CdmCreatedCB& cdm_created_cb,
                 std::unique_ptr<RemotingCdmController> remoting_cdm_controller,
                 bool is_remoting);

  const std::unique_ptr<CdmFactory> default_cdm_factory_;
  mojom::RemoterFactory* const remoter_factory_;  // Outlives this class.
  std::unique_ptr<RemotingSinkObserver> sink_observer_;
  base::WeakPtrFactory<RemotingCdmFactory> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RemotingCdmFactory);
};

}  // namespace media

#endif  // MEDIA_REMOTING_REMOTING_CDM_FACTORY_H_
