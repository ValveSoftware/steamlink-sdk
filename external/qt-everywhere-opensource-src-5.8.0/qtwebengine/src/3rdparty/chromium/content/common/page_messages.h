// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/page_message_enums.h"
#include "ipc/ipc_message_macros.h"
#include "ui/gfx/geometry/rect.h"

// IPC messages for page-level actions.
// Multiply-included message file, hence no include guard.

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START PageMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(
    PageMsg_SetZoomLevel_Command,
    PageMsg_SetZoomLevel_Command::LAST)

// Messages sent from the browser to the renderer.

IPC_MESSAGE_ROUTED1(PageMsg_UpdateWindowScreenRect,
                    gfx::Rect /* window_screen_rect */)

IPC_MESSAGE_ROUTED2(PageMsg_SetZoomLevel,
                    PageMsg_SetZoomLevel_Command /* command */,
                    double /* zoom_level */)

// Informs the renderer that the page was hidden.
IPC_MESSAGE_ROUTED0(PageMsg_WasHidden)

// Informs the renderer that the page is no longer hidden.
IPC_MESSAGE_ROUTED0(PageMsg_WasShown)

// -----------------------------------------------------------------------------
// Messages sent from the renderer to the browser.

// Adding a new message? Stick to the sort order above: first platform
// independent PageMsg, then ifdefs for platform specific PageMsg, then platform
// independent PageHostMsg, then ifdefs for platform specific PageHostMsg.
