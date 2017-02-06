// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included file, no traditional include guard.
#include "base/strings/string16.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "ipc/ipc_message_macros.h"
#include "url/gurl.h"
#include "url/ipc/url_param_traits.h"

#define IPC_MESSAGE_START ContentSettingsMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(ContentSettingsType,
                          CONTENT_SETTINGS_NUM_TYPES_DO_NOT_USE - 1)

//-----------------------------------------------------------------------------
// These are messages sent from the browser to the renderer process.

// Sent in response to FrameHostMsg_DidBlockDisplayingInsecureContent.
IPC_MESSAGE_ROUTED1(ChromeViewMsg_SetAllowDisplayingInsecureContent,
                    bool /* allowed */)

// Sent in response to FrameHostMsg_DidBlockRunningInsecureContent.
IPC_MESSAGE_ROUTED1(ChromeViewMsg_SetAllowRunningInsecureContent,
                    bool /* allowed */)

IPC_MESSAGE_ROUTED0(ChromeViewMsg_ReloadFrame)

// Tells the renderer whether or not a file system access has been allowed.
IPC_MESSAGE_ROUTED2(ChromeViewMsg_RequestFileSystemAccessAsyncResponse,
                    int  /* request_id */,
                    bool /* allowed */)

// Tells the render frame to load all blocked plugins with the given identifier.
IPC_MESSAGE_ROUTED1(ChromeViewMsg_LoadBlockedPlugins,
                    std::string /* identifier */)

// JavaScript related messages -----------------------------------------------

// Tells the frame it is displaying an interstitial page.
IPC_MESSAGE_ROUTED0(ChromeViewMsg_SetAsInterstitial)

//-----------------------------------------------------------------------------
// These are messages sent from the renderer to the browser process.

// Tells the browser that content in the current page was blocked due to the
// user's content settings.
IPC_MESSAGE_ROUTED2(ChromeViewHostMsg_ContentBlocked,
                    ContentSettingsType /* type of blocked content */,
                    base::string16 /* details on blocked content */)

// Sent by the renderer process to check whether access to web databases is
// granted by content settings.
IPC_SYNC_MESSAGE_CONTROL5_1(ChromeViewHostMsg_AllowDatabase,
                            int /* render_frame_id */,
                            GURL /* origin_url */,
                            GURL /* top origin url */,
                            base::string16 /* database name */,
                            base::string16 /* database display name */,
                            bool /* allowed */)

// Sent by the renderer process to check whether access to DOM Storage is
// granted by content settings.
IPC_SYNC_MESSAGE_CONTROL4_1(ChromeViewHostMsg_AllowDOMStorage,
                            int /* render_frame_id */,
                            GURL /* origin_url */,
                            GURL /* top origin url */,
                            bool /* if true local storage, otherwise session */,
                            bool /* allowed */)

// Sent by the renderer process when a keygen element is rendered onto the
// current page.
IPC_MESSAGE_ROUTED1(ChromeViewHostMsg_DidUseKeygen,
                    GURL /* origin_url */)

// Sent by the renderer process to check whether access to FileSystem is
// granted by content settings.
IPC_MESSAGE_CONTROL4(ChromeViewHostMsg_RequestFileSystemAccessAsync,
                    int /* render_frame_id */,
                    int /* request_id */,
                    GURL /* origin_url */,
                    GURL /* top origin url */)

// Sent by the renderer process to check whether access to Indexed DBis
// granted by content settings.
IPC_SYNC_MESSAGE_CONTROL4_1(ChromeViewHostMsg_AllowIndexedDB,
                            int /* render_frame_id */,
                            GURL /* origin_url */,
                            GURL /* top origin url */,
                            base::string16 /* database name */,
                            bool /* allowed */)

// Sent when the renderer was prevented from displaying insecure content in
// a secure page by a security policy.  The page may appear incomplete.
IPC_MESSAGE_ROUTED0(ChromeViewHostMsg_DidBlockDisplayingInsecureContent)
