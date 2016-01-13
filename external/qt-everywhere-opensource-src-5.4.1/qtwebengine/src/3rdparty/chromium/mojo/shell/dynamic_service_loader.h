// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_DYNAMIC_SERVICE_LOADER_H_
#define MOJO_SHELL_DYNAMIC_SERVICE_LOADER_H_

#include <map>

#include "base/macros.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/service_manager/service_loader.h"
#include "mojo/services/public/interfaces/network/network_service.mojom.h"
#include "mojo/shell/dynamic_service_runner.h"
#include "mojo/shell/keep_alive.h"
#include "url/gurl.h"

namespace mojo {
namespace shell {

class Context;
class DynamicServiceRunnerFactory;

// An implementation of ServiceLoader that retrieves a dynamic library
// containing the implementation of the service and loads/runs it (via a
// DynamicServiceRunner).
class DynamicServiceLoader : public ServiceLoader {
 public:
  DynamicServiceLoader(Context* context,
                       scoped_ptr<DynamicServiceRunnerFactory> runner_factory);
  virtual ~DynamicServiceLoader();

  // ServiceLoader methods:
  virtual void LoadService(ServiceManager* manager,
                           const GURL& url,
                           ScopedMessagePipeHandle service_handle) OVERRIDE;
  virtual void OnServiceError(ServiceManager* manager, const GURL& url)
      OVERRIDE;

 private:
  Context* const context_;
  scoped_ptr<DynamicServiceRunnerFactory> runner_factory_;
  NetworkServicePtr network_service_;

  DISALLOW_COPY_AND_ASSIGN(DynamicServiceLoader);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_DYNAMIC_SERVICE_LOADER_H_
