// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_COMMON_APPCACHE_APPCACHE_INTERFACES_H_
#define WEBKIT_COMMON_APPCACHE_APPCACHE_INTERFACES_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/time/time.h"
#include "url/gurl.h"
#include "webkit/common/webkit_storage_common_export.h"

namespace net {
class URLRequest;
}

namespace appcache {

// Defines constants, types, and abstract classes used in the main
// process and in child processes.

static const int kAppCacheNoHostId = 0;
static const int64 kAppCacheNoCacheId = 0;
static const int64 kAppCacheNoResponseId = 0;
static const int64 kAppCacheUnknownCacheId = -1;

enum AppCacheStatus {
  APPCACHE_STATUS_UNCACHED,
  APPCACHE_STATUS_IDLE,
  APPCACHE_STATUS_CHECKING,
  APPCACHE_STATUS_DOWNLOADING,
  APPCACHE_STATUS_UPDATE_READY,
  APPCACHE_STATUS_OBSOLETE,
  APPCACHE_STATUS_LAST = APPCACHE_STATUS_OBSOLETE
};

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

struct WEBKIT_STORAGE_COMMON_EXPORT AppCacheInfo {
  AppCacheInfo();
  ~AppCacheInfo();

  GURL manifest_url;
  base::Time creation_time;
  base::Time last_update_time;
  base::Time last_access_time;
  int64 cache_id;
  int64 group_id;
  AppCacheStatus status;
  int64 size;
  bool is_complete;
};

typedef std::vector<AppCacheInfo> AppCacheInfoVector;

// Type to hold information about a single appcache resource.
struct WEBKIT_STORAGE_COMMON_EXPORT AppCacheResourceInfo {
  AppCacheResourceInfo();
  ~AppCacheResourceInfo();

  GURL url;
  int64 size;
  bool is_master;
  bool is_manifest;
  bool is_intercept;
  bool is_fallback;
  bool is_foreign;
  bool is_explicit;
  int64 response_id;
};

struct WEBKIT_STORAGE_COMMON_EXPORT AppCacheErrorDetails {
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

struct WEBKIT_STORAGE_COMMON_EXPORT Namespace {
  Namespace();  // Type is set to APPCACHE_FALLBACK_NAMESPACE by default.
  Namespace(AppCacheNamespaceType type, const GURL& url, const GURL& target,
            bool is_pattern);
  Namespace(AppCacheNamespaceType type, const GURL& url, const GURL& target,
            bool is_pattern, bool is_executable);
  ~Namespace();

  bool IsMatch(const GURL& url) const;

  AppCacheNamespaceType type;
  GURL namespace_url;
  GURL target_url;
  bool is_pattern;
  bool is_executable;
};

typedef std::vector<Namespace> NamespaceVector;

// Interface used by backend (browser-process) to talk to frontend (renderer).
class WEBKIT_STORAGE_COMMON_EXPORT AppCacheFrontend {
 public:
  virtual void OnCacheSelected(
      int host_id, const appcache::AppCacheInfo& info) = 0;
  virtual void OnStatusChanged(const std::vector<int>& host_ids,
                               AppCacheStatus status) = 0;
  virtual void OnEventRaised(const std::vector<int>& host_ids,
                             AppCacheEventID event_id) = 0;
  virtual void OnProgressEventRaised(const std::vector<int>& host_ids,
                                     const GURL& url,
                                     int num_total, int num_complete) = 0;
  virtual void OnErrorEventRaised(
      const std::vector<int>& host_ids,
      const appcache::AppCacheErrorDetails& details) = 0;
  virtual void OnContentBlocked(int host_id,
                                const GURL& manifest_url) = 0;
  virtual void OnLogMessage(int host_id, AppCacheLogLevel log_level,
                            const std::string& message) = 0;
  virtual ~AppCacheFrontend() {}
};

// Interface used by frontend (renderer) to talk to backend (browser-process).
class WEBKIT_STORAGE_COMMON_EXPORT AppCacheBackend {
 public:
  virtual void RegisterHost(int host_id) = 0;
  virtual void UnregisterHost(int host_id) = 0;
  virtual void SetSpawningHostId(int host_id, int spawning_host_id) = 0;
  virtual void SelectCache(int host_id,
                           const GURL& document_url,
                           const int64 cache_document_was_loaded_from,
                           const GURL& manifest_url) = 0;
  virtual void SelectCacheForWorker(
                           int host_id,
                           int parent_process_id,
                           int parent_host_id) = 0;
  virtual void SelectCacheForSharedWorker(
                           int host_id,
                           int64 appcache_id) = 0;
  virtual void MarkAsForeignEntry(int host_id, const GURL& document_url,
                                  int64 cache_document_was_loaded_from) = 0;
  virtual AppCacheStatus GetStatus(int host_id) = 0;
  virtual bool StartUpdate(int host_id) = 0;
  virtual bool SwapCache(int host_id) = 0;
  virtual void GetResourceList(
      int host_id, std::vector<AppCacheResourceInfo>* resource_infos) = 0;

 protected:
  virtual ~AppCacheBackend() {}
};

// Useful string constants.
// Note: These are also defined elsewhere in the chrome code base in
// url_contants.h .cc, however the appcache library doesn't not have
// any dependencies on the chrome library, so we can't use them in here.
WEBKIT_STORAGE_COMMON_EXPORT extern const char kHttpScheme[];
WEBKIT_STORAGE_COMMON_EXPORT extern const char kHttpsScheme[];
WEBKIT_STORAGE_COMMON_EXPORT extern const char kHttpGETMethod[];
WEBKIT_STORAGE_COMMON_EXPORT extern const char kHttpHEADMethod[];

// CommandLine flag to turn this experimental feature on.
WEBKIT_STORAGE_COMMON_EXPORT extern const char kEnableExecutableHandlers[];

WEBKIT_STORAGE_COMMON_EXPORT bool IsSchemeSupported(const GURL& url);
WEBKIT_STORAGE_COMMON_EXPORT bool IsMethodSupported(const std::string& method);
WEBKIT_STORAGE_COMMON_EXPORT bool IsSchemeAndMethodSupported(
    const net::URLRequest* request);

WEBKIT_STORAGE_COMMON_EXPORT extern const base::FilePath::CharType
    kAppCacheDatabaseName[];

}  // namespace

#endif  // WEBKIT_COMMON_APPCACHE_APPCACHE_INTERFACES_H_
