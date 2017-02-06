// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_manager_mac.h"

#import "base/mac/mac_util.h"
#import "base/mac/sdk_forward_declarations.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "content/browser/accessibility/browser_accessibility_cocoa.h"
#import "content/browser/accessibility/browser_accessibility_mac.h"
#include "content/common/accessibility_messages.h"

namespace {

// Declare undocumented accessibility constants and enums only present in
// WebKit.

enum AXTextStateChangeType {
  AXTextStateChangeTypeUnknown,
  AXTextStateChangeTypeEdit,
  AXTextStateChangeTypeSelectionMove,
  AXTextStateChangeTypeSelectionExtend
};

enum AXTextSelectionDirection {
  AXTextSelectionDirectionUnknown,
  AXTextSelectionDirectionBeginning,
  AXTextSelectionDirectionEnd,
  AXTextSelectionDirectionPrevious,
  AXTextSelectionDirectionNext,
  AXTextSelectionDirectionDiscontiguous
};

enum AXTextSelectionGranularity {
  AXTextSelectionGranularityUnknown,
  AXTextSelectionGranularityCharacter,
  AXTextSelectionGranularityWord,
  AXTextSelectionGranularityLine,
  AXTextSelectionGranularitySentence,
  AXTextSelectionGranularityParagraph,
  AXTextSelectionGranularityPage,
  AXTextSelectionGranularityDocument,
  AXTextSelectionGranularityAll
};

enum AXTextEditType {
  AXTextEditTypeUnknown,
  AXTextEditTypeDelete,
  AXTextEditTypeInsert,
  AXTextEditTypeTyping,
  AXTextEditTypeDictation,
  AXTextEditTypeCut,
  AXTextEditTypePaste,
  AXTextEditTypeAttributesChange
};

NSString* const NSAccessibilityAutocorrectionOccurredNotification =
    @"AXAutocorrectionOccurred";
NSString* const NSAccessibilityLayoutCompleteNotification =
    @"AXLayoutComplete";
NSString* const NSAccessibilityLoadCompleteNotification =
    @"AXLoadComplete";
NSString* const NSAccessibilityInvalidStatusChangedNotification =
    @"AXInvalidStatusChanged";
NSString* const NSAccessibilityLiveRegionChangedNotification =
    @"AXLiveRegionChanged";
NSString* const NSAccessibilityExpandedChanged =
    @"AXExpandedChanged";
NSString* const NSAccessibilityMenuItemSelectedNotification =
    @"AXMenuItemSelected";

// Attributes used for NSAccessibilitySelectedTextChangedNotification and
// NSAccessibilityValueChangedNotification.
NSString* const NSAccessibilityTextStateChangeTypeKey =
    @"AXTextStateChangeType";
NSString* const NSAccessibilityTextStateSyncKey = @"AXTextStateSync";
NSString* const NSAccessibilityTextSelectionDirection =
    @"AXTextSelectionDirection";
NSString* const NSAccessibilityTextSelectionGranularity =
    @"AXTextSelectionGranularity";
NSString* const NSAccessibilityTextSelectionChangedFocus =
    @"AXTextSelectionChangedFocus";
NSString* const NSAccessibilitySelectedTextMarkerRangeAttribute =
    @"AXSelectedTextMarkerRange";
NSString* const NSAccessibilityTextChangeElement = @"AXTextChangeElement";
NSString* const NSAccessibilityTextEditType = @"AXTextEditType";
NSString* const NSAccessibilityTextChangeValue = @"AXTextChangeValue";
NSString* const NSAccessibilityTextChangeValueLength =
    @"AXTextChangeValueLength";
NSString* const NSAccessibilityTextChangeValues = @"AXTextChangeValues";

}  // namespace

namespace content {

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    const ui::AXTreeUpdate& initial_tree,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory) {
  return new BrowserAccessibilityManagerMac(
      NULL, initial_tree, delegate, factory);
}

