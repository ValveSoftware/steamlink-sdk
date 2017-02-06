// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/fake_access_token_store.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"

using testing::_;
using testing::Invoke;

namespace content {

FakeAccessTokenStore::FakeAccessTokenStore() {
  ON_CALL(*this, LoadAccessTokens(_))
      .WillByDefault(Invoke(this,
                            &FakeAccessTokenStore::DefaultLoadAccessTokens));
  ON_CALL(*this, SaveAccessToken(_, _))
      .WillByDefault(Invoke(this,
                            &FakeAccessTokenStore::DefaultSaveAccessToken));
}

void FakeAccessTokenStore::NotifyDelegateTokensLoaded() {
  DCHECK(originating_task_runner_);
  if (!originating_task_runner_->BelongsToCurrentThread()) {
    originating_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&FakeAccessTokenStore::NotifyDelegateTokensLoaded, this));
    return;
  }

  net::URLRequestContextGetter* context_getter = NULL;
  callback_.Run(access_token_map_, context_getter);
}

void FakeAccessTokenStore::DefaultLoadAccessTokens(
    const LoadAccessTokensCallback& callback) {
  originating_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  callback_ = callback;
}

void FakeAccessTokenStore::DefaultSaveAccessToken(
    const GURL& server_url, const base::string16& access_token) {
  DCHECK(server_url.is_valid());
  access_token_map_[server_url] = access_token;
}

FakeAccessTokenStore::~FakeAccessTokenStore() {}

}  // namespace content
