// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/metrics.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace {

// Default double click interval in milliseconds.
const int kDefaultDoubleClickInterval = 500;

}  // namespace

namespace views {

int GetDoubleClickInterval() {
#if defined(OS_WIN)
  return ::GetDoubleClickTime();
#else
  // TODO(jennyz): This value may need to be adjusted on different platforms.
  return kDefaultDoubleClickInterval;
#endif
}

int GetMenuShowDelay() {
#if defined(OS_WIN)
  static DWORD delay = 0;
  if (!delay && !SystemParametersInfo(SPI_GETMENUSHOWDELAY, 0, &delay, 0))
    delay = kDefaultMenuShowDelay;
  return delay;
#else
  return 0;
#endif
}

}  // namespace views
