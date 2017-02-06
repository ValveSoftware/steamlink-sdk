// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/test_change_tracker.h"

#include <stddef.h>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/mus/common/util.h"
#include "mojo/common/common_type_converters.h"

using mojo::Array;
using mojo::String;

namespace mus {

namespace ws {

std::string WindowIdToString(Id id) {
  return (id == 0) ? "null"
                   : base::StringPrintf("%d,%d", HiWord(id), LoWord(id));
}

namespace {

std::string DirectionToString(mojom::OrderDirection direction) {
  return direction == mojom::OrderDirection::ABOVE ? "above" : "below";
}

enum class ChangeDescriptionType {
  ONE,
  TWO,
};

std::string ChangeToDescription(const Change& change,
                                ChangeDescriptionType type) {
  switch (change.type) {
    case CHANGE_TYPE_EMBED:
      if (type == ChangeDescriptionType::ONE)
        return "OnEmbed";
      return base::StringPrintf("OnEmbed drawn=%s",
                                change.bool_value ? "true" : "false");

    case CHANGE_TYPE_EMBEDDED_APP_DISCONNECTED:
      return base::StringPrintf("OnEmbeddedAppDisconnected window=%s",
                                WindowIdToString(change.window_id).c_str());

    case CHANGE_TYPE_UNEMBED:
      return base::StringPrintf("OnUnembed window=%s",
                                WindowIdToString(change.window_id).c_str());

    case CHANGE_TYPE_LOST_CAPTURE:
      return base::StringPrintf("OnLostCapture window=%s",
                                WindowIdToString(change.window_id).c_str());

    case CHANGE_TYPE_NODE_ADD_TRANSIENT_WINDOW:
      return base::StringPrintf("AddTransientWindow parent = %s child = %s",
                                WindowIdToString(change.window_id).c_str(),
                                WindowIdToString(change.window_id2).c_str());

    case CHANGE_TYPE_NODE_BOUNDS_CHANGED:
      return base::StringPrintf(
          "BoundsChanged window=%s old_bounds=%s new_bounds=%s",
          WindowIdToString(change.window_id).c_str(),
          change.bounds.ToString().c_str(), change.bounds2.ToString().c_str());

    case CHANGE_TYPE_NODE_HIERARCHY_CHANGED:
      return base::StringPrintf(
          "HierarchyChanged window=%s old_parent=%s new_parent=%s",
          WindowIdToString(change.window_id).c_str(),
          WindowIdToString(change.window_id2).c_str(),
          WindowIdToString(change.window_id3).c_str());

    case CHANGE_TYPE_NODE_REMOVE_TRANSIENT_WINDOW_FROM_PARENT:
      return base::StringPrintf(
          "RemoveTransientWindowFromParent parent = %s child = %s",
          WindowIdToString(change.window_id).c_str(),
          WindowIdToString(change.window_id2).c_str());

    case CHANGE_TYPE_NODE_REORDERED:
      return base::StringPrintf("Reordered window=%s relative=%s direction=%s",
                                WindowIdToString(change.window_id).c_str(),
                                WindowIdToString(change.window_id2).c_str(),
                                DirectionToString(change.direction).c_str());

    case CHANGE_TYPE_NODE_DELETED:
      return base::StringPrintf("WindowDeleted window=%s",
                                WindowIdToString(change.window_id).c_str());

    case CHANGE_TYPE_NODE_VISIBILITY_CHANGED:
      return base::StringPrintf("VisibilityChanged window=%s visible=%s",
                                WindowIdToString(change.window_id).c_str(),
                                change.bool_value ? "true" : "false");

    case CHANGE_TYPE_NODE_DRAWN_STATE_CHANGED:
      return base::StringPrintf("DrawnStateChanged window=%s drawn=%s",
                                WindowIdToString(change.window_id).c_str(),
                                change.bool_value ? "true" : "false");

    case CHANGE_TYPE_INPUT_EVENT: {
      std::string result = base::StringPrintf(
          "InputEvent window=%s event_action=%d",
          WindowIdToString(change.window_id).c_str(), change.event_action);
      if (change.event_observer_id != 0)
        base::StringAppendF(&result, " event_observer_id=%u",
                            change.event_observer_id);
      return result;
    }

    case CHANGE_TYPE_EVENT_OBSERVED:
      return base::StringPrintf(
          "EventObserved event_action=%d event_observer_id=%u",
          change.event_action, change.event_observer_id);

    case CHANGE_TYPE_PROPERTY_CHANGED:
      return base::StringPrintf("PropertyChanged window=%s key=%s value=%s",
                                WindowIdToString(change.window_id).c_str(),
                                change.property_key.c_str(),
                                change.property_value.c_str());

    case CHANGE_TYPE_FOCUSED:
      return base::StringPrintf("Focused id=%s",
                                WindowIdToString(change.window_id).c_str());

    case CHANGE_TYPE_CURSOR_CHANGED:
      return base::StringPrintf("CursorChanged id=%s cursor_id=%d",
                                WindowIdToString(change.window_id).c_str(),
                                change.cursor_id);
    case CHANGE_TYPE_ON_CHANGE_COMPLETED:
      return base::StringPrintf("ChangeCompleted id=%d sucess=%s",
                                change.change_id,
                                change.bool_value ? "true" : "false");

    case CHANGE_TYPE_ON_TOP_LEVEL_CREATED:
      return base::StringPrintf("TopLevelCreated id=%d window_id=%s drawn=%s",
                                change.change_id,
                                WindowIdToString(change.window_id).c_str(),
                                change.bool_value ? "true" : "false");
    case CHANGE_TYPE_OPACITY:
      return base::StringPrintf("OpacityChanged window_id=%s opacity=%.2f",
                                WindowIdToString(change.window_id).c_str(),
                                change.float_value);
  }
  return std::string();
}

std::string SingleChangeToDescriptionImpl(const std::vector<Change>& changes,
                                          ChangeDescriptionType change_type) {
  std::string result;
  for (auto& change : changes) {
    if (!result.empty())
      result += "\n";
    result += ChangeToDescription(change, change_type);
  }
  return result;
}

}  // namespace

std::vector<std::string> ChangesToDescription1(
    const std::vector<Change>& changes) {
  std::vector<std::string> strings(changes.size());
  for (size_t i = 0; i < changes.size(); ++i)
    strings[i] = ChangeToDescription(changes[i], ChangeDescriptionType::ONE);
  return strings;
}

std::string SingleChangeToDescription(const std::vector<Change>& changes) {
  return SingleChangeToDescriptionImpl(changes, ChangeDescriptionType::ONE);
}

std::string SingleChangeToDescription2(const std::vector<Change>& changes) {
  return SingleChangeToDescriptionImpl(changes, ChangeDescriptionType::TWO);
}

std::string SingleWindowDescription(const std::vector<TestWindow>& windows) {
  if (windows.empty())
    return "no windows";
  std::string result;
  for (const TestWindow& window : windows)
    result += window.ToString();
  return result;
}

std::string ChangeWindowDescription(const std::vector<Change>& changes) {
  if (changes.size() != 1)
    return std::string();
  std::vector<std::string> window_strings(changes[0].windows.size());
  for (size_t i = 0; i < changes[0].windows.size(); ++i)
    window_strings[i] = "[" + changes[0].windows[i].ToString() + "]";
  return base::JoinString(window_strings, ",");
}

TestWindow WindowDataToTestWindow(const mojom::WindowDataPtr& data) {
  TestWindow window;
  window.parent_id = data->parent_id;
  window.window_id = data->window_id;
  window.visible = data->visible;
  window.properties =
      data->properties.To<std::map<std::string, std::vector<uint8_t>>>();
  return window;
}

void WindowDatasToTestWindows(const Array<mojom::WindowDataPtr>& data,
                              std::vector<TestWindow>* test_windows) {
  for (size_t i = 0; i < data.size(); ++i)
    test_windows->push_back(WindowDataToTestWindow(data[i]));
}

Change::Change()
    : type(CHANGE_TYPE_EMBED),
      client_id(0),
      window_id(0),
      window_id2(0),
      window_id3(0),
      event_action(0),
      event_observer_id(0u),
      direction(mojom::OrderDirection::ABOVE),
      bool_value(false),
      float_value(0.f),
      cursor_id(0),
      change_id(0u) {}

Change::Change(const Change& other) = default;

Change::~Change() {}

TestChangeTracker::TestChangeTracker() : delegate_(NULL) {}

TestChangeTracker::~TestChangeTracker() {}

void TestChangeTracker::OnEmbed(ClientSpecificId client_id,
                                mojom::WindowDataPtr root,
                                bool drawn) {
  Change change;
  change.type = CHANGE_TYPE_EMBED;
  change.client_id = client_id;
  change.bool_value = drawn;
  change.windows.push_back(WindowDataToTestWindow(root));
  AddChange(change);
}

void TestChangeTracker::OnEmbeddedAppDisconnected(Id window_id) {
  Change change;
  change.type = CHANGE_TYPE_EMBEDDED_APP_DISCONNECTED;
  change.window_id = window_id;
  AddChange(change);
}

void TestChangeTracker::OnWindowBoundsChanged(Id window_id,
                                              const gfx::Rect& old_bounds,
                                              const gfx::Rect& new_bounds) {
  Change change;
  change.type = CHANGE_TYPE_NODE_BOUNDS_CHANGED;
  change.window_id = window_id;
  change.bounds = old_bounds;
  change.bounds2 = new_bounds;
  AddChange(change);
}

void TestChangeTracker::OnUnembed(Id window_id) {
  Change change;
  change.type = CHANGE_TYPE_UNEMBED;
  change.window_id = window_id;
  AddChange(change);
}

void TestChangeTracker::OnTransientWindowAdded(Id window_id,
                                               Id transient_window_id) {
  Change change;
  change.type = CHANGE_TYPE_NODE_ADD_TRANSIENT_WINDOW;
  change.window_id = window_id;
  change.window_id2 = transient_window_id;
  AddChange(change);
}

void TestChangeTracker::OnTransientWindowRemoved(Id window_id,
                                                 Id transient_window_id) {
  Change change;
  change.type = CHANGE_TYPE_NODE_REMOVE_TRANSIENT_WINDOW_FROM_PARENT;
  change.window_id = window_id;
  change.window_id2 = transient_window_id;
  AddChange(change);
}

void TestChangeTracker::OnLostCapture(Id window_id) {
  Change change;
  change.type = CHANGE_TYPE_LOST_CAPTURE;
  change.window_id = window_id;
  AddChange(change);
}

void TestChangeTracker::OnWindowHierarchyChanged(
    Id window_id,
    Id old_parent_id,
    Id new_parent_id,
    Array<mojom::WindowDataPtr> windows) {
  Change change;
  change.type = CHANGE_TYPE_NODE_HIERARCHY_CHANGED;
  change.window_id = window_id;
  change.window_id2 = old_parent_id;
  change.window_id3 = new_parent_id;
  WindowDatasToTestWindows(windows, &change.windows);
  AddChange(change);
}

void TestChangeTracker::OnWindowReordered(Id window_id,
                                          Id relative_window_id,
                                          mojom::OrderDirection direction) {
  Change change;
  change.type = CHANGE_TYPE_NODE_REORDERED;
  change.window_id = window_id;
  change.window_id2 = relative_window_id;
  change.direction = direction;
  AddChange(change);
}

void TestChangeTracker::OnWindowDeleted(Id window_id) {
  Change change;
  change.type = CHANGE_TYPE_NODE_DELETED;
  change.window_id = window_id;
  AddChange(change);
}

void TestChangeTracker::OnWindowVisibilityChanged(Id window_id, bool visible) {
  Change change;
  change.type = CHANGE_TYPE_NODE_VISIBILITY_CHANGED;
  change.window_id = window_id;
  change.bool_value = visible;
  AddChange(change);
}

void TestChangeTracker::OnWindowOpacityChanged(Id window_id, float opacity) {
  Change change;
  change.type = CHANGE_TYPE_OPACITY;
  change.window_id = window_id;
  change.float_value = opacity;
  AddChange(change);
}

void TestChangeTracker::OnWindowParentDrawnStateChanged(Id window_id,
                                                        bool drawn) {
  Change change;
  change.type = CHANGE_TYPE_NODE_DRAWN_STATE_CHANGED;
  change.window_id = window_id;
  change.bool_value = drawn;
  AddChange(change);
}

void TestChangeTracker::OnWindowInputEvent(Id window_id,
                                           const ui::Event& event,
                                           uint32_t event_observer_id) {
  Change change;
  change.type = CHANGE_TYPE_INPUT_EVENT;
  change.window_id = window_id;
  change.event_action = static_cast<int32_t>(event.type());
  change.event_observer_id = event_observer_id;
  AddChange(change);
}

void TestChangeTracker::OnEventObserved(const ui::Event& event,
                                        uint32_t event_observer_id) {
  Change change;
  change.type = CHANGE_TYPE_EVENT_OBSERVED;
  change.event_action = static_cast<int32_t>(event.type());
  change.event_observer_id = event_observer_id;
  AddChange(change);
}

void TestChangeTracker::OnWindowSharedPropertyChanged(Id window_id,
                                                      String name,
                                                      Array<uint8_t> data) {
  Change change;
  change.type = CHANGE_TYPE_PROPERTY_CHANGED;
  change.window_id = window_id;
  change.property_key = name;
  if (data.is_null())
    change.property_value = "NULL";
  else
    change.property_value = data.To<std::string>();
  AddChange(change);
}

void TestChangeTracker::OnWindowFocused(Id window_id) {
  Change change;
  change.type = CHANGE_TYPE_FOCUSED;
  change.window_id = window_id;
  AddChange(change);
}

void TestChangeTracker::OnWindowPredefinedCursorChanged(
    Id window_id,
    mojom::Cursor cursor_id) {
  Change change;
  change.type = CHANGE_TYPE_CURSOR_CHANGED;
  change.window_id = window_id;
  change.cursor_id = static_cast<int32_t>(cursor_id);
  AddChange(change);
}

void TestChangeTracker::OnChangeCompleted(uint32_t change_id, bool success) {
  Change change;
  change.type = CHANGE_TYPE_ON_CHANGE_COMPLETED;
  change.change_id = change_id;
  change.bool_value = success;
  AddChange(change);
}

void TestChangeTracker::OnTopLevelCreated(uint32_t change_id,
                                          mojom::WindowDataPtr window_data,
                                          bool drawn) {
  Change change;
  change.type = CHANGE_TYPE_ON_TOP_LEVEL_CREATED;
  change.change_id = change_id;
  change.window_id = window_data->window_id;
  change.bool_value = drawn;
  AddChange(change);
}

void TestChangeTracker::AddChange(const Change& change) {
  changes_.push_back(change);
  if (delegate_)
    delegate_->OnChangeAdded();
}

TestWindow::TestWindow() {}

TestWindow::TestWindow(const TestWindow& other) = default;

TestWindow::~TestWindow() {}

std::string TestWindow::ToString() const {
  return base::StringPrintf("window=%s parent=%s",
                            WindowIdToString(window_id).c_str(),
                            WindowIdToString(parent_id).c_str());
}

std::string TestWindow::ToString2() const {
  return base::StringPrintf(
      "window=%s parent=%s visible=%s", WindowIdToString(window_id).c_str(),
      WindowIdToString(parent_id).c_str(), visible ? "true" : "false");
}

}  // namespace ws

}  // namespace mus
