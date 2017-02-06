// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/client_native_pixmap_factory.h"

#include "base/trace_event/trace_event.h"
#include "ui/ozone/platform_object.h"
#include "ui/ozone/platform_selection.h"

namespace ui {

namespace {

ClientNativePixmapFactory* g_instance = nullptr;

}  // namespace

// static
ClientNativePixmapFactory* ClientNativePixmapFactory::GetInstance() {
  return g_instance;
}

// static
void ClientNativePixmapFactory::SetInstance(
    ClientNativePixmapFactory* instance) {
  DCHECK(!g_instance);
  DCHECK(instance);
  g_instance = instance;
}

// static
std::unique_ptr<ClientNativePixmapFactory> ClientNativePixmapFactory::Create() {
  TRACE_EVENT1("ozone", "ClientNativePixmapFactory::Create", "platform",
               GetOzonePlatformName());
  return PlatformObject<ClientNativePixmapFactory>::Create();
}

ClientNativePixmapFactory::ClientNativePixmapFactory() {}

ClientNativePixmapFactory::~ClientNativePixmapFactory() {}

}  // namespace ui
