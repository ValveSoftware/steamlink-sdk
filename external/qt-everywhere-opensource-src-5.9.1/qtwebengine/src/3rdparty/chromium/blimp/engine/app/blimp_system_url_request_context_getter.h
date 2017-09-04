// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_APP_BLIMP_SYSTEM_URL_REQUEST_CONTEXT_GETTER_H_
#define BLIMP_ENGINE_APP_BLIMP_SYSTEM_URL_REQUEST_CONTEXT_GETTER_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "content/public/browser/content_browser_client.h"
#include "net/url_request/url_request_context_getter.h"

namespace base {
class MessageLoop;
class SingleThreadTaskRunner;
}

namespace blimp {
namespace engine {

// The URLRequestContextGetter for Blimp system requests that should not be
// associated with a user URL request context (e.g. metrics upload).
class BlimpSystemURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  BlimpSystemURLRequestContextGetter();

  // net::URLRequestContextGetter implementation.
  net::URLRequestContext* GetURLRequestContext() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override;

 private:
  ~BlimpSystemURLRequestContextGetter() override;

  std::unique_ptr<net::URLRequestContext> url_request_context_;

  DISALLOW_COPY_AND_ASSIGN(BlimpSystemURLRequestContextGetter);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_APP_BLIMP_SYSTEM_URL_REQUEST_CONTEXT_GETTER_H_
