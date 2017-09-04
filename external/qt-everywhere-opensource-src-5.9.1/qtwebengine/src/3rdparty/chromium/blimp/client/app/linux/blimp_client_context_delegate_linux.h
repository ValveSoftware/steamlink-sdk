// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_APP_LINUX_BLIMP_CLIENT_CONTEXT_DELEGATE_LINUX_H_
#define BLIMP_CLIENT_APP_LINUX_BLIMP_CLIENT_CONTEXT_DELEGATE_LINUX_H_

#include "blimp/client/public/blimp_client_context_delegate.h"
#include "google_apis/gaia/identity_provider.h"

namespace blimp {
namespace client {

class BlimpContents;

class BlimpClientContextDelegateLinux : public BlimpClientContextDelegate {
 public:
  BlimpClientContextDelegateLinux();
  ~BlimpClientContextDelegateLinux() override;

  // BlimpClientContextDelegate implementation.
  void AttachBlimpContentsHelpers(BlimpContents* blimp_contents) override;
  void OnAssignmentConnectionAttempted(AssignmentRequestResult result,
                                       const Assignment& assignment) override;
  std::unique_ptr<IdentityProvider> CreateIdentityProvider() override;
  void OnAuthenticationError(const GoogleServiceAuthError& error) override;
  void OnConnected() override;
  void OnEngineDisconnected(int result) override;
  void OnNetworkDisconnected(int result) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BlimpClientContextDelegateLinux);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_APP_LINUX_BLIMP_CLIENT_CONTEXT_DELEGATE_LINUX_H_
