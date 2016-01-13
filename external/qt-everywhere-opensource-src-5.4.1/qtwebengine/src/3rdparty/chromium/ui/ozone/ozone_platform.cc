// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "ui/ozone/ozone_platform.h"
#include "ui/ozone/ozone_switches.h"
#include "ui/ozone/platform_object.h"
#include "ui/ozone/platform_selection.h"

namespace ui {

namespace {

bool g_platform_initialized_ui = false;
bool g_platform_initialized_gpu = false;

}

OzonePlatform::OzonePlatform() {
  CHECK(!instance_) << "There should only be a single OzonePlatform.";
  instance_ = this;
  g_platform_initialized_ui = false;
  g_platform_initialized_gpu = false;
}

OzonePlatform::~OzonePlatform() {
  CHECK_EQ(instance_, this);
  instance_ = NULL;
}

// static
void OzonePlatform::InitializeForUI() {
  CreateInstance();
  if (g_platform_initialized_ui)
    return;
  g_platform_initialized_ui = true;
  instance_->InitializeUI();
}

// static
void OzonePlatform::InitializeForGPU() {
  CreateInstance();
  if (g_platform_initialized_gpu)
    return;
  g_platform_initialized_gpu = true;
  instance_->InitializeGPU();
}

// static
OzonePlatform* OzonePlatform::GetInstance() {
  CHECK(instance_) << "OzonePlatform is not initialized";
  return instance_;
}

// static
void OzonePlatform::CreateInstance() {
  if (!instance_) {
    TRACE_EVENT1("ozone",
                 "OzonePlatform::Initialize",
                 "platform",
                 GetOzonePlatformName());
    scoped_ptr<OzonePlatform> platform =
        PlatformObject<OzonePlatform>::Create();

    // TODO(spang): Currently need to leak this object.
    CHECK_EQ(instance_, platform.release());
  }
}

// static
OzonePlatform* OzonePlatform::instance_;

}  // namespace ui
