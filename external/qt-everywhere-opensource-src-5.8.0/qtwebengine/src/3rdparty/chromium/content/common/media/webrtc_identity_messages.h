// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for requesting WebRTC identity.
// Multiply-included message file, hence no include guard.

#include "content/common/content_export.h"
#include "ipc/ipc_message_macros.h"
#include "url/gurl.h"

#define IPC_MESSAGE_START WebRTCIdentityMsgStart
#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

IPC_STRUCT_BEGIN(WebRTCIdentityMsg_RequestIdentity_Params)
  IPC_STRUCT_MEMBER(int, request_id)
  IPC_STRUCT_MEMBER(GURL, url)
  IPC_STRUCT_MEMBER(GURL, first_party_for_cookies)
  IPC_STRUCT_MEMBER(std::string, identity_name)
  IPC_STRUCT_MEMBER(std::string, common_name)
IPC_STRUCT_END()

// Messages sent from the renderer to the browser.
// Request a WebRTC identity.
IPC_MESSAGE_CONTROL1(WebRTCIdentityMsg_RequestIdentity,
                     WebRTCIdentityMsg_RequestIdentity_Params)

// Cancel the WebRTC identity request.
IPC_MESSAGE_CONTROL0(WebRTCIdentityMsg_CancelRequest)

// Messages sent from the browser to the renderer.
// Return a WebRTC identity.
IPC_MESSAGE_CONTROL3(WebRTCIdentityHostMsg_IdentityReady,
                     int /* request_id */,
                     std::string /* certificate */,
                     std::string /* private_key */)
// Notifies an error from the identity request.
IPC_MESSAGE_CONTROL2(WebRTCIdentityHostMsg_RequestFailed,
                     int /* request_id */,
                     int /* error */)