BrowserAccessibilityManagerMac*
BrowserAccessibilityManager::ToBrowserAccessibilityManagerMac() {
  return static_cast<BrowserAccessibilityManagerMac*>(this);
}

BrowserAccessibilityManagerMac::BrowserAccessibilityManagerMac(
    NSView* parent_view,
    const ui::AXTreeUpdate& initial_tree,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory)
    : BrowserAccessibilityManager(delegate, factory),
      parent_view_(parent_view) {
  Initialize(initial_tree);
}

BrowserAccessibilityManagerMac::~BrowserAccessibilityManagerMac() {}

// static
ui::AXTreeUpdate
    BrowserAccessibilityManagerMac::GetEmptyDocument() {
  ui::AXNodeData empty_document;
  empty_document.id = 0;
  empty_document.role = ui::AX_ROLE_ROOT_WEB_AREA;
  empty_document.state =
      1 << ui::AX_STATE_READ_ONLY;
  ui::AXTreeUpdate update;
  update.root_id = empty_document.id;
  update.nodes.push_back(empty_document);
  return update;
}

BrowserAccessibility* BrowserAccessibilityManagerMac::GetFocus() {
  BrowserAccessibility* focus = BrowserAccessibilityManager::GetFocus();

  // On Mac, list boxes should always get focus on the whole list, otherwise
  // information about the number of selected items will never be reported.
  // For editable combo boxes, focus should stay on the combo box so the user
  // will not be taken out of the combo box while typing.
  if (focus && (focus->GetRole() == ui::AX_ROLE_LIST_BOX ||
                (focus->GetRole() == ui::AX_ROLE_COMBO_BOX &&
                 focus->HasState(ui::AX_STATE_EDITABLE)))) {
    return focus;
  }

  // For other roles, follow the active descendant.
  return GetActiveDescendant(focus);
}

