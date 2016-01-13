// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included file, no traditional include guard.
#include "ipc/ipc_message_macros.h"

// Singly-included section for enums and custom IPC traits.
#ifndef RENDER_VIEW_MESSAGES_H
#define RENDER_VIEW_MESSAGES_H

namespace IPC {

// TODO - add enums and custom IPC traits here when needed.

}  // namespace IPC

#endif  // RENDER_VIEW_MESSAGES_H

#define IPC_MESSAGE_START QtMsgStart

//-----------------------------------------------------------------------------
// RenderView messages
// These are messages sent from the browser to the renderer process.

IPC_MESSAGE_ROUTED1(QtRenderViewObserver_FetchDocumentMarkup,
                    uint64 /* requestId */)

IPC_MESSAGE_ROUTED1(QtRenderViewObserver_FetchDocumentInnerText,
                    uint64 /* requestId */)

//-----------------------------------------------------------------------------
// WebContents messages
// These are messages sent from the renderer back to the browser process.

IPC_MESSAGE_ROUTED2(QtRenderViewObserverHost_DidFetchDocumentMarkup,
                    uint64 /* requestId */,
                    base::string16 /* markup */)

IPC_MESSAGE_ROUTED2(QtRenderViewObserverHost_DidFetchDocumentInnerText,
                    uint64 /* requestId */,
                    base::string16 /* innerText */)

IPC_MESSAGE_ROUTED0(QtRenderViewObserverHost_DidFirstVisuallyNonEmptyLayout)
