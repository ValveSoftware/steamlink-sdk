// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_EVENT_FACTORY_OZONE_H_
#define UI_OZONE_PUBLIC_EVENT_FACTORY_OZONE_H_

#include <map>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_pump_libevent.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/ozone_base_export.h"

namespace gfx {
class PointF;
}

namespace ui {

class Event;

// Creates and dispatches |ui.Event|'s. Ozone assumes that events arrive on file
// descriptors with one  |EventConverterOzone| instance for each descriptor.
// Ozone presumes that the set of file descriptors can vary at runtime so this
// class supports dynamically adding and removing |EventConverterOzone|
// instances as necessary.
class OZONE_BASE_EXPORT EventFactoryOzone {
 public:
  EventFactoryOzone();
  virtual ~EventFactoryOzone();

  // Warp the cursor to a location within an AccelerateWidget.
  // If the cursor actually moves, the implementation must dispatch a mouse
  // move event with the new location.
  virtual void WarpCursorTo(gfx::AcceleratedWidget widget,
                            const gfx::PointF& location);

  // Returns the singleton instance.
  static EventFactoryOzone* GetInstance();

 private:
  static EventFactoryOzone* impl_;  // not owned

  DISALLOW_COPY_AND_ASSIGN(EventFactoryOzone);
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_EVENT_FACTORY_OZONE_H_
