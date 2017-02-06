// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_CACA_CACA_EVENT_SOURCE_H_
#define UI_OZONE_PLATFORM_CACA_CACA_EVENT_SOURCE_H_

#include <caca.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/platform/scoped_event_dispatcher.h"
#include "ui/gfx/geometry/point_f.h"

namespace ui {

class CacaWindow;

class CacaEventSource : public PlatformEventSource {
 public:
  CacaEventSource();
  ~CacaEventSource() override;

  // Poll for an event on a particular window. Input events will be
  // dispatched on the given dispatcher.
  void TryProcessingEvent(CacaWindow* window);

  // Process an input event on a particular window.
  void OnInputEvent(caca_event_t* event, CacaWindow* window);

 private:
  // Keep track of last cursor position to dispatch correct mouse push/release
  // events.
  gfx::PointF last_cursor_location_;

  int modifier_flags_ = 0;

  DISALLOW_COPY_AND_ASSIGN(CacaEventSource);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_CACA_CACA_EVENT_SOURCE_H_
