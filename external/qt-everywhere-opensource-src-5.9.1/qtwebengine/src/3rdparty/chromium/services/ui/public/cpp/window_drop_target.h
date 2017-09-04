// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_PUBLIC_CPP_WINDOW_DROP_TARGET_H_
#define SERVICES_UI_PUBLIC_CPP_WINDOW_DROP_TARGET_H_

#include <map>
#include <vector>

#include "ui/gfx/geometry/point.h"

namespace ui {

// Interface that clients that want to opt-in to receiving drag drop events
// pass to their ui::Window. This is the client side equivalent to the
// messages defined in ui::mojom::WindowTreeClient.
class WindowDropTarget {
 public:
  // On the first time the pointer enters the associated ui::Window, we get a
  // start message with all the data that's part of the drag. (The source
  // ui::Window receives this immediately through the client library instead
  // of asynchronously.)
  virtual void OnDragDropStart(
      std::map<std::string, std::vector<uint8_t>> mime_data) = 0;

  // Each time the pointer enters the associated ui::Window, we receive an
  // enter event and return a bitmask of drop operations that can be performed
  // at this location, in terms of the ui::mojom::kDropEffect{None,Move,
  // Copy,Link} constants.
  virtual uint32_t OnDragEnter(uint32_t event_flags,
                               const gfx::Point& position,
                               uint32_t effect_bitmask) = 0;

  // Each time the pointer moves inside the associated ui::Window, we receive
  // an over event and return a bitmask of drop oeprations.
  virtual uint32_t OnDragOver(uint32_t event_flags,
                              const gfx::Point& position,
                              uint32_t effect_bitmask) = 0;

  // Each time the pointer leaves the associated ui::Window, we receive a
  // leave event.
  virtual void OnDragLeave() = 0;

  // If the user releases the pointer over the associated ui::Window, we
  // receive this drop event to actually try to perform the drop. Unlike the
  // other methods in this class which return a value, this return value is not
  // a bitmask and is the operation which was actually completed. Returning 0
  // indicates that the drag failed and shouldn't be reported as a success.
  virtual uint32_t OnCompleteDrop(uint32_t event_flags,
                                  const gfx::Point& position,
                                  uint32_t effect_bitmask) = 0;

  // When a drag that entered the associated window finishes one way or
  // another, the target receives this message to clear the mime_data from the
  // start message, along with any other client specific caches.
  virtual void OnDragDropDone() = 0;

 protected:
  virtual ~WindowDropTarget() {}
};

}  // namespace ui

#endif  // SERVICES_UI_PUBLIC_CPP_WINDOW_DROP_TARGET_H_
