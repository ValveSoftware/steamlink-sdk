// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// From private/ppb_content_decryptor_private.idl modified Mon Mar 30 22:35:33
// 2015.

#include <stdint.h>

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_content_decryptor_private.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {
namespace thunk {

namespace {

void PromiseResolved(PP_Instance instance, uint32_t promise_id) {
  VLOG(4) << "PPB_ContentDecryptor_Private::PromiseResolved()";
  EnterInstance enter(instance);
  if (enter.failed())
    return;
  enter.functions()->PromiseResolved(instance, promise_id);
}

void PromiseResolvedWithSession(PP_Instance instance,
                                uint32_t promise_id,
                                struct PP_Var session_id) {
  VLOG(4) << "PPB_ContentDecryptor_Private::PromiseResolvedWithSession()";
  EnterInstance enter(instance);
  if (enter.failed())
    return;
  enter.functions()->PromiseResolvedWithSession(instance, promise_id,
                                                session_id);
}

void PromiseRejected(PP_Instance instance,
                     uint32_t promise_id,
                     PP_CdmExceptionCode exception_code,
                     uint32_t system_code,
                     struct PP_Var error_description) {
  VLOG(4) << "PPB_ContentDecryptor_Private::PromiseRejected()";
  EnterInstance enter(instance);
  if (enter.failed())
    return;
  enter.functions()->PromiseRejected(instance, promise_id, exception_code,
                                     system_code, error_description);
}

void SessionMessage(PP_Instance instance,
                    struct PP_Var session_id,
                    PP_CdmMessageType message_type,
                    struct PP_Var message,
                    struct PP_Var legacy_destination_url) {
  VLOG(4) << "PPB_ContentDecryptor_Private::SessionMessage()";
  EnterInstance enter(instance);
  if (enter.failed())
    return;
  enter.functions()->SessionMessage(instance, session_id, message_type, message,
                                    legacy_destination_url);
}

void SessionKeysChange(PP_Instance instance,
                       struct PP_Var session_id,
                       PP_Bool has_additional_usable_key,
                       uint32_t key_count,
                       const struct PP_KeyInformation key_information[]) {
  VLOG(4) << "PPB_ContentDecryptor_Private::SessionKeysChange()";
  EnterInstance enter(instance);
  if (enter.failed())
    return;
  enter.functions()->SessionKeysChange(instance, session_id,
                                       has_additional_usable_key, key_count,
                                       key_information);
}

void SessionExpirationChange(PP_Instance instance,
                             struct PP_Var session_id,
                             PP_Time new_expiry_time) {
  VLOG(4) << "PPB_ContentDecryptor_Private::SessionExpirationChange()";
  EnterInstance enter(instance);
  if (enter.failed())
    return;
  enter.functions()->SessionExpirationChange(instance, session_id,
                                             new_expiry_time);
}

void SessionClosed(PP_Instance instance, struct PP_Var session_id) {
  VLOG(4) << "PPB_ContentDecryptor_Private::SessionClosed()";
  EnterInstance enter(instance);
  if (enter.failed())
    return;
  enter.functions()->SessionClosed(instance, session_id);
}

void LegacySessionError(PP_Instance instance,
                        struct PP_Var session_id,
                        PP_CdmExceptionCode exception_code,
                        uint32_t system_code,
                        struct PP_Var error_description) {
  VLOG(4) << "PPB_ContentDecryptor_Private::LegacySessionError()";
  EnterInstance enter(instance);
  if (enter.failed())
    return;
  enter.functions()->LegacySessionError(instance, session_id, exception_code,
                                        system_code, error_description);
}

void DeliverBlock(PP_Instance instance,
                  PP_Resource decrypted_block,
                  const struct PP_DecryptedBlockInfo* decrypted_block_info) {
  VLOG(4) << "PPB_ContentDecryptor_Private::DeliverBlock()";
  EnterInstance enter(instance);
  if (enter.failed())
    return;
  enter.functions()->DeliverBlock(instance, decrypted_block,
                                  decrypted_block_info);
}

void DecoderInitializeDone(PP_Instance instance,
                           PP_DecryptorStreamType decoder_type,
                           uint32_t request_id,
                           PP_Bool success) {
  VLOG(4) << "PPB_ContentDecryptor_Private::DecoderInitializeDone()";
  EnterInstance enter(instance);
  if (enter.failed())
    return;
  enter.functions()->DecoderInitializeDone(instance, decoder_type, request_id,
                                           success);
}

void DecoderDeinitializeDone(PP_Instance instance,
                             PP_DecryptorStreamType decoder_type,
                             uint32_t request_id) {
  VLOG(4) << "PPB_ContentDecryptor_Private::DecoderDeinitializeDone()";
  EnterInstance enter(instance);
  if (enter.failed())
    return;
  enter.functions()->DecoderDeinitializeDone(instance, decoder_type,
                                             request_id);
}

void DecoderResetDone(PP_Instance instance,
                      PP_DecryptorStreamType decoder_type,
                      uint32_t request_id) {
  VLOG(4) << "PPB_ContentDecryptor_Private::DecoderResetDone()";
  EnterInstance enter(instance);
  if (enter.failed())
    return;
  enter.functions()->DecoderResetDone(instance, decoder_type, request_id);
}

void DeliverFrame(PP_Instance instance,
                  PP_Resource decrypted_frame,
                  const struct PP_DecryptedFrameInfo* decrypted_frame_info) {
  VLOG(4) << "PPB_ContentDecryptor_Private::DeliverFrame()";
  EnterInstance enter(instance);
  if (enter.failed())
    return;
  enter.functions()->DeliverFrame(instance, decrypted_frame,
                                  decrypted_frame_info);
}

void DeliverSamples(
    PP_Instance instance,
    PP_Resource audio_frames,
    const struct PP_DecryptedSampleInfo* decrypted_sample_info) {
  VLOG(4) << "PPB_ContentDecryptor_Private::DeliverSamples()";
  EnterInstance enter(instance);
  if (enter.failed())
    return;
  enter.functions()->DeliverSamples(instance, audio_frames,
                                    decrypted_sample_info);
}

const PPB_ContentDecryptor_Private_0_14
    g_ppb_contentdecryptor_private_thunk_0_14 = {&PromiseResolved,
                                                 &PromiseResolvedWithSession,
                                                 &PromiseRejected,
                                                 &SessionMessage,
                                                 &SessionKeysChange,
                                                 &SessionExpirationChange,
                                                 &SessionClosed,
                                                 &LegacySessionError,
                                                 &DeliverBlock,
                                                 &DecoderInitializeDone,
                                                 &DecoderDeinitializeDone,
                                                 &DecoderResetDone,
                                                 &DeliverFrame,
                                                 &DeliverSamples};

}  // namespace

PPAPI_THUNK_EXPORT const PPB_ContentDecryptor_Private_0_14*
GetPPB_ContentDecryptor_Private_0_14_Thunk() {
  return &g_ppb_contentdecryptor_private_thunk_0_14;
}

}  // namespace thunk
}  // namespace ppapi
