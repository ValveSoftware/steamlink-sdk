// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_PUBLIC_CPP_WINDOW_TRACKER_H_
#define SERVICES_UI_PUBLIC_CPP_WINDOW_TRACKER_H_

#include <stdint.h>
#include <set>

#include "base/macros.h"
#include "services/ui/public/cpp/window_observer.h"
#include "ui/base/window_tracker_template.h"

namespace ui {

using WindowTracker = ui::WindowTrackerTemplate<Window, WindowObserver>;

}  // namespace ui

#endif  // SERVICES_UI_PUBLIC_CPP_WINDOW_TRACKER_H_