void BrowserAccessibilityManagerMac::NotifyAccessibilityEvent(
    BrowserAccessibilityEvent::Source source,
    ui::AXEvent event_type,
    BrowserAccessibility* node) {
  if (!node->IsNative())
    return;

  auto native_node = ToBrowserAccessibilityCocoa(node);
  DCHECK(native_node);

  // Refer to |AXObjectCache::postPlatformNotification| in WebKit source code.
  NSString* mac_notification;
  switch (event_type) {
    case ui::AX_EVENT_ACTIVEDESCENDANTCHANGED:
      if (node->GetRole() == ui::AX_ROLE_TREE) {
        mac_notification = NSAccessibilitySelectedRowsChangedNotification;
      } else if (node->GetRole() == ui::AX_ROLE_COMBO_BOX) {
        // Even though the selected item in the combo box has changed, we don't
        // want to post a focus change because this will take the focus out of
        // the combo box where the user might be typing.
        mac_notification = NSAccessibilitySelectedChildrenChangedNotification;
      } else {
        // In all other cases we should post
        // |NSAccessibilityFocusedUIElementChangedNotification|, but this is
        // handled elsewhere.
        return;
      }
      break;
    case ui::AX_EVENT_AUTOCORRECTION_OCCURED:
      mac_notification = NSAccessibilityAutocorrectionOccurredNotification;
      break;
    case ui::AX_EVENT_FOCUS:
      mac_notification = NSAccessibilityFocusedUIElementChangedNotification;
      break;
    case ui::AX_EVENT_LAYOUT_COMPLETE:
      mac_notification = NSAccessibilityLayoutCompleteNotification;
      break;
    case ui::AX_EVENT_LOAD_COMPLETE:
      // This notification should only be fired on the top document.
      // Iframes should use |AX_EVENT_LAYOUT_COMPLETE| to signify that they have
      // finished loading.
      if (IsRootTree()) {
        mac_notification = NSAccessibilityLoadCompleteNotification;
      } else {
        mac_notification = NSAccessibilityLayoutCompleteNotification;
      }
      break;
    case ui::AX_EVENT_INVALID_STATUS_CHANGED:
      mac_notification = NSAccessibilityInvalidStatusChangedNotification;
      break;
    case ui::AX_EVENT_SELECTED_CHILDREN_CHANGED:
      if (node->GetRole() == ui::AX_ROLE_GRID ||
          node->GetRole() == ui::AX_ROLE_TABLE) {
        mac_notification = NSAccessibilitySelectedRowsChangedNotification;
      } else {
        mac_notification = NSAccessibilitySelectedChildrenChangedNotification;
      }
      break;
    case ui::AX_EVENT_DOCUMENT_SELECTION_CHANGED: {
      mac_notification = NSAccessibilitySelectedTextChangedNotification;
      // WebKit fires a notification both on the focused object and the root.
      BrowserAccessibility* focus = GetFocus();
      if (!focus)
        break;  // Just fire a notification on the root.
      NSAccessibilityPostNotification(ToBrowserAccessibilityCocoa(focus),
                                      mac_notification);

      if (base::mac::IsOSElCapitanOrLater()) {
        // |NSAccessibilityPostNotificationWithUserInfo| should be used on OS X
        // 10.11 or later to notify Voiceover about text selection changes. This
        // API has been present on versions of OS X since 10.7 but doesn't
        // appear to be needed by Voiceover before version 10.11.
        NSDictionary* user_info =
            GetUserInfoForSelectedTextChangedNotification();

        BrowserAccessibility* root = GetRoot();
        if (!root)
          return;

        NSAccessibilityPostNotificationWithUserInfo(
            ToBrowserAccessibilityCocoa(focus), mac_notification, user_info);
        NSAccessibilityPostNotificationWithUserInfo(
            ToBrowserAccessibilityCocoa(root), mac_notification, user_info);
        return;
      }
      break;
    }
    case ui::AX_EVENT_CHECKED_STATE_CHANGED:
      mac_notification = NSAccessibilityValueChangedNotification;
      break;
    case ui::AX_EVENT_VALUE_CHANGED:
      mac_notification = NSAccessibilityValueChangedNotification;
      if (base::mac::IsOSElCapitanOrLater() && text_edits_.size()) {
        // It seems that we don't need to distinguish between deleted and
        // inserted text for now.
        base::string16 deleted_text;
        base::string16 inserted_text;
        int32_t id = node->GetId();
        const auto iterator = text_edits_.find(id);
        if (iterator != text_edits_.end())
          inserted_text = iterator->second;
        NSDictionary* user_info = GetUserInfoForValueChangedNotification(
            native_node, deleted_text, inserted_text);

        BrowserAccessibility* root = GetRoot();
        if (!root)
          return;

        NSAccessibilityPostNotificationWithUserInfo(
            native_node, mac_notification, user_info);
        NSAccessibilityPostNotificationWithUserInfo(
            ToBrowserAccessibilityCocoa(root), mac_notification, user_info);
        return;
      }
      break;
    // TODO(nektar): Need to add an event for live region created.
    case ui::AX_EVENT_LIVE_REGION_CHANGED:
      mac_notification = NSAccessibilityLiveRegionChangedNotification;
      break;
    case ui::AX_EVENT_ROW_COUNT_CHANGED:
      mac_notification = NSAccessibilityRowCountChangedNotification;
      break;
    case ui::AX_EVENT_ROW_EXPANDED:
      mac_notification = NSAccessibilityRowExpandedNotification;
      break;
    case ui::AX_EVENT_ROW_COLLAPSED:
      mac_notification = NSAccessibilityRowCollapsedNotification;
      break;
    // TODO(nektar): Add events for busy.
    case ui::AX_EVENT_EXPANDED_CHANGED:
      mac_notification = NSAccessibilityExpandedChanged;
      break;
    // TODO(nektar): Support menu open/close notifications.
    case ui::AX_EVENT_MENU_LIST_ITEM_SELECTED:
      mac_notification = NSAccessibilityMenuItemSelectedNotification;
      break;

    // These events are not used on Mac for now.
    case ui::AX_EVENT_ALERT:
    case ui::AX_EVENT_TEXT_CHANGED:
    case ui::AX_EVENT_CHILDREN_CHANGED:
    case ui::AX_EVENT_MENU_LIST_VALUE_CHANGED:
    case ui::AX_EVENT_SCROLL_POSITION_CHANGED:
    case ui::AX_EVENT_SCROLLED_TO_ANCHOR:
    case ui::AX_EVENT_ARIA_ATTRIBUTE_CHANGED:
    case ui::AX_EVENT_LOCATION_CHANGED:
      return;

    // Deprecated events.
    case ui::AX_EVENT_BLUR:
    case ui::AX_EVENT_HIDE:
    case ui::AX_EVENT_HOVER:
    case ui::AX_EVENT_TEXT_SELECTION_CHANGED:
    case ui::AX_EVENT_SHOW:
      return;
    default:
      DLOG(WARNING) << "Unknown accessibility event: " << event_type;
      return;
  }

  NSAccessibilityPostNotification(native_node, mac_notification);
}

