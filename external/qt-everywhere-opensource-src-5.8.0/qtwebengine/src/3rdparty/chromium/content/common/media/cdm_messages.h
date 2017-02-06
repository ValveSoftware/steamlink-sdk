// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for content decryption module (CDM) implementation.
// Multiply-included message file, hence no include guard.

#include <stdint.h>

#include <string>
#include <vector>

#include "content/common/content_export.h"
#include "content/common/media/cdm_messages_enums.h"
#include "ipc/ipc_message_macros.h"
#include "media/base/cdm_key_information.h"
#include "media/base/media_keys.h"
#include "url/gurl.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START CdmMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(media::CdmKeyInformation::KeyStatus,
                          media::CdmKeyInformation::KEY_STATUS_MAX)
IPC_ENUM_TRAITS_MAX_VALUE(media::MediaKeys::Exception,
                          media::MediaKeys::EXCEPTION_MAX)
IPC_ENUM_TRAITS_MAX_VALUE(media::MediaKeys::SessionType,
                          media::MediaKeys::SESSION_TYPE_MAX)
IPC_ENUM_TRAITS_MAX_VALUE(media::MediaKeys::MessageType,
                          media::MediaKeys::MESSAGE_TYPE_MAX)
IPC_ENUM_TRAITS_MAX_VALUE(CdmHostMsg_CreateSession_InitDataType,
                          INIT_DATA_TYPE_MAX)

IPC_STRUCT_BEGIN(CdmHostMsg_InitializeCdm_Params)
  IPC_STRUCT_MEMBER(std::string, key_system)
  IPC_STRUCT_MEMBER(GURL, security_origin)
  IPC_STRUCT_MEMBER(bool, use_hw_secure_codecs)
IPC_STRUCT_END()

IPC_STRUCT_TRAITS_BEGIN(media::CdmKeyInformation)
  IPC_STRUCT_TRAITS_MEMBER(key_id)
  IPC_STRUCT_TRAITS_MEMBER(status)
  IPC_STRUCT_TRAITS_MEMBER(system_code)
IPC_STRUCT_TRAITS_END()

// Parameter structure for CdmHostMsg_CreateSessionAndGenerateRequest.
IPC_STRUCT_BEGIN(CdmHostMsg_CreateSessionAndGenerateRequest_Params)
  IPC_STRUCT_MEMBER(int, render_frame_id)
  IPC_STRUCT_MEMBER(int, cdm_id)
  IPC_STRUCT_MEMBER(uint32_t, promise_id)
  IPC_STRUCT_MEMBER(media::MediaKeys::SessionType, session_type)
  IPC_STRUCT_MEMBER(CdmHostMsg_CreateSession_InitDataType, init_data_type)
  IPC_STRUCT_MEMBER(std::vector<uint8_t>, init_data)
IPC_STRUCT_END()


// Messages from render to browser.

IPC_MESSAGE_CONTROL4(CdmHostMsg_InitializeCdm,
                     int /* render_frame_id */,
                     int /* cdm_id */,
                     uint32_t /* promise_id */,
                     CdmHostMsg_InitializeCdm_Params /* params */)

IPC_MESSAGE_CONTROL4(CdmHostMsg_SetServerCertificate,
                     int /* render_frame_id */,
                     int /* cdm_id */,
                     uint32_t /* promise_id */,
                     std::vector<uint8_t> /* certificate */)

IPC_MESSAGE_CONTROL1(CdmHostMsg_CreateSessionAndGenerateRequest,
                     CdmHostMsg_CreateSessionAndGenerateRequest_Params)

IPC_MESSAGE_CONTROL5(CdmHostMsg_LoadSession,
                     int /* render_frame_id */,
                     int /* cdm_id */,
                     uint32_t /* promise_id */,
                     media::MediaKeys::SessionType /* session_type */,
                     std::string /* session_id */)

IPC_MESSAGE_CONTROL5(CdmHostMsg_UpdateSession,
                     int /* render_frame_id */,
                     int /* cdm_id */,
                     uint32_t /* promise_id */,
                     std::string /* session_id */,
                     std::vector<uint8_t> /* response */)

IPC_MESSAGE_CONTROL4(CdmHostMsg_CloseSession,
                     int /* render_frame_id */,
                     int /* cdm_id */,
                     uint32_t /* promise_id */,
                     std::string /* session_id */)

IPC_MESSAGE_CONTROL4(CdmHostMsg_RemoveSession,
                     int /* render_frame_id */,
                     int /* cdm_id */,
                     uint32_t /* promise_id */,
                     std::string /* session_id */)

IPC_MESSAGE_CONTROL2(CdmHostMsg_DestroyCdm,
                     int /* render_frame_id */,
                     int /* cdm_id */)

// Messages from browser to render.

IPC_MESSAGE_ROUTED5(CdmMsg_SessionMessage,
                    int /* cdm_id */,
                    std::string /* session_id */,
                    media::MediaKeys::MessageType /* message_type */,
                    std::vector<uint8_t> /* message */,
                    GURL /* legacy_destination_url */)

IPC_MESSAGE_ROUTED2(CdmMsg_SessionClosed,
                    int /* cdm_id */,
                    std::string /* session_id */)

IPC_MESSAGE_ROUTED5(CdmMsg_LegacySessionError,
                    int /* cdm_id */,
                    std::string /* session_id */,
                    media::MediaKeys::Exception /* exception_code */,
                    uint32_t /* system_code */,
                    std::string /* error_message */)

IPC_MESSAGE_ROUTED4(CdmMsg_SessionKeysChange,
                    int /* cdm_id */,
                    std::string /* session_id */,
                    bool /* has_additional_usable_key */,
                    std::vector<media::CdmKeyInformation> /* keys_info */)

IPC_MESSAGE_ROUTED3(CdmMsg_SessionExpirationUpdate,
                    int /* cdm_id */,
                    std::string /* session_id */,
                    base::Time /* new_expiry_time */)

IPC_MESSAGE_ROUTED2(CdmMsg_ResolvePromise,
                    int /* cdm_id */,
                    uint32_t /* promise_id */)

IPC_MESSAGE_ROUTED3(CdmMsg_ResolvePromiseWithSession,
                    int /* cdm_id */,
                    uint32_t /* promise_id */,
                    std::string /* session_id */)

IPC_MESSAGE_ROUTED5(CdmMsg_RejectPromise,
                    int /* cdm_id */,
                    uint32_t /* promise_id */,
                    media::MediaKeys::Exception /* exception */,
                    uint32_t /* system_code */,
                    std::string /* error_message */)
