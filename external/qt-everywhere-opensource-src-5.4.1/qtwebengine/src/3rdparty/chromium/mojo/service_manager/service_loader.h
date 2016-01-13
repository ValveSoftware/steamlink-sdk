// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICE_MANAGER_SERVICE_LOADER_H_
#define MOJO_SERVICE_MANAGER_SERVICE_LOADER_H_

#include "mojo/public/cpp/system/core.h"
#include "mojo/service_manager/service_manager_export.h"
#include "url/gurl.h"

namespace mojo {

class ServiceManager;

// Interface to allowing default loading behavior to be overridden for a
// specific url.
class MOJO_SERVICE_MANAGER_EXPORT ServiceLoader {
 public:
  virtual ~ServiceLoader() {}
  virtual void LoadService(ServiceManager* manager,
                           const GURL& url,
                           ScopedMessagePipeHandle service_handle) = 0;
  virtual void OnServiceError(ServiceManager* manager, const GURL& url) = 0;

 protected:
  ServiceLoader() {}
};

}  // namespace mojo

#endif  // MOJO_SERVICE_MANAGER_SERVICE_LOADER_H_
