// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_GEOLOCATION_CAST_ACCESS_TOKEN_STORE_H_
#define CHROMECAST_BROWSER_GEOLOCATION_CAST_ACCESS_TOKEN_STORE_H_

#include "base/macros.h"
#include "content/public/browser/access_token_store.h"

namespace content {
class BrowserContext;
}

namespace chromecast {
namespace shell {

// Access token store for chromecast devices used to initialize the network
// location provider.
class CastAccessTokenStore : public content::AccessTokenStore {
 public:
  explicit CastAccessTokenStore(content::BrowserContext* browser_context);

 private:
  ~CastAccessTokenStore() override;

  // AccessTokenStore implementation:
  void LoadAccessTokens(const LoadAccessTokensCallback& callback) override;
  void SaveAccessToken(
      const GURL& server_url, const base::string16& access_token) override;

  void GetRequestContextGetterOnUIThread();
  void RespondOnOriginatingThread();

  content::BrowserContext* const browser_context_;
  net::URLRequestContextGetter* request_context_;
  AccessTokenMap access_token_map_;
  LoadAccessTokensCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(CastAccessTokenStore);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_GEOLOCATION_CAST_ACCESS_TOKEN_STORE_H_
