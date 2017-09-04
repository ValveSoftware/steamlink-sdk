// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_CONTEXT_ASSIGNMENT_FETCHER_H_
#define BLIMP_CLIENT_CORE_CONTEXT_ASSIGNMENT_FETCHER_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "blimp/client/core/session/assignment_source.h"
#include "blimp/client/core/session/identity_source.h"

namespace blimp {
namespace client {

class AssignmentFetcher {
 public:
  using AuthErrorCallback = base::Callback<void(const GoogleServiceAuthError&)>;
  using AssignmentResultCallback =
      base::Callback<void(AssignmentRequestResult, const Assignment&)>;

  AssignmentFetcher(
      scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> file_thread_task_runner,
      std::unique_ptr<IdentityProvider> identity_provider,
      GURL assigner_url,
      const AssignmentResultCallback& assignment_received_callback,
      const AuthErrorCallback& error_callback);

  ~AssignmentFetcher();

  void Fetch();

  IdentitySource* GetIdentitySource();

 private:
  // Called when an OAuth2 token is received.  Will then ask the
  // AssignmentSource for an Assignment with this token.
  void OnAuthTokenReceived(const std::string& client_auth_token);

  // Provide OAuth2 token and propagate account sign in states change.
  std::unique_ptr<IdentitySource> identity_source_;

  AssignmentResultCallback assignment_received_callback_;

  // Returns the URL to use for connections to the assigner. Used to construct
  // the AssignmentSource.
  GURL assigner_url_;

  // The AssignmentSource is used when the user of AssignmentFetcher calls
  // Fetch() to get a valid assignment and later connect to the engine.
  std::unique_ptr<AssignmentSource> assignment_source_;

  // The task runner to use for IO operations.
  scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner_;

  // The task runner to use for file operations.
  scoped_refptr<base::SingleThreadTaskRunner> file_thread_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(AssignmentFetcher);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_CONTEXT_ASSIGNMENT_FETCHER_H_