void BrowserAccessibilityManagerMac::OnAccessibilityEvents(
    const std::vector<AXEventNotificationDetails>& details) {
  text_edits_.clear();
  // Call the base method last as it might delete the tree if it receives an
  // invalid message.
  BrowserAccessibilityManager::OnAccessibilityEvents(details);
}

void BrowserAccessibilityManagerMac::OnNodeDataWillChange(
    ui::AXTree* tree,
    const ui::AXNodeData& old_node_data,
    const ui::AXNodeData& new_node_data) {
  BrowserAccessibilityManager::OnNodeDataWillChange(tree, old_node_data,
                                                    new_node_data);

  // Starting from OS X 10.11, if the user has edited some text we need to
  // dispatch the actual text that changed on the value changed notification.
  // We run this code on all OS X versions to get the highest test coverage.
  base::string16 old_text, new_text;
  ui::AXRole role = new_node_data.role;
  if (role == ui::AX_ROLE_COMBO_BOX || role == ui::AX_ROLE_SEARCH_BOX ||
      role == ui::AX_ROLE_TEXT_FIELD) {
    old_text = old_node_data.GetString16Attribute(ui::AX_ATTR_VALUE);
    new_text = new_node_data.GetString16Attribute(ui::AX_ATTR_VALUE);
  } else if (new_node_data.state & (1 << ui::AX_STATE_EDITABLE)) {
    old_text = old_node_data.GetString16Attribute(ui::AX_ATTR_NAME);
    new_text = new_node_data.GetString16Attribute(ui::AX_ATTR_NAME);
  }

  if ((old_text.empty() && new_text.empty()) ||
      old_text.length() == new_text.length()) {
    return;
  }

  if (old_text.length() < new_text.length()) {
    // Insertion.
    size_t i = 0;
    while (i < old_text.length() && i < new_text.length() &&
           old_text[i] == new_text[i]) {
      ++i;
    }
    size_t length = (new_text.length() - i) - (old_text.length() - i);
    base::string16 inserted_text = new_text.substr(i, length);
    text_edits_[new_node_data.id] = inserted_text;
  } else {
    // Deletion.
    size_t i = 0;
    while (i < old_text.length() && i < new_text.length() &&
           old_text[i] == new_text[i]) {
      ++i;
    }
    size_t length = (old_text.length() - i) - (new_text.length() - i);
    base::string16 deleted_text = old_text.substr(i, length);
    text_edits_[new_node_data.id] = deleted_text;
  }
}

