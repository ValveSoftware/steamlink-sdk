// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IMAGE_DECODER_IMAGE_DECODER_SERVICE_H_
#define SERVICES_IMAGE_DECODER_IMAGE_DECODER_SERVICE_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context_ref.h"

namespace image_decoder {

class ImageDecoderService : public service_manager::Service {
 public:
  ImageDecoderService();
  ~ImageDecoderService() override;

  // Factory function for use as an embedded service.
  static std::unique_ptr<service_manager::Service> Create();

  // service_manager::Service:
  void OnStart() override;
  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override;
  bool OnStop() override;

 private:
  std::unique_ptr<service_manager::ServiceContextRefFactory> ref_factory_;

  DISALLOW_COPY_AND_ASSIGN(ImageDecoderService);
};

}  // namespace image_decoder

#endif  // SERVICES_IMAGE_DECODER_IMAGE_DECODER_SERVICE_H_
