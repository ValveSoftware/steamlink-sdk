// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/frame_mojo_shell.h"

#include <utility>

#include "build/build_config.h"
#include "content/browser/mojo/mojo_shell_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/content_client.h"
#include "services/shell/public/cpp/interface_registry.h"

#if defined(OS_ANDROID) && defined(ENABLE_MOJO_CDM)
#include "content/browser/media/android/provision_fetcher_impl.h"
#endif

namespace content {

namespace {

void RegisterFrameMojoShellInterfaces(shell::InterfaceRegistry* registry,
                                      RenderFrameHost* render_frame_host) {
#if defined(OS_ANDROID) && defined(ENABLE_MOJO_CDM)
  registry->AddInterface(
      base::Bind(&ProvisionFetcherImpl::Create, render_frame_host));
#endif
}

}  // namespace

FrameMojoShell::FrameMojoShell(RenderFrameHost* frame_host)
    : frame_host_(frame_host) {
}

FrameMojoShell::~FrameMojoShell() {
}

void FrameMojoShell::BindRequest(shell::mojom::ConnectorRequest request) {
  connectors_.AddBinding(this, std::move(request));
}

// TODO(xhwang): Currently no callers are exposing |exposed_interfaces|. So we
// drop it and replace it with interfaces we provide in the browser. In the
// future we may need to support both.
void FrameMojoShell::Connect(
    shell::mojom::IdentityPtr target,
    shell::mojom::InterfaceProviderRequest interfaces,
    shell::mojom::InterfaceProviderPtr /* exposed_interfaces */,
    shell::mojom::ClientProcessConnectionPtr client_process_connection,
    const shell::mojom::Connector::ConnectCallback& callback) {
  shell::mojom::InterfaceProviderPtr frame_interfaces;
  interface_provider_bindings_.AddBinding(GetInterfaceRegistry(),
                                          GetProxy(&frame_interfaces));
  MojoShellContext::ConnectToApplication(
      shell::mojom::kRootUserID, target->name,
      frame_host_->GetSiteInstance()->GetSiteURL().spec(),
      std::move(interfaces),
      std::move(frame_interfaces), callback);
}

void FrameMojoShell::Clone(shell::mojom::ConnectorRequest request) {
  connectors_.AddBinding(this, std::move(request));
}

shell::InterfaceRegistry* FrameMojoShell::GetInterfaceRegistry() {
  if (!interface_registry_) {
    interface_registry_.reset(new shell::InterfaceRegistry(nullptr));

    // TODO(rockot/xhwang): Currently all applications connected share the same
    // set of interfaces registered in the |registry|. We may want to provide
    // different interfaces for different apps for better isolation.
    RegisterFrameMojoShellInterfaces(interface_registry_.get(), frame_host_);
    GetContentClient()->browser()->RegisterFrameMojoShellInterfaces(
        interface_registry_.get(), frame_host_);
  }

  return interface_registry_.get();
}

}  // namespace content
