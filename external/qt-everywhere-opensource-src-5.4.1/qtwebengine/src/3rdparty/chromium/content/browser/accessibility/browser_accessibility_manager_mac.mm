// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_manager_mac.h"

#import "base/logging.h"
#import "content/browser/accessibility/browser_accessibility_cocoa.h"
#import "content/browser/accessibility/browser_accessibility_mac.h"
#include "content/common/accessibility_messages.h"

namespace content {

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    const ui::AXTreeUpdate& initial_tree,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory) {
  return new BrowserAccessibilityManagerMac(
      NULL, initial_tree, delegate, factory);
}

BrowserAccessibilityManagerMac::BrowserAccessibilityManagerMac(
    NSView* parent_view,
    const ui::AXTreeUpdate& initial_tree,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory)
    : BrowserAccessibilityManager(initial_tree, delegate, factory),
      parent_view_(parent_view),
      created_live_region_(false) {
}

// static
ui::AXTreeUpdate BrowserAccessibilityManagerMac::GetEmptyDocument() {
  ui::AXNodeData empty_document;
  empty_document.id = 0;
  empty_document.role = ui::AX_ROLE_ROOT_WEB_AREA;
  empty_document.state =
      1 << ui::AX_STATE_READ_ONLY;
  ui::AXTreeUpdate update;
  update.nodes.push_back(empty_document);
  return update;
}

BrowserAccessibility* BrowserAccessibilityManagerMac::GetFocus(
    BrowserAccessibility* root) {
  BrowserAccessibility* node = GetActiveDescendantFocus(root);
  return node;
}

void BrowserAccessibilityManagerMac::NotifyAccessibilityEvent(
    ui::AXEvent event_type,
    BrowserAccessibility* node) {
  if (!node->IsNative())
    return;

  // Refer to AXObjectCache.mm (webkit).
  NSString* event_id = @"";
  switch (event_type) {
    case ui::AX_EVENT_ACTIVEDESCENDANTCHANGED:
      if (node->GetRole() == ui::AX_ROLE_TREE) {
        event_id = NSAccessibilitySelectedRowsChangedNotification;
      } else {
        event_id = NSAccessibilityFocusedUIElementChangedNotification;
        BrowserAccessibility* active_descendant_focus =
            GetActiveDescendantFocus(GetRoot());
        if (active_descendant_focus)
          node = active_descendant_focus;
      }

      break;
    case ui::AX_EVENT_ALERT:
      // Not used on Mac.
      return;
    case ui::AX_EVENT_BLUR:
      // A no-op on Mac.
      return;
    case ui::AX_EVENT_CHECKED_STATE_CHANGED:
      // Not used on Mac.
      return;
    case ui::AX_EVENT_CHILDREN_CHANGED:
      // TODO(dtseng): no clear equivalent on Mac.
      return;
    case ui::AX_EVENT_FOCUS:
      event_id = NSAccessibilityFocusedUIElementChangedNotification;
      break;
    case ui::AX_EVENT_LAYOUT_COMPLETE:
      event_id = @"AXLayoutComplete";
      break;
    case ui::AX_EVENT_LIVE_REGION_CHANGED:
      event_id = @"AXLiveRegionChanged";
      break;
    case ui::AX_EVENT_LOAD_COMPLETE:
      event_id = @"AXLoadComplete";
      break;
    case ui::AX_EVENT_MENU_LIST_VALUE_CHANGED:
      // Not used on Mac.
      return;
    case ui::AX_EVENT_SHOW:
      // Not used on Mac.
      return;
    case ui::AX_EVENT_HIDE:
      // Not used on Mac.
      return;
    case ui::AX_EVENT_ROW_COUNT_CHANGED:
      event_id = NSAccessibilityRowCountChangedNotification;
      break;
    case ui::AX_EVENT_ROW_COLLAPSED:
      event_id = @"AXRowCollapsed";
      break;
    case ui::AX_EVENT_ROW_EXPANDED:
      event_id = @"AXRowExpanded";
      break;
    case ui::AX_EVENT_SCROLLED_TO_ANCHOR:
      // Not used on Mac.
      return;
    case ui::AX_EVENT_SELECTED_CHILDREN_CHANGED:
      event_id = NSAccessibilitySelectedChildrenChangedNotification;
      break;
    case ui::AX_EVENT_SELECTED_TEXT_CHANGED:
      event_id = NSAccessibilitySelectedTextChangedNotification;
      break;
    case ui::AX_EVENT_TEXT_INSERTED:
      // Not used on Mac.
      return;
    case ui::AX_EVENT_TEXT_REMOVED:
      // Not used on Mac.
      return;
    case ui::AX_EVENT_VALUE_CHANGED:
      event_id = NSAccessibilityValueChangedNotification;
      break;
    case ui::AX_EVENT_ARIA_ATTRIBUTE_CHANGED:
      // Not used on Mac.
      return;
    case ui::AX_EVENT_AUTOCORRECTION_OCCURED:
      // Not used on Mac.
      return;
    case ui::AX_EVENT_INVALID_STATUS_CHANGED:
      // Not used on Mac.
      return;
    case ui::AX_EVENT_LOCATION_CHANGED:
      // Not used on Mac.
      return;
    case ui::AX_EVENT_MENU_LIST_ITEM_SELECTED:
      // Not used on Mac.
      return;
    case ui::AX_EVENT_TEXT_CHANGED:
      // Not used on Mac.
      return;
    default:
      LOG(WARNING) << "Unknown accessibility event: " << event_type;
      return;
  }
  BrowserAccessibilityCocoa* native_node = node->ToBrowserAccessibilityCocoa();
  DCHECK(native_node);
  NSAccessibilityPostNotification(native_node, event_id);
}

void BrowserAccessibilityManagerMac::OnNodeCreationFinished(ui::AXNode* node) {
  BrowserAccessibility* obj = GetFromAXNode(node);
  if (obj && obj->HasStringAttribute(ui::AX_ATTR_LIVE_STATUS))
    created_live_region_ = true;
}

void BrowserAccessibilityManagerMac::OnTreeUpdateFinished() {
  if (!created_live_region_)
    return;

  // This code is to work around a bug in VoiceOver, where a new live
  // region that gets added is ignored. VoiceOver seems to only scan the
  // page for live regions once. By recreating the NSAccessibility
  // object for the root of the tree, we force VoiceOver to clear out its
  // internal state and find newly-added live regions this time.
  BrowserAccessibilityMac* root =
      static_cast<BrowserAccessibilityMac*>(GetRoot());
  root->RecreateNativeObject();
  NotifyAccessibilityEvent(ui::AX_EVENT_CHILDREN_CHANGED, root);

  created_live_region_ = false;
}

}  // namespace content
