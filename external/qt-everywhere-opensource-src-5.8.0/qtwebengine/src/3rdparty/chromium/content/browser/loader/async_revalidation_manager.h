// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_ASYNC_REVALIDATION_MANAGER_H_
#define CONTENT_BROWSER_LOADER_ASYNC_REVALIDATION_MANAGER_H_

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"

class GURL;

namespace net {
class URLRequest;
class HttpCache;
}

namespace content {

class AsyncRevalidationDriver;
class ResourceContext;
class ResourceScheduler;
struct ResourceRequest;

// One instance of this class manages all active AsyncRevalidationDriver objects
// for all profiles. It is created by and owned by
// ResourceDispatcherHostImpl. It also implements the creation of a new
// net::URLRequest and AsyncRevalidationDriver from an existing net::URLRequest
// that has had the stale-while-revalidate algorithm applied to it.
class AsyncRevalidationManager {
 public:
  AsyncRevalidationManager();
  ~AsyncRevalidationManager();

  // Starts an async revalidation by copying |for_request|. |scheduler| must
  // remain valid until this object is destroyed.
  void BeginAsyncRevalidation(const net::URLRequest& for_request,
                              ResourceScheduler* scheduler);

  // Cancel all pending async revalidations that use ResourceContext.
  void CancelAsyncRevalidationsForResourceContext(
      ResourceContext* resource_context);

  static bool QualifiesForAsyncRevalidation(const ResourceRequest& request);

 private:
  // The key of the map of pending async revalidations. This key has a distinct
  // value for every in-progress async revalidation. It is used to avoid
  // duplicate async revalidations, and also to cancel affected async
  // revalidations when a ResourceContext is removed.
  //
  // Request headers are intentionally not included in the key for simplicity,
  // as they usually don't affect caching.
  //
  // TODO(ricea): Behave correctly in cases where the request headers do make a
  // difference. crbug.com/567721
  struct AsyncRevalidationKey {
    AsyncRevalidationKey(const ResourceContext* resource_context,
                         const net::HttpCache* http_cache,
                         const GURL& url);

    // Create a prefix key that is used to match all of the
    // AsyncRevalidationDrivers using |resource_context| in the map.
    explicit AsyncRevalidationKey(const ResourceContext* resource_context);

    // The key for a map needs to be copyable.
    AsyncRevalidationKey(const AsyncRevalidationKey& rhs) = default;
    ~AsyncRevalidationKey();

    // No operator= is generated because the struct members are immutable.

    // |resource_context| and |http_cache| are never dereferenced; they are only
    // compared to other values.
    const ResourceContext* const resource_context;

    // There are multiple independent HttpCache objects per ResourceContext.
    const net::HttpCache* const http_cache;

    // Derived from the url via net::HttpUtil::SpecForRequest().
    const std::string url_key;

    struct LessThan {
      bool operator()(const AsyncRevalidationKey& lhs,
                      const AsyncRevalidationKey& rhs) const;
    };
  };

  using AsyncRevalidationMap =
      std::map<AsyncRevalidationKey,
               std::unique_ptr<AsyncRevalidationDriver>,
               AsyncRevalidationKey::LessThan>;

  void OnAsyncRevalidationComplete(AsyncRevalidationMap::iterator it);

  // Map of AsyncRevalidationDriver instances that are currently in-flight:
  // either waiting to be scheduled or active on the network.
  AsyncRevalidationMap in_progress_;

  DISALLOW_COPY_AND_ASSIGN(AsyncRevalidationManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_ASYNC_REVALIDATION_MANAGER_H_
