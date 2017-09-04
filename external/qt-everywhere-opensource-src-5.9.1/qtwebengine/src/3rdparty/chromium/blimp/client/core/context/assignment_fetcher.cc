// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/context/assignment_fetcher.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "blimp/client/core/session/assignment_source.h"
#include "blimp/client/core/session/identity_source.h"

namespace blimp {
namespace client {

AssignmentFetcher::AssignmentFetcher(
    scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> file_thread_task_runner,
    std::unique_ptr<IdentityProvider> identity_provider,
    GURL assigner_url,
    const AssignmentResultCallback& assignment_received_callback,
    const AuthErrorCallback& error_callback)
    : identity_source_(base::MakeUnique<IdentitySource>(
          std::move(identity_provider),
          error_callback,
          base::Bind(&AssignmentFetcher::OnAuthTokenReceived,
                     base::Unretained(this)))),
      assignment_received_callback_(assignment_received_callback),
      assigner_url_(assigner_url),
      io_thread_task_runner_(io_thread_task_runner),
      file_thread_task_runner_(file_thread_task_runner) {}

AssignmentFetcher::~AssignmentFetcher() = default;

void AssignmentFetcher::Fetch() {
  // Start Blimp authentication flow. The OAuth2 token will be used in
  // assignment source.
  identity_source_->Connect();
}

IdentitySource* AssignmentFetcher::GetIdentitySource() {
  return identity_source_.get();
}

void AssignmentFetcher::OnAuthTokenReceived(
    const std::string& client_auth_token) {
  if (!assignment_source_) {
    assignment_source_.reset(new AssignmentSource(
        assigner_url_, io_thread_task_runner_, file_thread_task_runner_));
  }

  VLOG(1) << "Trying to get assignment.";
  assignment_source_->GetAssignment(client_auth_token,
                                    assignment_received_callback_);
}

}  // namespace client
}  // namespace blimp
