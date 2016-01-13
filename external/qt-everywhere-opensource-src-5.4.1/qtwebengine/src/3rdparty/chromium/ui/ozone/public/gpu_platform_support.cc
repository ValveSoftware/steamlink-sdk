// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/gpu_platform_support.h"

#include "base/logging.h"
#include "ui/ozone/ozone_export.h"

namespace ui {

namespace {

// No-op implementation of GpuPlatformSupport.
class StubGpuPlatformSupport : public GpuPlatformSupport {
 public:
  // GpuPlatformSupport:
  virtual void OnChannelEstablished(IPC::Sender* sender) {}
  bool OnMessageReceived(const IPC::Message&) OVERRIDE { return false; }
};

}  // namespace

GpuPlatformSupport::GpuPlatformSupport() {
}

GpuPlatformSupport::~GpuPlatformSupport() {
}

GpuPlatformSupport* CreateStubGpuPlatformSupport() {
  return new StubGpuPlatformSupport;
}

}  // namespace ui
