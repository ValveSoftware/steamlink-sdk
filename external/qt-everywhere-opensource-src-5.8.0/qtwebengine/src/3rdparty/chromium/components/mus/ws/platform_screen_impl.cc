// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/platform_screen_impl.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/gfx/geometry/rect.h"

namespace mus {

namespace ws {
namespace {

void FixedSizeScreenConfiguration(
    const PlatformScreen::ConfiguredDisplayCallback& callback) {
  callback.Run(1, gfx::Rect(1024, 768));
}

}  // namespace

// static
std::unique_ptr<PlatformScreen> PlatformScreen::Create() {
  return base::WrapUnique(new PlatformScreenImpl);
}

PlatformScreenImpl::PlatformScreenImpl() {}

PlatformScreenImpl::~PlatformScreenImpl() {}

void PlatformScreenImpl::Init() {}

void PlatformScreenImpl::ConfigurePhysicalDisplay(
    const PlatformScreen::ConfiguredDisplayCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&FixedSizeScreenConfiguration, callback));
}

}  // namespace ws
}  // namespace mus
