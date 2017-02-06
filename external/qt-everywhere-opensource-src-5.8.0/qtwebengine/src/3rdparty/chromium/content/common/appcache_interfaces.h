// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_APPCACHE_INTERFACES_H_
#define CONTENT_COMMON_APPCACHE_INTERFACES_H_

#include <stdint.h>

#include <string>

#include "base/files/file_path.h"
#include "content/public/common/appcache_info.h"

namespace net {
class URLRequest;
}

namespace content {

// Defines constants, types, and abstract classes used in the main
// process and in child processes.

enum AppCacheEventID {
  APPCACHE_CHECKING_EVENT,
  APPCACHE_ERROR_EVENT,
  APPCACHE_NO_UPDATE_EVENT,
  APPCACHE_DOWNLOADING_EVENT,
  APPCACHE_PROGRESS_EVENT,
  APPCACHE_UPDATE_READY_EVENT,
  APPCACHE_CACHED_EVENT,
  APPCACHE_OBSOLETE_EVENT,
  APPCACHE_EVENT_ID_LAST = APPCACHE_OBSOLETE_EVENT
};

// Temporarily renumber them in wierd way, to help remove LOG_TIP from WebKit
enum AppCacheLogLevel {
  APPCACHE_LOG_DEBUG = 4,
  APPCACHE_LOG_INFO = 1,
  APPCACHE_LOG_WARNING = 2,
  APPCACHE_LOG_ERROR = 3,
};

enum AppCacheNamespaceType {
  APPCACHE_FALLBACK_NAMESPACE,
  APPCACHE_INTERCEPT_NAMESPACE,
  APPCACHE_NETWORK_NAMESPACE
};

enum AppCacheErrorReason {
  APPCACHE_MANIFEST_ERROR,
  APPCACHE_SIGNATURE_ERROR,
  APPCACHE_RESOURCE_ERROR,
  APPCACHE_CHANGED_ERROR,
  APPCACHE_ABORT_ERROR,
  APPCACHE_QUOTA_ERROR,
  APPCACHE_POLICY_ERROR,
  APPCACHE_UNKNOWN_ERROR,
  APPCACHE_ERROR_REASON_LAST = APPCACHE_UNKNOWN_ERROR
};

// Type to hold information about a single appcache resource.
struct CONTENT_EXPORT AppCacheResourceInfo {
  AppCacheResourceInfo();
  AppCacheResourceInfo(const AppCacheResourceInfo& other);
  ~AppCacheResourceInfo();

  GURL url;
  int64_t size;
  bool is_master;
  bool is_manifest;
  bool is_intercept;
  bool is_fallback;
  bool is_foreign;
  bool is_explicit;
  int64_t response_id;
};

struct CONTENT_EXPORT AppCacheErrorDetails {
  AppCacheErrorDetails();
  AppCacheErrorDetails(std::string message,
               AppCacheErrorReason reason,
               GURL url,
               int status,
               bool is_cross_origin);
  ~AppCacheErrorDetails();

  std::string message;
  AppCacheErrorReason reason;
  GURL url;
  int status;
  bool is_cross_origin;
};

typedef std::vector<AppCacheResourceInfo> AppCacheResourceInfoVector;

struct CONTENT_EXPORT AppCacheNamespace {
  AppCacheNamespace();  // Type is APPCACHE_FALLBACK_NAMESPACE by default.
  AppCacheNamespace(AppCacheNamespaceType type, const GURL& url,
      const GURL& target, bool is_pattern);
  AppCacheNamespace(AppCacheNamespaceType type, const GURL& url,
      const GURL& target, bool is_pattern, bool is_executable);
  ~AppCacheNamespace();

  bool IsMatch(const GURL& url) const;

  AppCacheNamespaceType type;
  GURL namespace_url;
  GURL target_url;
  bool is_pattern;
  bool is_executable;
};

typedef std::vector<AppCacheNamespace> AppCacheNamespaceVector;

// Interface used by backend (browser-process) to talk to frontend (renderer).
class CONTENT_EXPORT AppCacheFrontend {
 public:
  virtual void OnCacheSelected(
      int host_id, const AppCacheInfo& info) = 0;
  virtual void OnStatusChanged(const std::vector<int>& host_ids,
                               AppCacheStatus status) = 0;
  virtual void OnEventRaised(const std::vector<int>& host_ids,
                             AppCacheEventID event_id) = 0;
  virtual void OnProgressEventRaised(const std::vector<int>& host_ids,
                                     const GURL& url,
                                     int num_total, int num_complete) = 0;
  virtual void OnErrorEventRaised(
      const std::vector<int>& host_ids,
      const AppCacheErrorDetails& details) = 0;
  virtual void OnContentBlocked(int host_id,
                                const GURL& manifest_url) = 0;
  virtual void OnLogMessage(int host_id, AppCacheLogLevel log_level,
                            const std::string& message) = 0;
  virtual ~AppCacheFrontend() {}
};

// Interface used by frontend (renderer) to talk to backend (browser-process).
class CONTENT_EXPORT AppCacheBackend {
 public:
  virtual void RegisterHost(int host_id) = 0;
  virtual void UnregisterHost(int host_id) = 0;
  virtual void SetSpawningHostId(int host_id, int spawning_host_id) = 0;
  virtual void SelectCache(int host_id,
                           const GURL& document_url,
                           const int64_t cache_document_was_loaded_from,
                           const GURL& manifest_url) = 0;
  virtual void SelectCacheForWorker(
                           int host_id,
                           int parent_process_id,
                           int parent_host_id) = 0;
  virtual void SelectCacheForSharedWorker(int host_id, int64_t appcache_id) = 0;
  virtual void MarkAsForeignEntry(int host_id,
                                  const GURL& document_url,
                                  int64_t cache_document_was_loaded_from) = 0;
  virtual AppCacheStatus GetStatus(int host_id) = 0;
  virtual bool StartUpdate(int host_id) = 0;
  virtual bool SwapCache(int host_id) = 0;
  virtual void GetResourceList(
      int host_id, std::vector<AppCacheResourceInfo>* resource_infos) = 0;

 protected:
  virtual ~AppCacheBackend() {}
};

// Useful string constants.
CONTENT_EXPORT extern const char kHttpGETMethod[];
CONTENT_EXPORT extern const char kHttpHEADMethod[];

// base::CommandLine flag to turn this experimental feature on.
CONTENT_EXPORT extern const char kEnableExecutableHandlers[];

CONTENT_EXPORT bool IsSchemeSupportedForAppCache(const GURL& url);
CONTENT_EXPORT bool IsMethodSupportedForAppCache(
    const std::string& method);
CONTENT_EXPORT bool IsSchemeAndMethodSupportedForAppCache(
    const net::URLRequest* request);

CONTENT_EXPORT extern const base::FilePath::CharType
    kAppCacheDatabaseName[];

}  // namespace

#endif  // CONTENT_COMMON_APPCACHE_INTERFACES_H_
