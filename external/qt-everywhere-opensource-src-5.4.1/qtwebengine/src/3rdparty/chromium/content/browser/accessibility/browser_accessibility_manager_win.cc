// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_manager_win.h"

#include "base/command_line.h"
#include "base/win/scoped_comptr.h"
#include "base/win/windows_version.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/accessibility/browser_accessibility_win.h"
#include "content/browser/renderer_host/legacy_render_widget_host_win.h"
#include "content/common/accessibility_messages.h"
#include "ui/base/win/atl_module.h"

namespace content {

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    const ui::AXTreeUpdate& initial_tree,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory) {
  return new BrowserAccessibilityManagerWin(
      content::LegacyRenderWidgetHostHWND::Create(GetDesktopWindow()).get(),
      NULL, initial_tree, delegate, factory);
}

BrowserAccessibilityManagerWin*
BrowserAccessibilityManager::ToBrowserAccessibilityManagerWin() {
  return static_cast<BrowserAccessibilityManagerWin*>(this);
}

BrowserAccessibilityManagerWin::BrowserAccessibilityManagerWin(
    LegacyRenderWidgetHostHWND* accessible_hwnd,
    IAccessible* parent_iaccessible,
    const ui::AXTreeUpdate& initial_tree,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory)
    : BrowserAccessibilityManager(initial_tree, delegate, factory),
      parent_hwnd_(NULL),
      parent_iaccessible_(parent_iaccessible),
      tracked_scroll_object_(NULL),
      accessible_hwnd_(accessible_hwnd),
      focus_event_on_root_needed_(false) {
  ui::win::CreateATLModuleIfNeeded();
  if (accessible_hwnd_) {
    accessible_hwnd_->set_browser_accessibility_manager(this);
    parent_hwnd_ = accessible_hwnd_->GetParent();
  }
}

BrowserAccessibilityManagerWin::~BrowserAccessibilityManagerWin() {
  if (tracked_scroll_object_) {
    tracked_scroll_object_->Release();
    tracked_scroll_object_ = NULL;
  }
  if (accessible_hwnd_)
    accessible_hwnd_->OnManagerDeleted();
}

// static
ui::AXTreeUpdate BrowserAccessibilityManagerWin::GetEmptyDocument() {
  ui::AXNodeData empty_document;
  empty_document.id = 0;
  empty_document.role = ui::AX_ROLE_ROOT_WEB_AREA;
  empty_document.state =
      (1 << ui::AX_STATE_ENABLED) |
      (1 << ui::AX_STATE_READ_ONLY) |
      (1 << ui::AX_STATE_BUSY);

  ui::AXTreeUpdate update;
  update.nodes.push_back(empty_document);
  return update;
}

void BrowserAccessibilityManagerWin::SetAccessibleHWND(
    LegacyRenderWidgetHostHWND* accessible_hwnd) {
  accessible_hwnd_ = accessible_hwnd;
  if (accessible_hwnd_) {
    accessible_hwnd_->set_browser_accessibility_manager(this);
    parent_hwnd_ = accessible_hwnd_->GetParent();
  }
}

void BrowserAccessibilityManagerWin::MaybeCallNotifyWinEvent(DWORD event,
                                                             LONG child_id) {
  // If on Win 7 and complete accessibility is enabled, use the fake child HWND
  // to use as the root of the accessibility tree. See comments above
  // LegacyRenderWidgetHostHWND for details.
  if (accessible_hwnd_ &&
      BrowserAccessibilityStateImpl::GetInstance()->IsAccessibleBrowser()) {
    parent_hwnd_ = accessible_hwnd_->hwnd();
    parent_iaccessible_ = accessible_hwnd_->window_accessible();
  }

  // Only fire events if this view is hooked up to its parent.
  if (parent_iaccessible() && parent_hwnd())
    ::NotifyWinEvent(event, parent_hwnd(), OBJID_CLIENT, child_id);
}


void BrowserAccessibilityManagerWin::OnNodeCreated(ui::AXNode* node) {
  BrowserAccessibilityManager::OnNodeCreated(node);
  BrowserAccessibility* obj = GetFromAXNode(node);
  LONG unique_id_win = obj->ToBrowserAccessibilityWin()->unique_id_win();
  unique_id_to_ax_id_map_[unique_id_win] = obj->GetId();
}

