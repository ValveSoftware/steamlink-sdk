// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/public/resources/blimp_strings.h"

#include "blimp/client/core/resources/grit/blimp_strings.h"
#include "blimp/common/proto/protocol_control.pb.h"
#include "ui/base/l10n/l10n_util.h"

namespace blimp {
namespace string {

base::string16 AssignmentResultErrorToString(
    client::AssignmentRequestResult result) {
  DCHECK(result != client::ASSIGNMENT_REQUEST_RESULT_OK);

  int string_id = IDS_ASSIGNMENT_SUCCESS;
  switch (result) {
    case client::ASSIGNMENT_REQUEST_RESULT_BAD_REQUEST:
      string_id = IDS_ASSIGNMENT_FAILURE_BAD_REQUEST;
      break;
    case client::ASSIGNMENT_REQUEST_RESULT_BAD_RESPONSE:
      string_id = IDS_ASSIGNMENT_FAILURE_BAD_RESPONSE;
      break;
    case client::ASSIGNMENT_REQUEST_RESULT_INVALID_PROTOCOL_VERSION:
      string_id = IDS_ASSIGNMENT_FAILURE_BAD_VERSION;
      break;
    case client::ASSIGNMENT_REQUEST_RESULT_EXPIRED_ACCESS_TOKEN:
      string_id = IDS_ASSIGNMENT_FAILURE_EXPIRED_TOKEN;
      break;
    case client::ASSIGNMENT_REQUEST_RESULT_USER_INVALID:
      string_id = IDS_ASSIGNMENT_FAILURE_USER_INVALID;
      break;
    case client::ASSIGNMENT_REQUEST_RESULT_OUT_OF_VMS:
      string_id = IDS_ASSIGNMENT_FAILURE_OUT_OF_VMS;
      break;
    case client::ASSIGNMENT_REQUEST_RESULT_SERVER_ERROR:
      string_id = IDS_ASSIGNMENT_FAILURE_SERVER_ERROR;
      break;
    case client::ASSIGNMENT_REQUEST_RESULT_SERVER_INTERRUPTED:
      string_id = IDS_ASSIGNMENT_FAILURE_SERVER_INTERRUPTED;
      break;
    case client::ASSIGNMENT_REQUEST_RESULT_NETWORK_FAILURE:
      string_id = IDS_ASSIGNMENT_FAILURE_NETWORK;
      break;
    case client::ASSIGNMENT_REQUEST_RESULT_INVALID_CERT:
      string_id = IDS_ASSIGNMENT_INVALID_CERT;
      break;
    case client::ASSIGNMENT_REQUEST_RESULT_UNKNOWN:
      string_id = IDS_ASSIGNMENT_FAILURE_UNKNOWN;
      break;
    case client::ASSIGNMENT_REQUEST_RESULT_OK:
      NOTIMPLEMENTED();
  }

  return l10n_util::GetStringUTF16(string_id);
}

base::string16 EndConnectionMessageToString(int reason) {
  int string_id = IDS_BLIMP_ENDCONNECTION_UNKNOWN;
  EndConnectionMessage::Reason end_connection_reason =
      static_cast<EndConnectionMessage::Reason>(reason);
  switch (end_connection_reason) {
    case EndConnectionMessage::AUTHENTICATION_FAILED:
      string_id = IDS_BLIMP_ENDCONNECTION_AUTH_FAIL;
      break;
    case EndConnectionMessage::PROTOCOL_MISMATCH:
      string_id = IDS_BLIMP_ENDCONNECTION_PROTOCOL_MISMATCH;
      break;
    case EndConnectionMessage::UNKNOWN:
      string_id = IDS_BLIMP_ENDCONNECTION_UNKNOWN;
      break;
  }

  return l10n_util::GetStringUTF16(string_id);
}

}  // namespace string
}  // namespace blimp
