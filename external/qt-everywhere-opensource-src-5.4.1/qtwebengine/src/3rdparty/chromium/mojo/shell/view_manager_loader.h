// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_VIEW_MANAGER_LOADER_H_
#define MOJO_SHELL_VIEW_MANAGER_LOADER_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "mojo/service_manager/service_loader.h"

namespace mojo {

class Application;

namespace shell {

// ServiceLoader responsible for creating connections to the ViewManager.
class ViewManagerLoader : public ServiceLoader {
 public:
  ViewManagerLoader();
  virtual ~ViewManagerLoader();

 private:
  // ServiceLoader overrides:
  virtual void LoadService(
      ServiceManager* manager,
      const GURL& url,
      ScopedMessagePipeHandle service_provider_handle) OVERRIDE;
  virtual void OnServiceError(ServiceManager* manager,
                              const GURL& url) OVERRIDE;

  ScopedVector<Application> apps_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerLoader);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_VIEW_MANAGER_LOADER_H_
