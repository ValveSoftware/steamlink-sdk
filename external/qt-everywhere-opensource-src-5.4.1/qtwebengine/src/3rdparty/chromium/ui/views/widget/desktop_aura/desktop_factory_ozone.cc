// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_factory_ozone.h"

#include "base/logging.h"

namespace views {

// static
DesktopFactoryOzone* DesktopFactoryOzone::impl_ = NULL;

DesktopFactoryOzone::DesktopFactoryOzone() {
}

DesktopFactoryOzone::~DesktopFactoryOzone() {
}

DesktopFactoryOzone* DesktopFactoryOzone::GetInstance() {
  CHECK(impl_) << "DesktopFactoryOzone accessed before constructed";
  return impl_;
}

void DesktopFactoryOzone::SetInstance(DesktopFactoryOzone* impl) {
  impl_ = impl;
}

} // namespace views
