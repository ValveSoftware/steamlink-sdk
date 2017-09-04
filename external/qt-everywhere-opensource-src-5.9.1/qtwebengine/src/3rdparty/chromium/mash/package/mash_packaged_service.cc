// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/package/mash_packaged_service.h"

#include "ash/autoclick/mus/autoclick_application.h"
#include "ash/mus/window_manager_application.h"
#include "ash/touch_hud/mus/touch_hud_application.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "mash/catalog_viewer/catalog_viewer.h"
#include "mash/quick_launch/quick_launch.h"
#include "mash/session/session.h"
#include "mash/task_viewer/task_viewer.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/ui/ime/test_ime_driver/test_ime_application.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "services/ui/service.h"

#if defined(OS_LINUX)
#include "components/font_service/font_service_app.h"
#endif

namespace mash {

MashPackagedService::MashPackagedService() {}

MashPackagedService::~MashPackagedService() {}

bool MashPackagedService::OnConnect(
    const service_manager::ServiceInfo& remote_info,
    service_manager::InterfaceRegistry* registry) {
  registry->AddInterface<ServiceFactory>(this);
  return true;
}

void MashPackagedService::Create(
    const service_manager::Identity& remote_identity,
    mojo::InterfaceRequest<ServiceFactory> request) {
  service_factory_bindings_.AddBinding(this, std::move(request));
}

void MashPackagedService::CreateService(
    service_manager::mojom::ServiceRequest request,
    const std::string& mojo_name) {
  if (context_) {
    LOG(ERROR) << "request to create additional service " << mojo_name;
    return;
  }
  std::unique_ptr<service_manager::Service> service = CreateService(mojo_name);
  if (service) {
    context_.reset(new service_manager::ServiceContext(
        std::move(service), std::move(request)));
    return;
  }
  LOG(ERROR) << "unknown name " << mojo_name;
  NOTREACHED();
}

// Please see header file for details on adding new services.
std::unique_ptr<service_manager::Service> MashPackagedService::CreateService(
    const std::string& name) {
  const std::string debugger_target =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kWaitForDebugger);
  if (!debugger_target.empty()) {
    const size_t index = name.find(':');
    if (index != std::string::npos &&
        name.substr(index + 1) == debugger_target) {
      LOG(WARNING) << "waiting for debugger to attach for service " << name;
      base::debug::WaitForDebugger(120, true);
    }
  }
  if (name == "ash")
    return base::WrapUnique(new ash::mus::WindowManagerApplication);
  if (name == "accessibility_autoclick")
    return base::WrapUnique(new ash::autoclick::AutoclickApplication);
  if (name == "catalog_viewer")
    return base::WrapUnique(new mash::catalog_viewer::CatalogViewer);
  if (name == "touch_hud")
    return base::WrapUnique(new ash::touch_hud::TouchHudApplication);
  if (name == "mash_session")
    return base::WrapUnique(new mash::session::Session);
  if (name == ui::mojom::kServiceName)
    return base::WrapUnique(new ui::Service);
  if (name == "quick_launch")
    return base::WrapUnique(new mash::quick_launch::QuickLaunch);
  if (name == "task_viewer")
    return base::WrapUnique(new mash::task_viewer::TaskViewer);
  if (name == "test_ime_driver")
    return base::WrapUnique(new ui::test::TestIMEApplication);
#if defined(OS_LINUX)
  if (name == "font_service")
    return base::WrapUnique(new font_service::FontServiceApp);
#endif
  return nullptr;
}

}  // namespace mash
