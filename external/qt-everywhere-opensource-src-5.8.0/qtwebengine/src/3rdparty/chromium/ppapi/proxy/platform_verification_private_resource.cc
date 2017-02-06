// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/platform_verification_private_resource.h"

#include "base/bind.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/dispatch_reply_message.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/shared_impl/var_tracker.h"

namespace ppapi {
namespace proxy {

PlatformVerificationPrivateResource::PlatformVerificationPrivateResource(
    Connection connection,
    PP_Instance instance)
    : PluginResource(connection, instance) {
  SendCreate(BROWSER, PpapiHostMsg_PlatformVerification_Create());
}

PlatformVerificationPrivateResource::~PlatformVerificationPrivateResource() {}

thunk::PPB_PlatformVerification_API*
PlatformVerificationPrivateResource::AsPPB_PlatformVerification_API() {
  return this;
}

int32_t PlatformVerificationPrivateResource::ChallengePlatform(
    const PP_Var& service_id,
    const PP_Var& challenge,
    PP_Var* signed_data,
    PP_Var* signed_data_signature,
    PP_Var* platform_key_certificate,
    const scoped_refptr<TrackedCallback>& callback) {
  // Prevent null types for obvious reasons, but also ref-counted types to avoid
  // leaks on challenge failures (since they're only written to on success).
  if (!signed_data || !signed_data_signature || !platform_key_certificate ||
      VarTracker::IsVarTypeRefcounted(signed_data->type) ||
      VarTracker::IsVarTypeRefcounted(signed_data_signature->type) ||
      VarTracker::IsVarTypeRefcounted(platform_key_certificate->type)) {
    return PP_ERROR_BADARGUMENT;
  }

  StringVar* service_id_str = StringVar::FromPPVar(service_id);
  if (!service_id_str)
    return PP_ERROR_BADARGUMENT;

  scoped_refptr<ArrayBufferVar> challenge_buffer =
      ArrayBufferVar::FromPPVar(challenge);
  if (!challenge_buffer.get())
    return PP_ERROR_BADARGUMENT;

  uint8_t* challenge_data = static_cast<uint8_t*>(challenge_buffer->Map());
  uint32_t challenge_length = challenge_buffer->ByteLength();
  std::vector<uint8_t> challenge_vector(challenge_data,
                                        challenge_data + challenge_length);
  challenge_buffer->Unmap();

  PpapiHostMsg_PlatformVerification_ChallengePlatform challenge_message(
      service_id_str->value(), challenge_vector);

  ChallengePlatformParams output_params = {
      signed_data, signed_data_signature, platform_key_certificate, callback };

  Call<PpapiHostMsg_PlatformVerification_ChallengePlatformReply>(
      BROWSER, challenge_message, base::Bind(
          &PlatformVerificationPrivateResource::OnChallengePlatformReply,
          base::Unretained(this), output_params));
  return PP_OK_COMPLETIONPENDING;
}

void PlatformVerificationPrivateResource::OnChallengePlatformReply(
    ChallengePlatformParams output_params,
    const ResourceMessageReplyParams& params,
    const std::vector<uint8_t>& raw_signed_data,
    const std::vector<uint8_t>& raw_signed_data_signature,
    const std::string& raw_platform_key_certificate) {
  if (!TrackedCallback::IsPending(output_params.callback) ||
      TrackedCallback::IsScheduledToRun(output_params.callback)) {
    return;
  }

  if (params.result() == PP_OK) {
    *(output_params.signed_data) =
        (PpapiGlobals::Get()->GetVarTracker()->MakeArrayBufferVar(
            static_cast<uint32_t>(raw_signed_data.size()),
            &raw_signed_data.front()))->GetPPVar();
    *(output_params.signed_data_signature) =
        (PpapiGlobals::Get()->GetVarTracker()->MakeArrayBufferVar(
            static_cast<uint32_t>(raw_signed_data_signature.size()),
            &raw_signed_data_signature.front()))->GetPPVar();
    *(output_params.platform_key_certificate) =
        (new StringVar(raw_platform_key_certificate))->GetPPVar();
  }
  output_params.callback->Run(params.result());
}

}  // namespace proxy
}  // namespace ppapi
