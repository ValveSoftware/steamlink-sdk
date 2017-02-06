// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RESOURCE_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_RESOURCE_CONTEXT_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/supports_user_data.h"
#include "build/build_config.h"
#include "content/common/content_export.h"

class GURL;

namespace net {
class HostResolver;
class KeygenHandler;
class URLRequestContext;
}

namespace content {

class AppCacheService;

// ResourceContext contains the relevant context information required for
// resource loading. It lives on the IO thread, although it is constructed on
// the UI thread. It must be destructed on the IO thread.
class CONTENT_EXPORT ResourceContext : public base::SupportsUserData {
 public:
  ResourceContext();
  ~ResourceContext() override;
  virtual net::HostResolver* GetHostResolver() = 0;

  // DEPRECATED: This is no longer a valid given isolated apps/sites and
  // storage partitioning. This getter returns the default context associated
  // with a BrowsingContext.
  virtual net::URLRequestContext* GetRequestContext() = 0;

  // Create a platform KeygenHandler and pass it to |callback|. The |callback|
  // may be run synchronously.
  virtual void CreateKeygenHandler(
      uint32_t key_size_in_bits,
      const std::string& challenge_string,
      const GURL& url,
      const base::Callback<void(std::unique_ptr<net::KeygenHandler>)>&
          callback);

  // Returns a random salt string that is used for creating media device IDs.
  // Returns a random string by default.
  virtual std::string GetMediaDeviceIDSalt();

  // Utility function useful for embedders. Only needs to be called if
  // 1) The embedder needs to use a new salt, and
  // 2) The embedder saves its salt across restarts.
  static std::string CreateRandomMediaDeviceIDSalt();

 private:
  const std::string media_device_id_salt_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RESOURCE_CONTEXT_H_
