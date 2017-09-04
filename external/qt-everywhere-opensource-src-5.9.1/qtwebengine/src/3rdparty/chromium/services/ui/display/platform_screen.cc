// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/display/platform_screen.h"

#include "base/logging.h"

namespace display {

// static
PlatformScreen* PlatformScreen::instance_ = nullptr;

PlatformScreen::PlatformScreen() {
  DCHECK(!instance_);
  instance_ = this;
}

PlatformScreen::~PlatformScreen() {
  DCHECK_EQ(instance_, this);
  instance_ = nullptr;
}

// static
PlatformScreen* PlatformScreen::GetInstance() {
  DCHECK(instance_);
  return instance_;
}

}  // namespace display
