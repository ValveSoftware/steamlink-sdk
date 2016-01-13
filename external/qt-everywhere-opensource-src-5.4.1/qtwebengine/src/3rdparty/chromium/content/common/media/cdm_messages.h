// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for content decryption module (CDM) implementation.
// Multiply-included message file, hence no include guard.

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "content/common/content_export.h"
#include "content/common/media/cdm_messages_enums.h"
#include "ipc/ipc_message_macros.h"
#include "media/base/media_keys.h"
#include "url/gurl.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START CdmMsgStart

IPC_ENUM_TRAITS(media::MediaKeys::KeyError)
IPC_ENUM_TRAITS(CdmHostMsg_CreateSession_ContentType)

IPC_MESSAGE_ROUTED3(CdmHostMsg_InitializeCdm,
                    int /* cdm_id */,
                    std::string /* key_system */,
                    GURL /* security_origin */)

IPC_MESSAGE_ROUTED4(CdmHostMsg_CreateSession,
                    int /* cdm_id */,
                    uint32_t /* session_id */,
                    CdmHostMsg_CreateSession_ContentType /* content_type */,
                    std::vector<uint8> /* init_data */)

IPC_MESSAGE_ROUTED3(CdmHostMsg_UpdateSession,
                    int /* cdm_id */,
                    uint32_t /* session_id */,
                    std::vector<uint8> /* response */)

IPC_MESSAGE_ROUTED2(CdmHostMsg_ReleaseSession,
                    int /* cdm_id */,
                    uint32_t /* session_id */)

IPC_MESSAGE_ROUTED1(CdmHostMsg_DestroyCdm, int /* cdm_id */)

IPC_MESSAGE_ROUTED3(CdmMsg_SessionCreated,
                    int /* cdm_id */,
                    uint32_t /* session_id */,
                    std::string /* web_session_id */)

IPC_MESSAGE_ROUTED4(CdmMsg_SessionMessage,
                    int /* cdm_id */,
                    uint32_t /* session_id */,
                    std::vector<uint8> /* message */,
                    GURL /* destination_url */)

IPC_MESSAGE_ROUTED2(CdmMsg_SessionReady,
                    int /* cdm_id */,
                    uint32_t /* session_id */)

IPC_MESSAGE_ROUTED2(CdmMsg_SessionClosed,
                    int /* cdm_id */,
                    uint32_t /* session_id */)

IPC_MESSAGE_ROUTED4(CdmMsg_SessionError,
                    int /* cdm_id */,
                    uint32_t /* session_id */,
                    media::MediaKeys::KeyError /* error_code */,
                    uint32_t /* system_code */)
