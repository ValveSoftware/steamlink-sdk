// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_DB_V4_PROTOCOL_MANAGER_UTIL_H_
#define COMPONENTS_SAFE_BROWSING_DB_V4_PROTOCOL_MANAGER_UTIL_H_

// A class that implements the stateless methods used by the GetHashUpdate and
// GetFullHash stubby calls made by Chrome using the SafeBrowsing V4 protocol.

#include <ostream>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/hash.h"
#include "components/safe_browsing_db/safebrowsing.pb.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

namespace net {
class HttpRequestHeaders;
}  // namespace net

namespace safe_browsing {

typedef FetchThreatListUpdatesRequest::ListUpdateRequest ListUpdateRequest;
typedef FetchThreatListUpdatesResponse::ListUpdateResponse ListUpdateResponse;

// Config passed to the constructor of a V4 protocol manager.
struct V4ProtocolConfig {
  // The safe browsing client name sent in each request.
  std::string client_name;

  // Current product version sent in each request.
  std::string version;

  // The Google API key.
  std::string key_param;

  // Disable auto-updates using a command line switch?
  bool disable_auto_update;

  V4ProtocolConfig();
  V4ProtocolConfig(const V4ProtocolConfig& other);
  ~V4ProtocolConfig();
};

// The information required to uniquely identify each list the client is
// interested in maintaining and downloading from the SafeBrowsing servers.
// For example, for digests of Malware binaries on Windows:
// platform_type = WINDOWS,
// threat_entry_type = EXECUTABLE,
// threat_type = MALWARE
struct UpdateListIdentifier {
 public:
  PlatformType platform_type;
  ThreatEntryType threat_entry_type;
  ThreatType threat_type;

  UpdateListIdentifier(PlatformType, ThreatEntryType, ThreatType);
  explicit UpdateListIdentifier(const ListUpdateResponse&);

  bool operator==(const UpdateListIdentifier& other) const;
  bool operator!=(const UpdateListIdentifier& other) const;
  size_t hash() const;

 private:
  UpdateListIdentifier();
};

std::ostream& operator<<(std::ostream& os, const UpdateListIdentifier& id);

// The set of interesting lists and ASCII filenames for their hash prefix
// stores. The stores are created inside the user-data directory.
// For instance, the UpdateListIdentifier could be for URL expressions for UwS
// on Windows platform, and the corresponding file on disk could be named:
// "uws_win_url.store"
// TODO(vakh): Find the canonical place where these are defined and update the
// comment to point to that place.
typedef base::hash_map<UpdateListIdentifier, std::string> StoreFileNameMap;

// Represents the state of each store.
typedef base::hash_map<UpdateListIdentifier, std::string> StoreStateMap;

// Sever response, parsed in vector form.
typedef std::vector<std::unique_ptr<ListUpdateResponse>> ParsedServerResponse;

// Enumerate failures for histogramming purposes.  DO NOT CHANGE THE
// ORDERING OF THESE VALUES.
enum V4OperationResult {
  // 200 response code means that the server recognized the request.
  STATUS_200 = 0,

  // Subset of successful responses where the response body wasn't parsable.
  PARSE_ERROR = 1,

  // Operation request failed (network error).
  NETWORK_ERROR = 2,

  // Operation request returned HTTP result code other than 200.
  HTTP_ERROR = 3,

  // Operation attempted during error backoff, no request sent.
  BACKOFF_ERROR = 4,

  // Operation attempted before min wait duration elapsed, no request sent.
  MIN_WAIT_DURATION_ERROR = 5,

  // Identical operation already pending.
  ALREADY_PENDING_ERROR = 6,

  // Memory space for histograms is determined by the max.  ALWAYS
  // ADD NEW VALUES BEFORE THIS ONE.
  OPERATION_RESULT_MAX = 7
};

// A class that provides static methods related to the Pver4 protocol.
class V4ProtocolManagerUtil {
 public:
  // Record HTTP response code when there's no error in fetching an HTTP
  // request, and the error code, when there is.
  // |metric_name| is the name of the UMA metric to record the response code or
  // error code against, |status| represents the status of the HTTP request, and
  // |response code| represents the HTTP response code received from the server.
  static void RecordHttpResponseOrErrorCode(const char* metric_name,
                                            const net::URLRequestStatus& status,
                                            int response_code);

  // Generates a Pver4 request URL and sets the appropriate header values.
  // |request_base64| is the serialized request protocol buffer encoded in
  // base 64.
  // |method_name| is the name of the method to call, as specified in the proto,
  // |config| is an instance of V4ProtocolConfig that stores the client config,
  // |gurl| is set to the value of the PVer4 request URL,
  // |headers| is populated with the appropriate header values.
  static void GetRequestUrlAndHeaders(const std::string& request_base64,
                                      const std::string& method_name,
                                      const V4ProtocolConfig& config,
                                      GURL* gurl,
                                      net::HttpRequestHeaders* headers);

  // Worker function for calculating the backoff times.
  // |multiplier| is doubled for each consecutive error after the
  // first, and |error_count| is incremented with each call.
  static base::TimeDelta GetNextBackOffInterval(size_t* error_count,
                                                size_t* multiplier);

 private:
  V4ProtocolManagerUtil(){};
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingV4ProtocolManagerUtilTest,
                           TestBackOffLogic);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingV4ProtocolManagerUtilTest,
                           TestGetRequestUrlAndUpdateHeaders);

  // Composes a URL using |prefix|, |method| (e.g.: encodedFullHashes).
  // |request_base64|, |client_id|, |version| and |key_param|. |prefix|
  // should contain the entire url prefix including scheme, host and path.
  static std::string ComposeUrl(const std::string& prefix,
                                const std::string& method,
                                const std::string& request_base64,
                                const std::string& key_param);

  // Sets the HTTP headers expected by a standard PVer4 request.
  static void UpdateHeaders(net::HttpRequestHeaders* headers);

  DISALLOW_COPY_AND_ASSIGN(V4ProtocolManagerUtil);
};

}  // namespace safe_browsing

namespace std {
template <>
struct hash<safe_browsing::UpdateListIdentifier> {
  std::size_t operator()(const safe_browsing::UpdateListIdentifier& s) const {
    return s.hash();
  }
};
}

#endif  // COMPONENTS_SAFE_BROWSING_DB_V4_PROTOCOL_MANAGER_UTIL_H_
