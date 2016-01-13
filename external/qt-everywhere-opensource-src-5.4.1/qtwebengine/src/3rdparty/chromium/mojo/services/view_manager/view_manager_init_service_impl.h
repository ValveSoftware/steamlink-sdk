// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_VIEW_MANAGER_INIT_SERVICE_IMPL_H_
#define MOJO_SERVICES_VIEW_MANAGER_VIEW_MANAGER_INIT_SERVICE_IMPL_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "mojo/services/public/interfaces/view_manager/view_manager.mojom.h"
#include "mojo/services/view_manager/root_node_manager.h"
#include "mojo/services/view_manager/root_view_manager_delegate.h"
#include "mojo/services/view_manager/view_manager_export.h"

namespace mojo {

class ServiceProvider;

namespace view_manager {
namespace service {

#if defined(OS_WIN)
// Equivalent of NON_EXPORTED_BASE which does not work with the template snafu
// below.
#pragma warning(push)
#pragma warning(disable : 4275)
#endif

// Used to create the initial ViewManagerClient. Doesn't initiate the Connect()
// until the WindowTreeHost has been created.
class MOJO_VIEW_MANAGER_EXPORT ViewManagerInitServiceImpl
    : public InterfaceImpl<ViewManagerInitService>,
      public RootViewManagerDelegate {
 public:
  explicit ViewManagerInitServiceImpl(ServiceProvider* service_provider);
  virtual ~ViewManagerInitServiceImpl();

 private:
  struct ConnectParams {
    ConnectParams();
    ~ConnectParams();

    std::string url;
    Callback<void(bool)> callback;
  };

  void MaybeEmbedRoot(const std::string& url,
                      const Callback<void(bool)>& callback);

  // ViewManagerInitService overrides:
  virtual void EmbedRoot(const String& url,
                         const Callback<void(bool)>& callback) OVERRIDE;

  // RootViewManagerDelegate overrides:
  virtual void OnRootViewManagerWindowTreeHostCreated() OVERRIDE;

  ServiceProvider* service_provider_;

  RootNodeManager root_node_manager_;

  // Parameters passed to Connect(). If non-null Connect() has been invoked.
  scoped_ptr<ConnectParams> connect_params_;

  bool is_tree_host_ready_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerInitServiceImpl);
};

#if defined(OS_WIN)
#pragma warning(pop)
#endif

}  // namespace service
}  // namespace view_manager
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_VIEW_MANAGER_INIT_SERVICE_IMPL_H_
