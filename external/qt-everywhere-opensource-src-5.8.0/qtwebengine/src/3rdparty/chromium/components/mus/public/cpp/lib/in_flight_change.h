// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_LIB_IN_FLIGHT_CHANGE_H_
#define COMPONENTS_MUS_PUBLIC_CPP_LIB_IN_FLIGHT_CHANGE_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "components/mus/public/cpp/window_observer.h"
#include "mojo/public/cpp/bindings/array.h"
#include "ui/gfx/geometry/rect.h"

namespace mus {

namespace mojom {
enum class Cursor : int32_t;
}

class Window;
class WindowTreeClient;

enum class ChangeType {
  ADD_CHILD,
  ADD_TRANSIENT_WINDOW,
  BOUNDS,
  CAPTURE,
  DELETE_WINDOW,
  FOCUS,
  NEW_WINDOW,
  NEW_TOP_LEVEL_WINDOW,
  OPACITY,
  PREDEFINED_CURSOR,
  PROPERTY,
  REMOVE_CHILD,
  REMOVE_TRANSIENT_WINDOW_FROM_PARENT,
  REORDER,
  SET_MODAL,
  VISIBLE,
};

// InFlightChange is used to track function calls to the server and take the
// appropriate action when the call fails, or the same property changes while
// waiting for the response. When a function is called on the server an
// InFlightChange is created. The function call is complete when
// OnChangeCompleted() is received from the server. The following may occur
// while waiting for a response:
// . A new value is encountered from the server. For example, the bounds of
//   a window is locally changed and while waiting for the ack
//   OnWindowBoundsChanged() is received.
//   When this happens SetRevertValueFrom() is invoked on the InFlightChange.
//   The expectation is SetRevertValueFrom() takes the value to revert from the
//   supplied change.
// . While waiting for the ack the property is again modified locally. When
//   this happens a new InFlightChange is created. Once the ack for the first
//   call is received, and the server rejected the change ChangeFailed() is
//   invoked on the first change followed by SetRevertValueFrom() on the second
//   InFlightChange. This allows the new change to update the value to revert
//   should the second call fail.
// . If the server responds that the call failed and there is not another
//   InFlightChange for the same window outstanding, then ChangeFailed() is
//   invoked followed by Revert(). The expectation is Revert() sets the
//   appropriate value back on the Window.
//
// In general there are two classes of changes:
// 1. We are the only side allowed to make the change.
// 2. The change can also be applied by another client. For example, the
//    window manager may change the bounds as well as the local client.
//
// For (1) use CrashInFlightChange. As the name implies this change CHECKs that
// the change succeeded. Use the following pattern for this. This code goes
// where the change is sent to the server (in WindowTreeClient):
//   const uint32_t change_id =
//   ScheduleInFlightChange(base::WrapUnique(new CrashInFlightChange(
//       window, ChangeType::REORDER)));
//
// For (2) use the same pattern as (1), but in the on change callback from the
// server (e.g. OnWindowBoundsChanged()) add the following:
//   // value_from_server is the value supplied from the server. It corresponds
//   // to the value of the property at the time the server processed the
//   // change. If the local change fails, this is the value reverted to.
//   InFlightBoundsChange new_change(window, value_from_server);
//   if (ApplyServerChangeToExistingInFlightChange(new_change)) {
//     // There was an in flight change for the same property. The in flight
//     // change takes the value to revert from |new_change|.
//     return;
//   }
//
//   // else case is no flight in change and the new value can be applied
//   // immediately.
//   WindowPrivate(window).LocalSetValue(new_value_from_server);
//
class InFlightChange {
 public:
  InFlightChange(Window* window, ChangeType type);
  virtual ~InFlightChange();

  // NOTE: for properties not associated with any window window is null.
  Window* window() { return window_; }
  const Window* window() const { return window_; }
  ChangeType change_type() const { return change_type_; }

  // Returns true if the two changes are considered the same. This is only
  // invoked if the window id and ChangeType match.
  virtual bool Matches(const InFlightChange& change) const;

  // Called in two cases:
  // . When the corresponding property on the server changes. In this case
  //   |change| corresponds to the value from the server.
  // . When |change| unsuccesfully completes.
  virtual void SetRevertValueFrom(const InFlightChange& change) = 0;

  // The change failed. Normally you need not take any action here as Revert()
  // is called as appropriate.
  virtual void ChangeFailed();

  // The change failed and there is no pending change of the same type
  // outstanding, revert the value.
  virtual void Revert() = 0;

 private:
  Window* window_;
  const ChangeType change_type_;
};

class InFlightBoundsChange : public InFlightChange {
 public:
  InFlightBoundsChange(Window* window, const gfx::Rect& revert_bounds);

