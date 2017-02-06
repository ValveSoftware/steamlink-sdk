// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/lib/in_flight_change.h"

#include "components/mus/public/cpp/lib/window_private.h"
#include "components/mus/public/cpp/window_tree_client.h"

namespace mus {

// InFlightChange -------------------------------------------------------------

InFlightChange::InFlightChange(Window* window, ChangeType type)
    : window_(window), change_type_(type) {}

InFlightChange::~InFlightChange() {}

bool InFlightChange::Matches(const InFlightChange& change) const {
  DCHECK(change.window_ == window_ && change.change_type_ == change_type_);
  return true;
}

void InFlightChange::ChangeFailed() {}

// InFlightBoundsChange -------------------------------------------------------

InFlightBoundsChange::InFlightBoundsChange(Window* window,
                                           const gfx::Rect& revert_bounds)
    : InFlightChange(window, ChangeType::BOUNDS),
      revert_bounds_(revert_bounds) {}

void InFlightBoundsChange::SetRevertValueFrom(const InFlightChange& change) {
  revert_bounds_ =
      static_cast<const InFlightBoundsChange&>(change).revert_bounds_;
}

void InFlightBoundsChange::Revert() {
  WindowPrivate(window()).LocalSetBounds(window()->bounds(), revert_bounds_);
}

// CrashInFlightChange --------------------------------------------------------

CrashInFlightChange::CrashInFlightChange(Window* window, ChangeType type)
    : InFlightChange(window, type) {}

CrashInFlightChange::~CrashInFlightChange() {}

void CrashInFlightChange::SetRevertValueFrom(const InFlightChange& change) {
  CHECK(false);
}

void CrashInFlightChange::ChangeFailed() {
  DLOG(ERROR) << "changed failed, type=" << static_cast<int>(change_type());
  CHECK(false);
}

void CrashInFlightChange::Revert() {
  CHECK(false);
}

// InFlightWindowChange -------------------------------------------------------

InFlightWindowTreeClientChange::InFlightWindowTreeClientChange(
    WindowTreeClient* client,
    Window* revert_value,
    ChangeType type)
    : InFlightChange(nullptr, type),
      client_(client),
      revert_window_(nullptr) {
  SetRevertWindow(revert_value);
}

InFlightWindowTreeClientChange::~InFlightWindowTreeClientChange() {
  SetRevertWindow(nullptr);
}

void InFlightWindowTreeClientChange::SetRevertValueFrom(
    const InFlightChange& change) {
  SetRevertWindow(static_cast<const InFlightWindowTreeClientChange&>(change)
                      .revert_window_);
}

void InFlightWindowTreeClientChange::SetRevertWindow(Window* window) {
  if (revert_window_)
    revert_window_->RemoveObserver(this);
  revert_window_ = window;
  if (revert_window_)
    revert_window_->AddObserver(this);
}

void InFlightWindowTreeClientChange::OnWindowDestroying(Window* window) {
  SetRevertWindow(nullptr);
}

// InFlightCaptureChange ------------------------------------------------------

InFlightCaptureChange::InFlightCaptureChange(
    WindowTreeClient* client, Window* revert_value)
    : InFlightWindowTreeClientChange(client,
                                     revert_value,
                                     ChangeType::CAPTURE) {}

InFlightCaptureChange::~InFlightCaptureChange() {}

void InFlightCaptureChange::Revert() {
  client()->LocalSetCapture(revert_window());
}

// InFlightFocusChange --------------------------------------------------------

InFlightFocusChange::InFlightFocusChange(
    WindowTreeClient* client,
    Window* revert_value)
    : InFlightWindowTreeClientChange(client,
                                     revert_value,
                                     ChangeType::FOCUS) {}

InFlightFocusChange::~InFlightFocusChange() {}

void InFlightFocusChange::Revert() {
  client()->LocalSetFocus(revert_window());
}

// InFlightPropertyChange -----------------------------------------------------

InFlightPropertyChange::InFlightPropertyChange(
    Window* window,
    const std::string& property_name,
    const mojo::Array<uint8_t>& revert_value)
    : InFlightChange(window, ChangeType::PROPERTY),
      property_name_(property_name),
      revert_value_(revert_value.Clone()) {}

InFlightPropertyChange::~InFlightPropertyChange() {}

bool InFlightPropertyChange::Matches(const InFlightChange& change) const {
  return static_cast<const InFlightPropertyChange&>(change).property_name_ ==
         property_name_;
}

void InFlightPropertyChange::SetRevertValueFrom(const InFlightChange& change) {
  revert_value_ =
      static_cast<const InFlightPropertyChange&>(change).revert_value_.Clone();
}

void InFlightPropertyChange::Revert() {
  WindowPrivate(window())
      .LocalSetSharedProperty(property_name_, std::move(revert_value_));
}

// InFlightPredefinedCursorChange ---------------------------------------------

InFlightPredefinedCursorChange::InFlightPredefinedCursorChange(
    Window* window,
    mojom::Cursor revert_value)
    : InFlightChange(window, ChangeType::PREDEFINED_CURSOR),
      revert_cursor_(revert_value) {}

InFlightPredefinedCursorChange::~InFlightPredefinedCursorChange() {}

void InFlightPredefinedCursorChange::SetRevertValueFrom(
    const InFlightChange& change) {
  revert_cursor_ =
      static_cast<const InFlightPredefinedCursorChange&>(change).revert_cursor_;
}

void InFlightPredefinedCursorChange::Revert() {
  WindowPrivate(window()).LocalSetPredefinedCursor(revert_cursor_);
}

// InFlightVisibleChange -------------------------------------------------------

InFlightVisibleChange::InFlightVisibleChange(Window* window,
                                             bool revert_value)
    : InFlightChange(window, ChangeType::VISIBLE),
      revert_visible_(revert_value) {}

InFlightVisibleChange::~InFlightVisibleChange() {}

void InFlightVisibleChange::SetRevertValueFrom(const InFlightChange& change) {
  revert_visible_ =
      static_cast<const InFlightVisibleChange&>(change).revert_visible_;
}

void InFlightVisibleChange::Revert() {
  WindowPrivate(window()).LocalSetVisible(revert_visible_);
}

// InFlightOpacityChange -------------------------------------------------------

InFlightOpacityChange::InFlightOpacityChange(Window* window, float revert_value)
    : InFlightChange(window, ChangeType::OPACITY),
      revert_opacity_(revert_value) {}

InFlightOpacityChange::~InFlightOpacityChange() {}

void InFlightOpacityChange::SetRevertValueFrom(const InFlightChange& change) {
  revert_opacity_ =
      static_cast<const InFlightOpacityChange&>(change).revert_opacity_;
}

void InFlightOpacityChange::Revert() {
  WindowPrivate(window()).LocalSetOpacity(revert_opacity_);
}

// InFlightSetModalChange ------------------------------------------------------

InFlightSetModalChange::InFlightSetModalChange(Window* window)
    : InFlightChange(window, ChangeType::SET_MODAL) {}

InFlightSetModalChange::~InFlightSetModalChange() {}

void InFlightSetModalChange::SetRevertValueFrom(const InFlightChange& change) {}

void InFlightSetModalChange::Revert() {
  WindowPrivate(window()).LocalUnsetModal();
}

}  // namespace mus