void BrowserAccessibilityManagerMac::OnAtomicUpdateFinished(
    ui::AXTree* tree,
    bool root_changed,
    const std::vector<ui::AXTreeDelegate::Change>& changes) {
  BrowserAccessibilityManager::OnAtomicUpdateFinished(
      tree, root_changed, changes);

  bool created_live_region = false;
  for (size_t i = 0; i < changes.size(); ++i) {
    if (changes[i].type != NODE_CREATED && changes[i].type != SUBTREE_CREATED)
      continue;

    const ui::AXNode* changed_node = changes[i].node;
    DCHECK(changed_node);
    BrowserAccessibility* obj = GetFromAXNode(changed_node);
    if (obj && obj->HasStringAttribute(ui::AX_ATTR_LIVE_STATUS)) {
      created_live_region = true;
      break;
    }
  }

  if (!created_live_region)
    return;

  // This code is to work around a bug in VoiceOver, where a new live
  // region that gets added is ignored. VoiceOver seems to only scan the
  // page for live regions once. By recreating the NSAccessibility
  // object for the root of the tree, we force VoiceOver to clear out its
  // internal state and find newly-added live regions this time.
  BrowserAccessibilityMac* root =
      static_cast<BrowserAccessibilityMac*>(GetRoot());
  if (root) {
    root->RecreateNativeObject();
    NotifyAccessibilityEvent(
        BrowserAccessibilityEvent::FromTreeChange,
        ui::AX_EVENT_CHILDREN_CHANGED,
        root);
  }
}

NSDictionary* BrowserAccessibilityManagerMac::
    GetUserInfoForSelectedTextChangedNotification() {
  NSMutableDictionary* user_info = [[[NSMutableDictionary alloc] init]
      autorelease];
  [user_info setObject:@YES forKey:NSAccessibilityTextStateSyncKey];
  [user_info setObject:@(AXTextStateChangeTypeUnknown)
                forKey:NSAccessibilityTextStateChangeTypeKey];
  [user_info setObject:@(AXTextSelectionDirectionUnknown)
                forKey:NSAccessibilityTextSelectionDirection];
  [user_info setObject:@(AXTextSelectionGranularityUnknown)
                forKey:NSAccessibilityTextSelectionGranularity];
  [user_info setObject:@YES forKey:NSAccessibilityTextSelectionChangedFocus];

  int32_t focus_id = GetTreeData().sel_focus_object_id;
  BrowserAccessibility* focus_object = GetFromID(focus_id);
  if (focus_object) {
    focus_object = focus_object->GetClosestPlatformObject();
    auto native_focus_object = ToBrowserAccessibilityCocoa(focus_object);
    if (native_focus_object && [native_focus_object instanceActive]) {
      [user_info setObject:[native_focus_object selectedTextMarkerRange]
                    forKey:NSAccessibilitySelectedTextMarkerRangeAttribute];
      [user_info setObject:native_focus_object
                    forKey:NSAccessibilityTextChangeElement];
    }
  }

  return user_info;
}

NSDictionary*
BrowserAccessibilityManagerMac::GetUserInfoForValueChangedNotification(
    const BrowserAccessibilityCocoa* native_node,
    const base::string16& deleted_text,
    const base::string16& inserted_text) const {
  DCHECK(native_node);
  if (deleted_text.empty() && inserted_text.empty())
    return nil;

  NSMutableArray* changes = [[[NSMutableArray alloc] init] autorelease];
  if (!deleted_text.empty()) {
    [changes addObject:@{
      NSAccessibilityTextEditType : @(AXTextEditTypeUnknown),
      NSAccessibilityTextChangeValueLength : @(deleted_text.length()),
      NSAccessibilityTextChangeValue : base::SysUTF16ToNSString(deleted_text)
    }];
  }
  if (!inserted_text.empty()) {
    [changes addObject:@{
      NSAccessibilityTextEditType : @(AXTextEditTypeUnknown),
      NSAccessibilityTextChangeValueLength : @(inserted_text.length()),
      NSAccessibilityTextChangeValue : base::SysUTF16ToNSString(inserted_text)
    }];
  }

  return @{
    NSAccessibilityTextStateChangeTypeKey : @(AXTextStateChangeTypeEdit),
    NSAccessibilityTextChangeValues : changes,
    NSAccessibilityTextChangeElement : native_node
  };
}

}  // namespace content
