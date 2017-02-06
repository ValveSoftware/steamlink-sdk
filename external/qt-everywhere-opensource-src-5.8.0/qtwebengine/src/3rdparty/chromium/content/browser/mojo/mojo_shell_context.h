// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MOJO_MOJO_SHELL_CONTEXT_H_
#define CONTENT_BROWSER_MOJO_MOJO_SHELL_CONTEXT_H_

#include <map>
#include <memory>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "services/shell/public/interfaces/connector.mojom.h"
#include "services/shell/shell.h"

namespace catalog {
class Catalog;
}

namespace shell {
class ShellClient;
}

namespace content {

// MojoShellContext hosts the browser's ApplicationManager, coordinating
// app registration and interconnection.
class CONTENT_EXPORT MojoShellContext {
 public:
  MojoShellContext();
  ~MojoShellContext();

  // Connects an application at |name| and gets a handle to its exposed
  // services. This is only intended for use in browser code that's not part of
  // some Mojo application. May be called from any thread. |requestor_name| is
  // given to the target application as the requestor's name upon connection.
  static void ConnectToApplication(
      const std::string& user_id,
      const std::string& name,
      const std::string& requestor_name,
      shell::mojom::InterfaceProviderRequest request,
      shell::mojom::InterfaceProviderPtr exposed_services,
      const shell::mojom::Connector::ConnectCallback& callback);

  // Returns a shell::Connector that can be used on the IO thread.
  static shell::Connector* GetConnectorForIOThread();

 private:
  class BuiltinManifestProvider;
  class Proxy;
  friend class Proxy;

  void ConnectToApplicationOnOwnThread(
      const std::string& user_id,
      const std::string& name,
      const std::string& requestor_name,
      shell::mojom::InterfaceProviderRequest request,
      shell::mojom::InterfaceProviderPtr exposed_services,
      const shell::mojom::Connector::ConnectCallback& callback);

  static base::LazyInstance<std::unique_ptr<Proxy>> proxy_;

  std::unique_ptr<BuiltinManifestProvider> manifest_provider_;
  std::unique_ptr<catalog::Catalog> catalog_;
  std::unique_ptr<shell::Shell> shell_;

  DISALLOW_COPY_AND_ASSIGN(MojoShellContext);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MOJO_MOJO_SHELL_CONTEXT_H_