void BrowserAccessibilityManagerWin::OnNodeWillBeDeleted(ui::AXNode* node) {
  BrowserAccessibilityManager::OnNodeWillBeDeleted(node);
  BrowserAccessibility* obj = GetFromAXNode(node);
  if (!obj)
    return;
  unique_id_to_ax_id_map_.erase(
      obj->ToBrowserAccessibilityWin()->unique_id_win());
  if (obj == tracked_scroll_object_) {
    tracked_scroll_object_->Release();
    tracked_scroll_object_ = NULL;
  }
}

void BrowserAccessibilityManagerWin::OnWindowFocused() {
  // This is called either when this web frame gets focused, or when
  // the root of the accessibility tree changes. In both cases, we need
  // to fire a focus event on the root and then on the focused element
  // within the page, if different.

  // Set this flag so that we'll keep trying to fire these focus events
  // if they're not successful this time.
  focus_event_on_root_needed_ = true;

  if (!delegate_ || !delegate_->AccessibilityViewHasFocus())
    return;

  // Try to fire a focus event on the root first and then the focused node.
  // This will clear focus_event_on_root_needed_ if successful.
  if (focus_ != tree_->GetRoot())
    NotifyAccessibilityEvent(ui::AX_EVENT_FOCUS, GetRoot());
  BrowserAccessibilityManager::OnWindowFocused();
}

void BrowserAccessibilityManagerWin::NotifyAccessibilityEvent(
    ui::AXEvent event_type,
    BrowserAccessibility* node) {
  if (node->GetRole() == ui::AX_ROLE_INLINE_TEXT_BOX)
    return;

  // Don't fire focus, blur, or load complete notifications if the
  // window isn't focused, because that can confuse screen readers into
  // entering their "browse" mode.
  if ((event_type == ui::AX_EVENT_FOCUS ||
       event_type == ui::AX_EVENT_BLUR ||
       event_type == ui::AX_EVENT_LOAD_COMPLETE) &&
      (!delegate_ || !delegate_->AccessibilityViewHasFocus())) {
    return;
  }

  // NVDA gets confused if we focus the main document element when it hasn't
  // finished loading and it has no children at all, so suppress that event.
  if (event_type == ui::AX_EVENT_FOCUS &&
      node == GetRoot() &&
      node->PlatformChildCount() == 0 &&
      !node->HasState(ui::AX_STATE_BUSY) &&
      !node->GetBoolAttribute(ui::AX_ATTR_DOC_LOADED)) {
    return;
  }

  // If a focus event is needed on the root, fire that first before
  // this event.
  if (event_type == ui::AX_EVENT_FOCUS && node == GetRoot())
    focus_event_on_root_needed_ = false;
  else if (focus_event_on_root_needed_)
    OnWindowFocused();

  LONG event_id = EVENT_MIN;
  switch (event_type) {
    case ui::AX_EVENT_ACTIVEDESCENDANTCHANGED:
      event_id = IA2_EVENT_ACTIVE_DESCENDANT_CHANGED;
      break;
    case ui::AX_EVENT_ALERT:
      event_id = EVENT_SYSTEM_ALERT;
      break;
    case ui::AX_EVENT_ARIA_ATTRIBUTE_CHANGED:
      event_id = IA2_EVENT_OBJECT_ATTRIBUTE_CHANGED;
      break;
    case ui::AX_EVENT_AUTOCORRECTION_OCCURED:
      event_id = IA2_EVENT_OBJECT_ATTRIBUTE_CHANGED;
      break;
    case ui::AX_EVENT_BLUR:
      // Equivalent to focus on the root.
      event_id = EVENT_OBJECT_FOCUS;
      node = GetRoot();
      break;
    case ui::AX_EVENT_CHECKED_STATE_CHANGED:
      event_id = EVENT_OBJECT_STATECHANGE;
      break;
    case ui::AX_EVENT_CHILDREN_CHANGED:
      event_id = EVENT_OBJECT_REORDER;
      break;
    case ui::AX_EVENT_FOCUS:
      event_id = EVENT_OBJECT_FOCUS;
      break;
    case ui::AX_EVENT_INVALID_STATUS_CHANGED:
      event_id = EVENT_OBJECT_STATECHANGE;
      break;
    case ui::AX_EVENT_LIVE_REGION_CHANGED:
      if (node->GetBoolAttribute(ui::AX_ATTR_CONTAINER_LIVE_BUSY))
        return;
      event_id = EVENT_OBJECT_LIVEREGIONCHANGED;
      break;
    case ui::AX_EVENT_LOAD_COMPLETE:
      event_id = IA2_EVENT_DOCUMENT_LOAD_COMPLETE;
      break;
    case ui::AX_EVENT_MENU_LIST_ITEM_SELECTED:
      event_id = EVENT_OBJECT_FOCUS;
      break;
    case ui::AX_EVENT_MENU_LIST_VALUE_CHANGED:
      event_id = EVENT_OBJECT_VALUECHANGE;
      break;
    case ui::AX_EVENT_HIDE:
      event_id = EVENT_OBJECT_HIDE;
      break;
    case ui::AX_EVENT_SHOW:
      event_id = EVENT_OBJECT_SHOW;
      break;
    case ui::AX_EVENT_SCROLL_POSITION_CHANGED:
      event_id = EVENT_SYSTEM_SCROLLINGEND;
      break;
    case ui::AX_EVENT_SCROLLED_TO_ANCHOR:
      event_id = EVENT_SYSTEM_SCROLLINGSTART;
      break;
    case ui::AX_EVENT_SELECTED_CHILDREN_CHANGED:
      event_id = EVENT_OBJECT_SELECTIONWITHIN;
      break;
    case ui::AX_EVENT_SELECTED_TEXT_CHANGED:
      event_id = IA2_EVENT_TEXT_CARET_MOVED;
      break;
    case ui::AX_EVENT_TEXT_CHANGED:
      event_id = EVENT_OBJECT_NAMECHANGE;
      break;
    case ui::AX_EVENT_TEXT_INSERTED:
      event_id = IA2_EVENT_TEXT_INSERTED;
      break;
    case ui::AX_EVENT_TEXT_REMOVED:
      event_id = IA2_EVENT_TEXT_REMOVED;
      break;
    case ui::AX_EVENT_VALUE_CHANGED:
      event_id = EVENT_OBJECT_VALUECHANGE;
      break;
    default:
      // Not all WebKit accessibility events result in a Windows
      // accessibility notification.
      break;
  }

  if (event_id != EVENT_MIN) {
    // Pass the node's unique id in the |child_id| argument to NotifyWinEvent;
    // the AT client will then call get_accChild on the HWND's accessibility
    // object and pass it that same id, which we can use to retrieve the
    // IAccessible for this node.
    LONG child_id = node->ToBrowserAccessibilityWin()->unique_id_win();
    MaybeCallNotifyWinEvent(event_id, child_id);
  }

  // If this is a layout complete notification (sent when a container scrolls)
  // and there is a descendant tracked object, send a notification on it.
  // TODO(dmazzoni): remove once http://crbug.com/113483 is fixed.
  if (event_type == ui::AX_EVENT_LAYOUT_COMPLETE &&
      tracked_scroll_object_ &&
      tracked_scroll_object_->IsDescendantOf(node)) {
    MaybeCallNotifyWinEvent(
        IA2_EVENT_VISIBLE_DATA_CHANGED,
        tracked_scroll_object_->ToBrowserAccessibilityWin()->unique_id_win());
    tracked_scroll_object_->Release();
    tracked_scroll_object_ = NULL;
  }
}

