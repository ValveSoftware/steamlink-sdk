// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_GPU_SERVICE_FACTORY_H_
#define CONTENT_GPU_GPU_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "content/child/service_factory.h"

namespace content {

// Customization of ServiceFactory for the GPU process.
class GpuServiceFactory : public ServiceFactory {
 public:
  GpuServiceFactory();
  ~GpuServiceFactory() override;

  // ServiceFactory overrides:
  void RegisterServices(ServiceMap* services) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(GpuServiceFactory);
};

}  // namespace content

#endif  // CONTENT_GPU_GPU_SERVICE_FACTORY_H_
