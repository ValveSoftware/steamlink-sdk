// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MUS_DISPLAY_LIST_H_
#define UI_VIEWS_MUS_DISPLAY_LIST_H_

#include <stdint.h>

#include <vector>

#include "base/observer_list.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/display/display.h"
#include "ui/views/mus/mus_export.h"

namespace display {
class Display;
class DisplayObserver;
}

namespace views {

// Maintains an ordered list of display::Displays as well as operations to add,
// remove and update said list. Additionally maintains display::DisplayObservers
// and updates them as appropriate.
class VIEWS_MUS_EXPORT DisplayList {
 public:
  using Displays = std::vector<display::Display>;

  enum class Type {
    PRIMARY,
    NOT_PRIMARY,
  };

  DisplayList();
  ~DisplayList();

  void AddObserver(display::DisplayObserver* observer);
  void RemoveObserver(display::DisplayObserver* observer);

  const Displays& displays() const { return displays_; }

  Displays::const_iterator FindDisplayById(int64_t id) const;
  Displays::iterator FindDisplayById(int64_t id);

  Displays::const_iterator GetPrimaryDisplayIterator() const;

  // Updates the cached id based on display.id() as well as whether the Display
  // is the primary display.
  void UpdateDisplay(const display::Display& display, Type type);

  // Adds a new Display.
  void AddDisplay(const display::Display& display, Type type);

  // Removes the Display with the specified id.
  void RemoveDisplay(int64_t id);

 private:
  std::vector<display::Display> displays_;
  int primary_display_index_ = -1;
  base::ObserverList<display::DisplayObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(DisplayList);
};

}  // namespace views

#endif  // UI_VIEWS_MUS_DISPLAY_LIST_H_
