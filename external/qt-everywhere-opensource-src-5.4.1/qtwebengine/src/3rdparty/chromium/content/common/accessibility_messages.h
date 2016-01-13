// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for accessibility.
// Multiply-included message file, hence no include guard.

#include "base/basictypes.h"
#include "content/common/content_export.h"
#include "content/common/view_message_enums.h"
#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_param_traits.h"
#include "ipc/param_traits_macros.h"
#include "third_party/WebKit/public/web/WebAXEnums.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_update.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START AccessibilityMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(ui::AXEvent, ui::AX_EVENT_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(ui::AXRole, ui::AX_ROLE_LAST)

IPC_ENUM_TRAITS_MAX_VALUE(ui::AXBoolAttribute, ui::AX_BOOL_ATTRIBUTE_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(ui::AXFloatAttribute, ui::AX_FLOAT_ATTRIBUTE_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(ui::AXIntAttribute, ui::AX_INT_ATTRIBUTE_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(ui::AXIntListAttribute,
                          ui::AX_INT_LIST_ATTRIBUTE_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(ui::AXStringAttribute, ui::AX_STRING_ATTRIBUTE_LAST)

IPC_STRUCT_TRAITS_BEGIN(ui::AXNodeData)
  IPC_STRUCT_TRAITS_MEMBER(id)
  IPC_STRUCT_TRAITS_MEMBER(role)
  IPC_STRUCT_TRAITS_MEMBER(state)
  IPC_STRUCT_TRAITS_MEMBER(location)
  IPC_STRUCT_TRAITS_MEMBER(string_attributes)
  IPC_STRUCT_TRAITS_MEMBER(int_attributes)
  IPC_STRUCT_TRAITS_MEMBER(float_attributes)
  IPC_STRUCT_TRAITS_MEMBER(bool_attributes)
  IPC_STRUCT_TRAITS_MEMBER(intlist_attributes)
  IPC_STRUCT_TRAITS_MEMBER(html_attributes)
  IPC_STRUCT_TRAITS_MEMBER(child_ids)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ui::AXTreeUpdate)
  IPC_STRUCT_TRAITS_MEMBER(node_id_to_clear)
  IPC_STRUCT_TRAITS_MEMBER(nodes)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_BEGIN(AccessibilityHostMsg_EventParams)
  // The tree update.
  IPC_STRUCT_MEMBER(ui::AXTreeUpdate, update)

  // Type of event.
  IPC_STRUCT_MEMBER(ui::AXEvent, event_type)

  // ID of the node that the event applies to.
  IPC_STRUCT_MEMBER(int, id)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(AccessibilityHostMsg_LocationChangeParams)
  // ID of the object whose location is changing.
  IPC_STRUCT_MEMBER(int, id)

  // The object's new location, in frame-relative coordinates (same
  // as the coordinates in AccessibilityNodeData).
  IPC_STRUCT_MEMBER(gfx::Rect, new_location)
IPC_STRUCT_END()

// Messages sent from the browser to the renderer.

// Relay a request from assistive technology to set focus to a given node.
IPC_MESSAGE_ROUTED1(AccessibilityMsg_SetFocus,
                    int /* object id */)

// Relay a request from assistive technology to perform the default action
// on a given node.
IPC_MESSAGE_ROUTED1(AccessibilityMsg_DoDefaultAction,
                    int /* object id */)

// Relay a request from assistive technology to make a given object
// visible by scrolling as many scrollable containers as possible.
// In addition, if it's not possible to make the entire object visible,
// scroll so that the |subfocus| rect is visible at least. The subfocus
// rect is in local coordinates of the object itself.
IPC_MESSAGE_ROUTED2(AccessibilityMsg_ScrollToMakeVisible,
                    int /* object id */,
                    gfx::Rect /* subfocus */)

// Relay a request from assistive technology to move a given object
// to a specific location, in the WebContents area coordinate space, i.e.
// (0, 0) is the top-left corner of the WebContents.
IPC_MESSAGE_ROUTED2(AccessibilityMsg_ScrollToPoint,
                    int /* object id */,
                    gfx::Point /* new location */)

// Relay a request from assistive technology to set the cursor or
// selection within an editable text element.
IPC_MESSAGE_ROUTED3(AccessibilityMsg_SetTextSelection,
                    int /* object id */,
                    int /* New start offset */,
                    int /* New end offset */)

// Determine the accessibility object under a given point and reply with
// a AccessibilityHostMsg_HitTestResult with the same id.
IPC_MESSAGE_ROUTED1(AccessibilityMsg_HitTest,
                    gfx::Point /* location to test */);

// Tells the render view that a AccessibilityHostMsg_Events
// message was processed and it can send addition events.
IPC_MESSAGE_ROUTED0(AccessibilityMsg_Events_ACK)

// Kill the renderer because we got a fatal error in the accessibility tree.
IPC_MESSAGE_ROUTED0(AccessibilityMsg_FatalError)

// Messages sent from the renderer to the browser.

// Sent to notify the browser about renderer accessibility events.
// The browser responds with a AccessibilityMsg_Events_ACK.
IPC_MESSAGE_ROUTED1(
    AccessibilityHostMsg_Events,
    std::vector<AccessibilityHostMsg_EventParams>)

// Sent to update the browser of the location of accessibility objects.
IPC_MESSAGE_ROUTED1(
    AccessibilityHostMsg_LocationChanges,
    std::vector<AccessibilityHostMsg_LocationChangeParams>)
