// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_CLIENTS_MOJO_CDM_FACTORY_H_
#define MEDIA_MOJO_CLIENTS_MOJO_CDM_FACTORY_H_

#include "base/macros.h"
#include "media/base/cdm_factory.h"

namespace service_manager {
namespace mojom {
class InterfaceProvider;
}
}

namespace media {

class MojoCdmFactory : public CdmFactory {
 public:
  explicit MojoCdmFactory(
      service_manager::mojom::InterfaceProvider* interface_provider);
  ~MojoCdmFactory() final;

  // CdmFactory implementation.
  void Create(const std::string& key_system,
              const GURL& security_origin,
              const CdmConfig& cdm_config,
              const SessionMessageCB& session_message_cb,
              const SessionClosedCB& session_closed_cb,
              const SessionKeysChangeCB& session_keys_change_cb,
              const SessionExpirationUpdateCB& session_expiration_update_cb,
              const CdmCreatedCB& cdm_created_cb) final;

 private:
  service_manager::mojom::InterfaceProvider* interface_provider_;

  DISALLOW_COPY_AND_ASSIGN(MojoCdmFactory);
};

}  // namespace media

#endif  // MEDIA_MOJO_CLIENTS_MOJO_CDM_FACTORY_H_
