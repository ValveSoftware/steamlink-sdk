// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_PUBLIC_BLIMP_CLIENT_CONTEXT_DELEGATE_H_
#define BLIMP_CLIENT_PUBLIC_BLIMP_CLIENT_CONTEXT_DELEGATE_H_

#include "base/macros.h"
#include "blimp/client/public/session/assignment.h"

class IdentityProvider;
class GoogleServiceAuthError;

namespace blimp {
namespace client {
class BlimpContents;

// BlimpClientContextDelegate is how the BlimpClientContext gets the
// functionality it needs from its embedder.
class BlimpClientContextDelegate {
 public:
  virtual ~BlimpClientContextDelegate() = default;

  // Attaches any required base::SupportsUserData::Data to the BlimpContents.
  virtual void AttachBlimpContentsHelpers(BlimpContents* blimp_contents) = 0;

  // Called whenever an assignment request has finished and the resulting
  // Assignment is ready to be used in an attempt to connect to the engine. The
  // |result| is the result for the assignment request itself, not for the
  // connection attempt. Only when |result| is ASSIGNMENT_REQUEST_RESULT_OK
  // will an attempt actually be made to connect to the engine using the
  // Assignment.
  virtual void OnAssignmentConnectionAttempted(
      AssignmentRequestResult result,
      const Assignment& assignment) = 0;

  // Create IdentityProvider for OAuth2 token retrieval, used in Authenticator.
  virtual std::unique_ptr<IdentityProvider> CreateIdentityProvider() = 0;

  // Propagate authentication error to the embedder.
  virtual void OnAuthenticationError(const GoogleServiceAuthError& error) = 0;

  // Called when the client is connected to the engine.
  virtual void OnConnected() = 0;

  // Called when the client is disconnected from the engine.
  // See EndConnectionMessage::Reason.
  virtual void OnEngineDisconnected(int result) = 0;

  // Called when the network is disconnected.
  // See /net/base/net_errors.h.
  virtual void OnNetworkDisconnected(int result) = 0;

 protected:
  BlimpClientContextDelegate() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(BlimpClientContextDelegate);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_PUBLIC_BLIMP_CLIENT_CONTEXT_DELEGATE_H_
