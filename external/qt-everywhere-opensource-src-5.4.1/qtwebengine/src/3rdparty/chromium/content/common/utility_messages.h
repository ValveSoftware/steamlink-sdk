// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, so no include guard.

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "content/common/content_export.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/webplugininfo.h"
#include "ipc/ipc_message_macros.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START UtilityMsgStart

//------------------------------------------------------------------------------
// Utility process messages:
// These are messages from the browser to the utility process.

// Tells the utility process that it's running in batch mode.
IPC_MESSAGE_CONTROL0(UtilityMsg_BatchMode_Started)

// Tells the utility process that it can shutdown.
IPC_MESSAGE_CONTROL0(UtilityMsg_BatchMode_Finished)

#if defined(OS_POSIX)
// Tells the utility process to load each plugin in the order specified by the
// vector. It will respond after each load with the WebPluginInfo.
IPC_MESSAGE_CONTROL1(UtilityMsg_LoadPlugins,
                     std::vector<base::FilePath> /* plugin paths */)
#endif

//------------------------------------------------------------------------------
// Utility process host messages:
// These are messages from the utility process to the browser.

#if defined(OS_POSIX)
// Notifies the browser when a plugin failed to load so the two processes can
// keep the canonical list in sync.
IPC_SYNC_MESSAGE_CONTROL2_0(UtilityHostMsg_LoadPluginFailed,
                            uint32_t /* index in the vector */,
                            base::FilePath /* path of plugin */)

// Notifies the browser that a plugin in the vector sent by it has been loaded.
IPC_SYNC_MESSAGE_CONTROL2_0(UtilityHostMsg_LoadedPlugin,
                            uint32_t /* index in the vector */,
                            content::WebPluginInfo /* plugin info */)
#endif  // OS_POSIX