void BrowserAccessibilityManagerWin::OnRootChanged(ui::AXNode* new_root) {
  // In order to make screen readers aware of the new accessibility root,
  // we need to fire a focus event on it.
  OnWindowFocused();
}

void BrowserAccessibilityManagerWin::TrackScrollingObject(
    BrowserAccessibilityWin* node) {
  if (tracked_scroll_object_)
    tracked_scroll_object_->Release();
  tracked_scroll_object_ = node;
  tracked_scroll_object_->AddRef();
}

BrowserAccessibilityWin* BrowserAccessibilityManagerWin::GetFromUniqueIdWin(
    LONG unique_id_win) {
  base::hash_map<LONG, int32>::iterator iter =
      unique_id_to_ax_id_map_.find(unique_id_win);
  if (iter != unique_id_to_ax_id_map_.end()) {
    BrowserAccessibility* result = GetFromID(iter->second);
    if (result)
      return result->ToBrowserAccessibilityWin();
  }
  return NULL;
}

void BrowserAccessibilityManagerWin::OnAccessibleHwndDeleted() {
  // If the AccessibleHWND is deleted, |parent_hwnd_| and
  // |parent_iaccessible_| are no longer valid either, since they were
  // derived from AccessibleHWND. We don't have to restore them to
  // previous values, though, because this should only happen
  // during the destruct sequence for this window.
  accessible_hwnd_ = NULL;
  parent_hwnd_ = NULL;
  parent_iaccessible_ = NULL;
}

}  // namespace content
