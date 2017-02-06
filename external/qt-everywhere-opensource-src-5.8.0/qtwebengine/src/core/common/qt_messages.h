// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included file, no traditional include guard.

#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"

#include "user_script_data.h"

IPC_STRUCT_TRAITS_BEGIN(UserScriptData)
    IPC_STRUCT_TRAITS_MEMBER(source)
    IPC_STRUCT_TRAITS_MEMBER(url)
    IPC_STRUCT_TRAITS_MEMBER(injectionPoint)
    IPC_STRUCT_TRAITS_MEMBER(injectForSubframes)
    IPC_STRUCT_TRAITS_MEMBER(worldId)
    IPC_STRUCT_TRAITS_MEMBER(scriptId)
    IPC_STRUCT_TRAITS_MEMBER(globs)
    IPC_STRUCT_TRAITS_MEMBER(excludeGlobs)
    IPC_STRUCT_TRAITS_MEMBER(urlPatterns)
IPC_STRUCT_TRAITS_END()


#define IPC_MESSAGE_START QtMsgStart

//-----------------------------------------------------------------------------
// RenderView messages
// These are messages sent from the browser to the renderer process.

IPC_MESSAGE_ROUTED1(RenderViewObserverQt_FetchDocumentMarkup,
                    uint64_t /* requestId */)

IPC_MESSAGE_ROUTED1(RenderViewObserverQt_FetchDocumentInnerText,
                    uint64_t /* requestId */)

IPC_MESSAGE_ROUTED1(RenderViewObserverQt_SetBackgroundColor,
                    uint32_t /* color */)

IPC_MESSAGE_ROUTED1(WebChannelIPCTransport_Install, uint /* worldId */)
IPC_MESSAGE_ROUTED1(WebChannelIPCTransport_Uninstall, uint /* worldId */)
IPC_MESSAGE_ROUTED2(WebChannelIPCTransport_Message, std::vector<char> /*binaryJSON*/, uint /* worldId */)

// User scripts messages
IPC_MESSAGE_ROUTED1(RenderViewObserverHelper_AddScript,
                    UserScriptData /* script */)
IPC_MESSAGE_ROUTED1(RenderViewObserverHelper_RemoveScript,
                    UserScriptData /* script */)
IPC_MESSAGE_ROUTED0(RenderViewObserverHelper_ClearScripts)

IPC_MESSAGE_CONTROL1(UserResourceController_AddScript, UserScriptData /* scriptContents */)
IPC_MESSAGE_CONTROL1(UserResourceController_RemoveScript, UserScriptData /* scriptContents */)
IPC_MESSAGE_CONTROL0(UserResourceController_ClearScripts)

//-----------------------------------------------------------------------------
// WebContents messages
// These are messages sent from the renderer back to the browser process.

IPC_MESSAGE_ROUTED2(RenderViewObserverHostQt_DidFetchDocumentMarkup,
                    uint64_t /* requestId */,
                    base::string16 /* markup */)

IPC_MESSAGE_ROUTED2(RenderViewObserverHostQt_DidFetchDocumentInnerText,
                    uint64_t /* requestId */,
                    base::string16 /* innerText */)

IPC_MESSAGE_ROUTED0(RenderViewObserverHostQt_DidFirstVisuallyNonEmptyLayout)

IPC_MESSAGE_ROUTED1(WebChannelIPCTransportHost_SendMessage, std::vector<char> /*binaryJSON*/)

//-----------------------------------------------------------------------------
// Misc messages
// These are messages sent from the renderer to the browser process.

#if defined(ENABLE_PEPPER_CDMS)
// Returns whether any internal plugin supporting |mime_type| is registered and
// enabled. Does not determine whether the plugin can actually be instantiated
// (e.g. whether it has all its dependencies).
// When the returned *|is_available| is true, |additional_param_names| and
// |additional_param_values| contain the name-value pairs, if any, specified
// for the *first* non-disabled plugin found that is registered for |mime_type|.
IPC_SYNC_MESSAGE_CONTROL1_3(QtWebEngineHostMsg_IsInternalPluginAvailableForMimeType,
                            std::string /* mime_type */,
                            bool /* is_available */,
                            std::vector<base::string16> /* additional_param_names */,
                            std::vector<base::string16> /* additional_param_values */)
#endif
