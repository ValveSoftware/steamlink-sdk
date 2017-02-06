// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_cdm_factory.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/key_systems.h"
#include "media/cdm/aes_decryptor.h"
#include "media/mojo/clients/mojo_cdm.h"
#include "services/shell/public/cpp/connect.h"
#include "services/shell/public/interfaces/interface_provider.mojom.h"

namespace media {

MojoCdmFactory::MojoCdmFactory(
    shell::mojom::InterfaceProvider* interface_provider)
    : interface_provider_(interface_provider) {
  DCHECK(interface_provider_);
}

MojoCdmFactory::~MojoCdmFactory() {}

void MojoCdmFactory::Create(
    const std::string& key_system,
    const GURL& security_origin,
    const CdmConfig& cdm_config,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const LegacySessionErrorCB& legacy_session_error_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb,
    const CdmCreatedCB& cdm_created_cb) {
  DVLOG(2) << __FUNCTION__ << ": " << key_system;

  if (!security_origin.is_valid()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(cdm_created_cb, nullptr, "Invalid origin."));
    return;
  }

  if (CanUseAesDecryptor(key_system)) {
    scoped_refptr<MediaKeys> cdm(
        new AesDecryptor(security_origin, session_message_cb, session_closed_cb,
                         session_keys_change_cb));
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(cdm_created_cb, cdm, ""));
    return;
  }

  mojom::ContentDecryptionModulePtr cdm_ptr;
  shell::GetInterface<mojom::ContentDecryptionModule>(interface_provider_,
                                                      &cdm_ptr);

  MojoCdm::Create(key_system, security_origin, cdm_config, std::move(cdm_ptr),
                  session_message_cb, session_closed_cb,
                  legacy_session_error_cb, session_keys_change_cb,
                  session_expiration_update_cb, cdm_created_cb);
}

}  // namespace media
