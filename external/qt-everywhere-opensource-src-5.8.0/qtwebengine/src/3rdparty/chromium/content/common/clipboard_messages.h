// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, so no include guard.

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "base/memory/shared_memory.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "content/common/clipboard_format.h"
#include "content/common/content_export.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/param_traits_macros.h"
#include "ui/base/clipboard/clipboard.h"
#include "url/ipc/url_param_traits.h"

// Singly-included section for types and/or struct declarations.
#ifndef CONTENT_COMMON_CLIPBOARD_MESSAGES_H_
#define CONTENT_COMMON_CLIPBOARD_MESSAGES_H_

// Custom data consists of arbitrary MIME types an untrusted sender wants to
// write to the clipboard. Note that exposing a general interface to do this is
// dangerous--an untrusted sender could cause a DoS or code execution.
typedef std::map<base::string16, base::string16> CustomDataMap;

#endif  // CONTENT_COMMON_CLIPBOARD_MESSAGES_H_

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START ClipboardMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(content::ClipboardFormat,
                          content::CLIPBOARD_FORMAT_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(ui::ClipboardType, ui::CLIPBOARD_TYPE_LAST)

// Clipboard IPC messages sent from the renderer to the browser.

IPC_SYNC_MESSAGE_CONTROL1_1(ClipboardHostMsg_GetSequenceNumber,
                            ui::ClipboardType /* type */,
                            uint64_t /* result */)
IPC_SYNC_MESSAGE_CONTROL2_1(ClipboardHostMsg_IsFormatAvailable,
                            content::ClipboardFormat /* format */,
                            ui::ClipboardType /* type */,
                            bool /* result */)
IPC_MESSAGE_CONTROL1(ClipboardHostMsg_Clear,
                     ui::ClipboardType /* type */)
IPC_SYNC_MESSAGE_CONTROL1_2(ClipboardHostMsg_ReadAvailableTypes,
                            ui::ClipboardType /* type */,
                            std::vector<base::string16> /* types */,
                            bool /* contains filenames */)
IPC_SYNC_MESSAGE_CONTROL1_1(ClipboardHostMsg_ReadText,
                            ui::ClipboardType /* type */,
                            base::string16 /* result */)
IPC_SYNC_MESSAGE_CONTROL1_4(ClipboardHostMsg_ReadHTML,
                            ui::ClipboardType /* type */,
                            base::string16 /* markup */,
                            GURL /* url */,
                            uint32_t /* fragment start */,
                            uint32_t /* fragment end */)
IPC_SYNC_MESSAGE_CONTROL1_1(ClipboardHostMsg_ReadRTF,
                            ui::ClipboardType /* type */,
                            std::string /* result */)
IPC_SYNC_MESSAGE_CONTROL1_3(ClipboardHostMsg_ReadImage,
                            ui::ClipboardType /* type */,
                            std::string /* blob_uuid */,
                            std::string /* mime_type */,
                            int64_t /* size */)
IPC_SYNC_MESSAGE_CONTROL2_1(ClipboardHostMsg_ReadCustomData,
                            ui::ClipboardType /* type */,
                            base::string16 /* type */,
                            base::string16 /* result */)

// Writing to the clipboard via IPC is a two-phase operation. First, the sender
// sends the different types of data it'd like to write to the receiver. Then,
// it sends a commit message to commit the data to the system clipboard.
IPC_MESSAGE_CONTROL2(ClipboardHostMsg_WriteText,
                     ui::ClipboardType /* type */,
                     base::string16 /* text */)
IPC_MESSAGE_CONTROL3(ClipboardHostMsg_WriteHTML,
                     ui::ClipboardType /* type */,
                     base::string16 /* markup */,
                     GURL /* url */)
IPC_MESSAGE_CONTROL1(ClipboardHostMsg_WriteSmartPasteMarker,
                     ui::ClipboardType /* type */)
IPC_MESSAGE_CONTROL2(ClipboardHostMsg_WriteCustomData,
                     ui::ClipboardType /* type */,
                     CustomDataMap /* custom data */)
// TODO(dcheng): The |url| parameter should really be a GURL, but <canvas>'s
// copy as image tries to set very long data: URLs on the clipboard. Using
// GURL causes the browser to kill the renderer for sending a bad IPC (GURLs
// bigger than 2 megabytes are considered to be bad). https://crbug.com/459822
IPC_MESSAGE_CONTROL3(ClipboardHostMsg_WriteBookmark,
                     ui::ClipboardType /* type */,
                     std::string /* url */,
                     base::string16 /* title */)
IPC_SYNC_MESSAGE_CONTROL3_0(ClipboardHostMsg_WriteImage,
                            ui::ClipboardType /* type */,
                            gfx::Size /* size */,
                            base::SharedMemoryHandle /* bitmap handle */)
IPC_MESSAGE_CONTROL1(ClipboardHostMsg_CommitWrite, ui::ClipboardType /* type */)

#if defined(OS_MACOSX)
IPC_MESSAGE_CONTROL1(ClipboardHostMsg_FindPboardWriteStringAsync,
                     base::string16 /* text */)
#endif