  // InFlightChange:
  void SetRevertValueFrom(const InFlightChange& change) override;
  void Revert() override;

 private:
  gfx::Rect revert_bounds_;

  DISALLOW_COPY_AND_ASSIGN(InFlightBoundsChange);
};

// Inflight change that crashes on failure. This is useful for changes that are
// expected to always complete.
class CrashInFlightChange : public InFlightChange {
 public:
  CrashInFlightChange(Window* window, ChangeType type);
  ~CrashInFlightChange() override;

  // InFlightChange:
  void SetRevertValueFrom(const InFlightChange& change) override;
  void ChangeFailed() override;
  void Revert() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CrashInFlightChange);
};

// Use this class for properties that are specific to the client, and not a
// particular window. For example, only a single window can have focus, so focus
// is specific to the client.
//
// This does not implement InFlightChange::Revert, subclasses must implement
// that to update the WindowTreeClient.
class InFlightWindowTreeClientChange : public InFlightChange,
                                       public WindowObserver {
 public:
  InFlightWindowTreeClientChange(WindowTreeClient* client,
                                 Window* revert_value,
                                 ChangeType type);
  ~InFlightWindowTreeClientChange() override;

  // InFlightChange:
  void SetRevertValueFrom(const InFlightChange& change) override;

 protected:
  WindowTreeClient* client() { return client_; }
  Window* revert_window() { return revert_window_; }

 private:
  void SetRevertWindow(Window* window);

  // WindowObserver:
  void OnWindowDestroying(Window* window) override;

  WindowTreeClient* client_;
  Window* revert_window_;

  DISALLOW_COPY_AND_ASSIGN(InFlightWindowTreeClientChange);
};

class InFlightCaptureChange : public InFlightWindowTreeClientChange {
 public:
  InFlightCaptureChange(WindowTreeClient* client, Window* revert_value);
  ~InFlightCaptureChange() override;

  // InFlightChange:
  void Revert() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(InFlightCaptureChange);
};

class InFlightFocusChange : public InFlightWindowTreeClientChange {
 public:
  InFlightFocusChange(WindowTreeClient* client, Window* revert_value);
  ~InFlightFocusChange() override;

  // InFlightChange:
  void Revert() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(InFlightFocusChange);
};

class InFlightPropertyChange : public InFlightChange {
 public:
  InFlightPropertyChange(Window* window,
                         const std::string& property_name,
                         const mojo::Array<uint8_t>& revert_value);
  ~InFlightPropertyChange() override;

  // InFlightChange:
  bool Matches(const InFlightChange& change) const override;
  void SetRevertValueFrom(const InFlightChange& change) override;
  void Revert() override;

 private:
  const std::string property_name_;
  mojo::Array<uint8_t> revert_value_;

  DISALLOW_COPY_AND_ASSIGN(InFlightPropertyChange);
};

class InFlightPredefinedCursorChange : public InFlightChange {
 public:
  InFlightPredefinedCursorChange(Window* window, mojom::Cursor revert_value);
  ~InFlightPredefinedCursorChange() override;

  // InFlightChange:
  void SetRevertValueFrom(const InFlightChange& change) override;
  void Revert() override;

 private:
  mojom::Cursor revert_cursor_;

  DISALLOW_COPY_AND_ASSIGN(InFlightPredefinedCursorChange);
};

class InFlightVisibleChange : public InFlightChange {
 public:
  InFlightVisibleChange(Window* window, const bool revert_value);
  ~InFlightVisibleChange() override;

  // InFlightChange:
  void SetRevertValueFrom(const InFlightChange& change) override;
  void Revert() override;

 private:
  bool revert_visible_;

  DISALLOW_COPY_AND_ASSIGN(InFlightVisibleChange);
};

class InFlightOpacityChange : public InFlightChange {
 public:
  InFlightOpacityChange(Window* window, float revert_value);
  ~InFlightOpacityChange() override;

  // InFlightChange:
  void SetRevertValueFrom(const InFlightChange& change) override;
  void Revert() override;

 private:
  float revert_opacity_;

  DISALLOW_COPY_AND_ASSIGN(InFlightOpacityChange);
};

class InFlightSetModalChange : public InFlightChange {
 public:
  explicit InFlightSetModalChange(Window* window);
  ~InFlightSetModalChange() override;

  // InFlightChange:
  void SetRevertValueFrom(const InFlightChange& change) override;
  void Revert() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(InFlightSetModalChange);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_LIB_IN_FLIGHT_CHANGE_H_
