// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/fake_access_token_store.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"

using base::MessageLoopProxy;
using testing::_;
using testing::Invoke;

namespace content {

FakeAccessTokenStore::FakeAccessTokenStore()
    : originating_message_loop_(NULL) {
  ON_CALL(*this, LoadAccessTokens(_))
      .WillByDefault(Invoke(this,
                            &FakeAccessTokenStore::DefaultLoadAccessTokens));
  ON_CALL(*this, SaveAccessToken(_, _))
      .WillByDefault(Invoke(this,
                            &FakeAccessTokenStore::DefaultSaveAccessToken));
}

void FakeAccessTokenStore::NotifyDelegateTokensLoaded() {
  DCHECK(originating_message_loop_);
  if (!originating_message_loop_->BelongsToCurrentThread()) {
    originating_message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&FakeAccessTokenStore::NotifyDelegateTokensLoaded, this));
    return;
  }

  net::URLRequestContextGetter* context_getter = NULL;
  callback_.Run(access_token_set_, context_getter);
}

void FakeAccessTokenStore::DefaultLoadAccessTokens(
    const LoadAccessTokensCallbackType& callback) {
  originating_message_loop_ = MessageLoopProxy::current().get();
  callback_ = callback;
}

void FakeAccessTokenStore::DefaultSaveAccessToken(
    const GURL& server_url, const base::string16& access_token) {
  DCHECK(server_url.is_valid());
  access_token_set_[server_url] = access_token;
}

FakeAccessTokenStore::~FakeAccessTokenStore() {}

}  // namespace content
