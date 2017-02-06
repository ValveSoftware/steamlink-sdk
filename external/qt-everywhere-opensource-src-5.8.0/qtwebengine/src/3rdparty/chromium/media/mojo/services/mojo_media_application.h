// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_MEDIA_APPLICATION_H_
#define MEDIA_MOJO_SERVICES_MOJO_MEDIA_APPLICATION_H_

#include <stdint.h>

#include <memory>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "media/mojo/interfaces/service_factory.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "services/shell/public/cpp/interface_factory.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/shell/public/cpp/shell_connection_ref.h"
#include "url/gurl.h"

namespace media {

class MediaLog;
class MojoMediaClient;

class MEDIA_MOJO_EXPORT MojoMediaApplication
    : public NON_EXPORTED_BASE(shell::ShellClient),
      public NON_EXPORTED_BASE(shell::InterfaceFactory<mojom::ServiceFactory>) {
 public:
  MojoMediaApplication(std::unique_ptr<MojoMediaClient> mojo_media_client,
                       const base::Closure& quit_closure);
  ~MojoMediaApplication() final;

 private:
  // shell::ShellClient implementation.
  void Initialize(shell::Connector* connector,
                  const shell::Identity& identity,
                  uint32_t id) final;
  bool AcceptConnection(shell::Connection* connection) final;
  bool ShellConnectionLost() final;

  // shell::InterfaceFactory<mojom::ServiceFactory> implementation.
  void Create(shell::Connection* connection,
              mojo::InterfaceRequest<mojom::ServiceFactory> request) final;

  // Note: Since each instance runs on a different thread, do not share a common
  // MojoMediaClient with other instances to avoid threading issues. Hence using
  // a unique_ptr here.
  std::unique_ptr<MojoMediaClient> mojo_media_client_;

  shell::Connector* connector_;
  scoped_refptr<MediaLog> media_log_;
  shell::ShellConnectionRefFactory ref_factory_;
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_MEDIA_APPLICATION_H_
