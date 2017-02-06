// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for drag and drop.
// Multiply-included message file, hence no include guard.

#include <vector>

#include "content/common/drag_event_source_info.h"
#include "content/public/common/drop_data.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/public/platform/WebDragOperation.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/vector2d.h"

#define IPC_MESSAGE_START DragMsgStart

// Messages sent from the browser to the renderer.

IPC_MESSAGE_ROUTED5(DragMsg_TargetDragEnter,
                    std::vector<content::DropData::Metadata> /* drop_data */,
                    gfx::Point /* client_pt */,
                    gfx::Point /* screen_pt */,
                    blink::WebDragOperationsMask /* ops_allowed */,
                    int /* key_modifiers */)

IPC_MESSAGE_ROUTED4(DragMsg_TargetDragOver,
                    gfx::Point /* client_pt */,
                    gfx::Point /* screen_pt */,
                    blink::WebDragOperationsMask /* ops_allowed */,
                    int /* key_modifiers */)

IPC_MESSAGE_ROUTED0(DragMsg_TargetDragLeave)

IPC_MESSAGE_ROUTED4(DragMsg_TargetDrop,
                    content::DropData /* drop_data */,
                    gfx::Point /* client_pt */,
                    gfx::Point /* screen_pt */,
                    int /* key_modifiers */)

// Notifies the renderer when and where the mouse-drag ended.
IPC_MESSAGE_ROUTED3(DragMsg_SourceEnded,
                    gfx::Point /* client_pt */,
                    gfx::Point /* screen_pt */,
                    blink::WebDragOperation /* drag_operation */)

// Notifies the renderer that the system DoDragDrop call has ended.
IPC_MESSAGE_ROUTED0(DragMsg_SourceSystemDragEnded)

// Messages sent from the renderer to the browser.

// Used to tell the parent the user started dragging in the content area. The
// DropData struct contains contextual information about the pieces of the
// page the user dragged. The parent uses this notification to initiate a
// drag session at the OS level.
IPC_MESSAGE_ROUTED5(DragHostMsg_StartDragging,
                    content::DropData /* drop_data */,
                    blink::WebDragOperationsMask /* ops_allowed */,
                    SkBitmap /* image */,
                    gfx::Vector2d /* image_offset */,
                    content::DragEventSourceInfo /* event_info */)

// The page wants to update the mouse cursor during a drag & drop operation.
// |is_drop_target| is true if the mouse is over a valid drop target.
IPC_MESSAGE_ROUTED1(DragHostMsg_UpdateDragCursor,
                    blink::WebDragOperation /* drag_operation */)
