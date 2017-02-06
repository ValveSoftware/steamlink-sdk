// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/ozone/platform_object.h"
#include "ui/ozone/platform_selection.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/ozone_switches.h"

namespace ui {

namespace {

bool g_platform_initialized_ui = false;
bool g_platform_initialized_gpu = false;

}

OzonePlatform::OzonePlatform() {
  DCHECK(!instance_) << "There should only be a single OzonePlatform.";
  instance_ = this;
  g_platform_initialized_ui = false;
  g_platform_initialized_gpu = false;
}

OzonePlatform::~OzonePlatform() {
  DCHECK_EQ(instance_, this);
  instance_ = NULL;
}

// static
void OzonePlatform::InitializeForUI() {
  const InitParams params;
  OzonePlatform::InitializeForUI(params);
}

// static
void OzonePlatform::InitializeForUI(const InitParams& args) {
  CreateInstance();
  if (g_platform_initialized_ui)
    return;
  g_platform_initialized_ui = true;
  instance_->InitializeUI(args);
  // This is deliberately created after initializing so that the platform can
  // create its own version of DDM.
  DeviceDataManager::CreateInstance();
}

// static
void OzonePlatform::InitializeForGPU() {
  const InitParams params;
  OzonePlatform::InitializeForGPU(params);
}

// static
void OzonePlatform::InitializeForGPU(const InitParams& args) {
  CreateInstance();
  if (g_platform_initialized_gpu)
    return;
  g_platform_initialized_gpu = true;
  instance_->InitializeGPU(args);
}

// static
OzonePlatform* OzonePlatform::GetInstance() {
  DCHECK(instance_) << "OzonePlatform is not initialized";
  return instance_;
}

// static
void OzonePlatform::CreateInstance() {
  if (!instance_) {
    TRACE_EVENT1("ozone",
                 "OzonePlatform::Initialize",
                 "platform",
                 GetOzonePlatformName());
    std::unique_ptr<OzonePlatform> platform =
        PlatformObject<OzonePlatform>::Create();

    // TODO(spang): Currently need to leak this object.
    OzonePlatform* pl = platform.release();
    DCHECK_EQ(instance_, pl);
  }
}

// static
OzonePlatform* OzonePlatform::instance_;

// Convenience methods to facilitate transitionning to new API.
void OzonePlatform::InitializeUI(const InitParams& args) {
  InitializeUI();
}
void OzonePlatform::InitializeGPU(const InitParams& args) {
  InitializeGPU();
}

void OzonePlatform::AddInterfaces(shell::Connection* connection) {}

}  // namespace ui
