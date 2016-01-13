// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines the Geolocation access token store, and associated factory function.
// An access token store is responsible for providing the API to persist
// access tokens, one at a time, and to load them back on mass.
// The API is a little more complex than one might wish, due to the need for
// prefs access to happen asynchronously on the UI thread.
// This API is provided as abstract base classes to allow mocking and testing
// of clients, without dependency on browser process singleton objects etc.

#ifndef CONTENT_PUBLIC_BROWSER_ACCESS_TOKEN_STORE_H_
#define CONTENT_PUBLIC_BROWSER_ACCESS_TOKEN_STORE_H_

#include <map>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

class GURL;

namespace net {
class URLRequestContextGetter;
}

namespace content {

// Provides storage for the access token used in the network request.
class AccessTokenStore : public base::RefCountedThreadSafe<AccessTokenStore> {
 public:
  // Map of server URLs to associated access token.
  typedef std::map<GURL, base::string16> AccessTokenSet;
  typedef base::Callback<void(AccessTokenSet, net::URLRequestContextGetter*)>
      LoadAccessTokensCallbackType;

  // |callback| will be invoked once per LoadAccessTokens call, after existing
  // access tokens have been loaded from persistent store. As a convenience the
  // URLRequestContextGetter is also supplied as an argument in |callback|, as
  // in Chrome the call to obtain this must also be performed on the UI thread
  // so it is efficient to piggyback it onto this request.
  // Takes ownership of |callback|.
  virtual void LoadAccessTokens(
      const LoadAccessTokensCallbackType& callback) = 0;

  virtual void SaveAccessToken(
      const GURL& server_url, const base::string16& access_token) = 0;

 protected:
  friend class base::RefCountedThreadSafe<AccessTokenStore>;
  CONTENT_EXPORT AccessTokenStore() {}
  CONTENT_EXPORT virtual ~AccessTokenStore() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ACCESS_TOKEN_STORE_H_
