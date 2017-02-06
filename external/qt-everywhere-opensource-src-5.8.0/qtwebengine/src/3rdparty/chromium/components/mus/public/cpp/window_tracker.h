// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_WINDOW_TRACKER_H_
#define COMPONENTS_MUS_PUBLIC_CPP_WINDOW_TRACKER_H_

#include <stdint.h>
#include <set>

#include "base/macros.h"
#include "components/mus/public/cpp/window_observer.h"
#include "ui/base/window_tracker_template.h"

namespace mus {

using WindowTracker = ui::WindowTrackerTemplate<Window, WindowObserver>;

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_WINDOW_TRACKER_H_
