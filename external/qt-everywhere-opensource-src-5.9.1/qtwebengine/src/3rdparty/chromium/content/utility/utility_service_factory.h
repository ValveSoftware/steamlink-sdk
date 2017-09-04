// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_UTILITY_UTILITY_SERVICE_FACTORY_H_
#define CONTENT_UTILITY_UTILITY_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "content/child/service_factory.h"

namespace content {

// Customization of ServiceFactory for the utility process. Exposed to the
// browser via the utility process's InterfaceRegistry.
class UtilityServiceFactory : public ServiceFactory {
 public:
  UtilityServiceFactory();
  ~UtilityServiceFactory() override;

  // ServiceFactory overrides:
  void RegisterServices(ServiceMap* services) override;
  void OnServiceQuit() override;

 private:
  void OnLoadFailed() override;

  DISALLOW_COPY_AND_ASSIGN(UtilityServiceFactory);
};

}  // namespace content

#endif  // CONTENT_UTILITY_UTILITY_SERVICE_FACTORY_H_
