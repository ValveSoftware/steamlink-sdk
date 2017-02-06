// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/example/views_examples/views_examples_application_delegate.h"

#include "services/shell/public/cpp/connection.h"
#include "services/shell/public/cpp/connector.h"
#include "ui/views/examples/example_base.h"
#include "ui/views/examples/examples_window.h"
#include "ui/views/mus/aura_init.h"
#include "ui/views/mus/window_manager_connection.h"

ViewsExamplesApplicationDelegate::ViewsExamplesApplicationDelegate() {}

ViewsExamplesApplicationDelegate::~ViewsExamplesApplicationDelegate() {
}

void ViewsExamplesApplicationDelegate::Initialize(
    shell::Connector* connector,
    const shell::Identity& identity,
    uint32_t id) {
  tracing_.Initialize(connector, identity.name());
  aura_init_.reset(new views::AuraInit(connector, "views_mus_resources.pak"));
  window_manager_connection_ =
      views::WindowManagerConnection::Create(connector, identity);
}

bool ViewsExamplesApplicationDelegate::AcceptConnection(
    shell::Connection* connection) {
  connection->AddInterface<mash::mojom::Launchable>(this);
  return true;
}

void ViewsExamplesApplicationDelegate::Launch(uint32_t what,
                                              mash::mojom::LaunchMode how) {
  views::examples::ShowExamplesWindow(views::examples::QUIT_ON_CLOSE,
                                      nullptr, nullptr);
}

void ViewsExamplesApplicationDelegate::Create(
    shell::Connection* connection,
    mash::mojom::LaunchableRequest request) {
  bindings_.AddBinding(this, std::move(request));
}
