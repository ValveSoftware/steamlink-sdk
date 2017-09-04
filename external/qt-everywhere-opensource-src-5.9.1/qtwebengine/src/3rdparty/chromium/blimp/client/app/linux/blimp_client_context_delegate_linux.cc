// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "blimp/client/app/linux/blimp_client_context_delegate_linux.h"
#include "blimp/client/public/resources/blimp_strings.h"
#include "blimp/client/support/session/blimp_default_identity_provider.h"
#include "net/base/net_errors.h"

namespace blimp {
namespace client {

BlimpClientContextDelegateLinux::BlimpClientContextDelegateLinux() = default;

BlimpClientContextDelegateLinux::~BlimpClientContextDelegateLinux() = default;

void BlimpClientContextDelegateLinux::AttachBlimpContentsHelpers(
    BlimpContents* blimp_contents) {}

void BlimpClientContextDelegateLinux::OnAssignmentConnectionAttempted(
    AssignmentRequestResult result,
    const Assignment& assignment) {
  if (result == ASSIGNMENT_REQUEST_RESULT_OK)
    return;

  LOG(WARNING) << string::AssignmentResultErrorToString(result);
}

std::unique_ptr<IdentityProvider>
BlimpClientContextDelegateLinux::CreateIdentityProvider() {
  return base::MakeUnique<BlimpDefaultIdentityProvider>();
}

void BlimpClientContextDelegateLinux::OnAuthenticationError(
    const GoogleServiceAuthError& error) {
  LOG(WARNING) << "GoogleAuth error : " << error.ToString();
}

void BlimpClientContextDelegateLinux::OnConnected() {
  VLOG(1) << "Connected.";
}

void BlimpClientContextDelegateLinux::OnEngineDisconnected(int result) {
  LOG(WARNING) << "Disconnected from the engine, reason: "
               << string::EndConnectionMessageToString(result);
}

void BlimpClientContextDelegateLinux::OnNetworkDisconnected(int result) {
  LOG(WARNING) << "Disconnected, reason: " << net::ErrorToShortString(result);
}

}  // namespace client
}  // namespace blimp
