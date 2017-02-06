// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_FAKE_ACCESS_TOKEN_STORE_H_
#define CONTENT_BROWSER_GEOLOCATION_FAKE_ACCESS_TOKEN_STORE_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "content/public/browser/access_token_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

// A fake (non-persisted) access token store instance useful for testing.
class FakeAccessTokenStore : public AccessTokenStore {
 public:
  FakeAccessTokenStore();

  void NotifyDelegateTokensLoaded();

  // AccessTokenStore
  MOCK_METHOD1(LoadAccessTokens,
               void(const LoadAccessTokensCallback& callback));
  MOCK_METHOD2(SaveAccessToken,
               void(const GURL& server_url,
                    const base::string16& access_token));

  void DefaultLoadAccessTokens(const LoadAccessTokensCallback& callback);

  void DefaultSaveAccessToken(const GURL& server_url,
                              const base::string16& access_token);

  AccessTokenMap access_token_map_;
  LoadAccessTokensCallback callback_;

 protected:
  // Protected instead of private so we can have NiceMocks.
  virtual ~FakeAccessTokenStore();

 private:
  // In some tests, NotifyDelegateTokensLoaded() is called on a thread
  // other than the originating thread, in which case we must post
  // back to it.
  scoped_refptr<base::SingleThreadTaskRunner> originating_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(FakeAccessTokenStore);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GEOLOCATION_FAKE_ACCESS_TOKEN_STORE_H_
