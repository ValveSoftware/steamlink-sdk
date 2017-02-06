// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_TEST_CHANGE_TRACKER_H_
#define COMPONENTS_MUS_WS_TEST_CHANGE_TRACKER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "components/mus/common/types.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "mojo/public/cpp/bindings/array.h"
#include "ui/gfx/geometry/mojo/geometry.mojom.h"

namespace mus {

namespace ws {

enum ChangeType {
  CHANGE_TYPE_EMBED,
  CHANGE_TYPE_EMBEDDED_APP_DISCONNECTED,
  CHANGE_TYPE_UNEMBED,
  CHANGE_TYPE_LOST_CAPTURE,
  // TODO(sky): nuke NODE.
  CHANGE_TYPE_NODE_ADD_TRANSIENT_WINDOW,
  CHANGE_TYPE_NODE_BOUNDS_CHANGED,
  CHANGE_TYPE_NODE_HIERARCHY_CHANGED,
  CHANGE_TYPE_NODE_REMOVE_TRANSIENT_WINDOW_FROM_PARENT,
  CHANGE_TYPE_NODE_REORDERED,
  CHANGE_TYPE_NODE_VISIBILITY_CHANGED,
  CHANGE_TYPE_NODE_DRAWN_STATE_CHANGED,
  CHANGE_TYPE_NODE_DELETED,
  CHANGE_TYPE_INPUT_EVENT,
  CHANGE_TYPE_EVENT_OBSERVED,
  CHANGE_TYPE_PROPERTY_CHANGED,
  CHANGE_TYPE_FOCUSED,
  CHANGE_TYPE_CURSOR_CHANGED,
  CHANGE_TYPE_ON_CHANGE_COMPLETED,
  CHANGE_TYPE_ON_TOP_LEVEL_CREATED,
  CHANGE_TYPE_OPACITY,
};

// TODO(sky): consider nuking and converting directly to WindowData.
struct TestWindow {
  TestWindow();
  TestWindow(const TestWindow& other);
  ~TestWindow();

  // Returns a string description of this.
  std::string ToString() const;

  // Returns a string description that includes visible and drawn.
  std::string ToString2() const;

  Id parent_id;
  Id window_id;
  bool visible;
  std::map<std::string, std::vector<uint8_t>> properties;
};

// Tracks a call to WindowTreeClient. See the individual functions for the
// fields that are used.
struct Change {
  Change();
  Change(const Change& other);
  ~Change();

  ChangeType type;
  ClientSpecificId client_id;
  std::vector<TestWindow> windows;
  Id window_id;
  Id window_id2;
  Id window_id3;
  gfx::Rect bounds;
  gfx::Rect bounds2;
  int32_t event_action;
  uint32_t event_observer_id;
  mojo::String embed_url;
  mojom::OrderDirection direction;
  bool bool_value;
  float float_value;
  std::string property_key;
  std::string property_value;
  int32_t cursor_id;
  uint32_t change_id;
};

// Converts Changes to string descriptions.
std::vector<std::string> ChangesToDescription1(
    const std::vector<Change>& changes);

// Convenience for returning the description of the first item in |changes|.
// Returns an empty string if |changes| has something other than one entry.
std::string SingleChangeToDescription(const std::vector<Change>& changes);
std::string SingleChangeToDescription2(const std::vector<Change>& changes);

// Convenience for returning the description of the first item in |windows|.
// Returns an empty string if |windows| has something other than one entry.
std::string SingleWindowDescription(const std::vector<TestWindow>& windows);

// Returns a string description of |changes[0].windows|. Returns an empty string
// if change.size() != 1.
std::string ChangeWindowDescription(const std::vector<Change>& changes);

// Converts WindowDatas to TestWindows.
void WindowDatasToTestWindows(const mojo::Array<mojom::WindowDataPtr>& data,
                              std::vector<TestWindow>* test_windows);

// TestChangeTracker is used to record WindowTreeClient functions. It notifies
// a delegate any time a change is added.
class TestChangeTracker {
 public:
  // Used to notify the delegate when a change is added. A change corresponds to
  // a single WindowTreeClient function.
  class Delegate {
   public:
    virtual void OnChangeAdded() = 0;

   protected:
    virtual ~Delegate() {}
  };

  TestChangeTracker();
  ~TestChangeTracker();

  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  std::vector<Change>* changes() { return &changes_; }

  // Each of these functions generate a Change. There is one per
  // WindowTreeClient function.
  void OnEmbed(ClientSpecificId client_id,
               mojom::WindowDataPtr root,
               bool drawn);
  void OnEmbeddedAppDisconnected(Id window_id);
  void OnUnembed(Id window_id);
  void OnLostCapture(Id window_id);
  void OnTransientWindowAdded(Id window_id, Id transient_window_id);
  void OnTransientWindowRemoved(Id window_id, Id transient_window_id);
  void OnWindowBoundsChanged(Id window_id,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds);
  void OnWindowHierarchyChanged(Id window_id,
                                Id old_parent_id,
                                Id new_parent_id,
                                mojo::Array<mojom::WindowDataPtr> windows);
  void OnWindowReordered(Id window_id,
                         Id relative_window_id,
                         mojom::OrderDirection direction);
  void OnWindowDeleted(Id window_id);
  void OnWindowVisibilityChanged(Id window_id, bool visible);
  void OnWindowOpacityChanged(Id window_id, float opacity);
  void OnWindowParentDrawnStateChanged(Id window_id, bool drawn);
  void OnWindowInputEvent(Id window_id,
                          const ui::Event& event,
                          uint32_t event_observer_id);
  void OnEventObserved(const ui::Event& event, uint32_t event_observer_id);
  void OnWindowSharedPropertyChanged(Id window_id,
                                     mojo::String name,
                                     mojo::Array<uint8_t> data);
  void OnWindowFocused(Id window_id);
  void OnWindowPredefinedCursorChanged(Id window_id, mojom::Cursor cursor_id);
  void OnChangeCompleted(uint32_t change_id, bool success);
  void OnTopLevelCreated(uint32_t change_id,
                         mojom::WindowDataPtr window_data,
                         bool drawn);

 private:
  void AddChange(const Change& change);

  Delegate* delegate_;
  std::vector<Change> changes_;

  DISALLOW_COPY_AND_ASSIGN(TestChangeTracker);
};

}  // namespace ws

}  // namespace mus

#endif  // COMPONENTS_MUS_WS_TEST_CHANGE_TRACKER_H_
