// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FONT_SERVICE_FONT_SERVICE_APP_H_
#define COMPONENTS_FONT_SERVICE_FONT_SERVICE_APP_H_

#include <stdint.h>
#include <vector>

#include "base/macros.h"
#include "components/font_service/public/interfaces/font_service.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/tracing/public/cpp/provider.h"
#include "skia/ext/skia_utils_base.h"

namespace font_service {

class FontServiceApp
    : public service_manager::Service,
      public service_manager::InterfaceFactory<mojom::FontService>,
      public mojom::FontService {
 public:
  FontServiceApp();
  ~FontServiceApp() override;

 private:
  // service_manager::Service:
  void OnStart() override;
  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override;

  // service_manager::InterfaceFactory<mojom::FontService>:
  void Create(const service_manager::Identity& remote_identity,
              mojo::InterfaceRequest<mojom::FontService> request) override;

  // FontService:
  void MatchFamilyName(const std::string& family_name,
                       mojom::TypefaceStylePtr requested_style,
                       const MatchFamilyNameCallback& callback) override;
  void OpenStream(uint32_t id_number,
                  const OpenStreamCallback& callback) override;

  int FindOrAddPath(const SkString& path);

  mojo::BindingSet<mojom::FontService> bindings_;

  tracing::Provider tracing_;

  // We don't want to leak paths to our callers; we thus enumerate the paths of
  // fonts.
  std::vector<SkString> paths_;

  DISALLOW_COPY_AND_ASSIGN(FontServiceApp);
};

}  // namespace font_service

#endif  // COMPONENTS_FONT_SERVICE_FONT_SERVICE_APP_H_
