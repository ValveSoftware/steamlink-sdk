// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/app_driver/app_driver.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "components/mus/common/event_matcher_util.h"
#include "mash/public/interfaces/launchable.mojom.h"
#include "services/shell/public/cpp/connection.h"
#include "services/shell/public/cpp/connector.h"

using mash::mojom::LaunchablePtr;
using mash::mojom::LaunchMode;

namespace mash {
namespace app_driver {
namespace {

enum class Accelerator : uint32_t {
  NewChromeWindow,
  NewChromeTab,
  NewChromeIncognitoWindow,
  ShowTaskManager,
};

struct AcceleratorSpec {
  Accelerator id;
  ui::mojom::KeyboardCode keyboard_code;
  // A bitfield of kEventFlag* and kMouseEventFlag* values in
  // input_event_constants.mojom.
  int event_flags;
};

AcceleratorSpec g_spec[] = {
    {Accelerator::NewChromeWindow, ui::mojom::KeyboardCode::N,
     ui::mojom::kEventFlagControlDown},
    {Accelerator::NewChromeTab, ui::mojom::KeyboardCode::T,
     ui::mojom::kEventFlagControlDown},
    {Accelerator::NewChromeIncognitoWindow, ui::mojom::KeyboardCode::N,
     ui::mojom::kEventFlagControlDown | ui::mojom::kEventFlagShiftDown},
    {Accelerator::ShowTaskManager, ui::mojom::KeyboardCode::ESCAPE,
     ui::mojom::kEventFlagShiftDown},
};

void AssertTrue(bool success) {
  DCHECK(success);
}

void DoNothing() {}

}  // namespace

AppDriver::AppDriver()
    : connector_(nullptr), binding_(this), weak_factory_(this) {}

AppDriver::~AppDriver() {}

void AppDriver::OnAvailableCatalogEntries(
    mojo::Array<catalog::mojom::EntryPtr> entries) {
  if (entries.empty()) {
    LOG(ERROR) << "Unable to install accelerators for launching chrome.";
    return;
  }

  mus::mojom::AcceleratorRegistrarPtr registrar;
  connector_->ConnectToInterface(entries[0]->name, &registrar);

  if (binding_.is_bound())
    binding_.Unbind();
  registrar->SetHandler(binding_.CreateInterfacePtrAndBind());
  // If the window manager restarts, the handler pipe will close and we'll need
  // to re-add our accelerators when the window manager comes back up.
  binding_.set_connection_error_handler(
      base::Bind(&AppDriver::AddAccelerators, weak_factory_.GetWeakPtr()));

  for (const AcceleratorSpec& spec : g_spec) {
    registrar->AddAccelerator(
        static_cast<uint32_t>(spec.id),
        mus::CreateKeyMatcher(spec.keyboard_code, spec.event_flags),
        base::Bind(&AssertTrue));
  }
}

void AppDriver::Initialize(shell::Connector* connector,
                           const shell::Identity& identity,
                           uint32_t id) {
  connector_ = connector;
  AddAccelerators();
}

bool AppDriver::AcceptConnection(shell::Connection* connection) {
  return true;
}

bool AppDriver::ShellConnectionLost() {
  // Prevent the code in AddAccelerators() from keeping this app alive.
  binding_.set_connection_error_handler(base::Bind(&DoNothing));
  return true;
}

void AppDriver::OnAccelerator(uint32_t id, std::unique_ptr<ui::Event> event) {
  struct LaunchOptions {
    uint32_t option;
    const char* app;
    LaunchMode mode;
  };

  std::map<Accelerator, LaunchOptions> options{
      {Accelerator::NewChromeWindow,
       {mojom::kWindow, "exe:chrome", LaunchMode::MAKE_NEW}},
      {Accelerator::NewChromeTab,
       {mojom::kDocument, "exe:chrome", LaunchMode::MAKE_NEW}},
      {Accelerator::NewChromeIncognitoWindow,
       {mojom::kIncognitoWindow, "exe:chrome", LaunchMode::MAKE_NEW}},
      {Accelerator::ShowTaskManager,
       {mojom::kWindow, "mojo:task_viewer", LaunchMode::DEFAULT}},
  };

  const auto iter = options.find(static_cast<Accelerator>(id));
  DCHECK(iter != options.end());
  const LaunchOptions& entry = iter->second;
  LaunchablePtr launchable;
  connector_->ConnectToInterface(entry.app, &launchable);
  launchable->Launch(entry.option, entry.mode);
}

void AppDriver::AddAccelerators() {
  connector_->ConnectToInterface("mojo:catalog", &catalog_);
  catalog_->GetEntriesProvidingClass(
      "mus:window_manager", base::Bind(&AppDriver::OnAvailableCatalogEntries,
                                       weak_factory_.GetWeakPtr()));
}

}  // namespace app_driver
}  // namespace mash
