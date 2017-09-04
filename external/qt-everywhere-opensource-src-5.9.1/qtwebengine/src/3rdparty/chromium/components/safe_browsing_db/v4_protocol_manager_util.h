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
#include "base/strings/string_piece.h"
#include "components/safe_browsing_db/safebrowsing.pb.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

namespace net {
class HttpRequestHeaders;
class IPAddress;
}  // namespace net

namespace safe_browsing {

// The size of the hash prefix, in bytes. It should be between 4 to 32 (full
// hash).
typedef size_t PrefixSize;

// The minimum expected size (in bytes) of a hash-prefix.
const PrefixSize kMinHashPrefixLength = 4;

// The maximum expected size (in bytes) of a hash-prefix. This represents the
// length of a SHA256 hash.
const PrefixSize kMaxHashPrefixLength = 32;

// A hash prefix sent by the SafeBrowsing PVer4 service.
typedef std::string HashPrefix;

// A full SHA256 hash.
typedef HashPrefix FullHash;

typedef FetchThreatListUpdatesRequest::ListUpdateRequest ListUpdateRequest;
typedef FetchThreatListUpdatesResponse::ListUpdateResponse ListUpdateResponse;

// Config passed to the constructor of a V4 protocol manager.
struct V4ProtocolConfig {
  // The safe browsing client name sent in each request.
  std::string client_name;

  // Disable auto-updates using a command line switch.
  bool disable_auto_update;

  // The Google API key.
  std::string key_param;

  // Current product version sent in each request.
  std::string version;

  V4ProtocolConfig(const std::string& client_name,
                   bool disable_auto_update,
                   const std::string& key_param,
                   const std::string& version);
  V4ProtocolConfig(const V4ProtocolConfig& other);
  ~V4ProtocolConfig();

 private:
  V4ProtocolConfig();
};

// Different types of threats that SafeBrowsing protects against. This is the
// type that's returned to the clients of SafeBrowsing in Chromium.
enum SBThreatType {
  // This type can be used for lists that can be checked synchronously so a
  // client callback isn't required, or for whitelists.
  SB_THREAT_TYPE_UNUSED,

  // No threat at all.
  SB_THREAT_TYPE_SAFE,

  // The URL is being used for phishing.
  SB_THREAT_TYPE_URL_PHISHING,

  // The URL hosts malware.
  SB_THREAT_TYPE_URL_MALWARE,

  // The URL hosts unwanted programs.
  SB_THREAT_TYPE_URL_UNWANTED,

  // The download URL is malware.
  SB_THREAT_TYPE_BINARY_MALWARE_URL,

  // Url detected by the client-side phishing model.  Note that unlike the
  // above values, this does not correspond to a downloaded list.
  SB_THREAT_TYPE_CLIENT_SIDE_PHISHING_URL,

  // The Chrome extension or app (given by its ID) is malware.
  SB_THREAT_TYPE_EXTENSION,

  // Url detected by the client-side malware IP list. This IP list is part
  // of the client side detection model.
  SB_THREAT_TYPE_CLIENT_SIDE_MALWARE_URL,

  // Url leads to a blacklisted resource script. Note that no warnings should be
  // shown on this threat type, but an incident report might be sent.
  SB_THREAT_TYPE_BLACKLISTED_RESOURCE,

  // Url abuses a permission API.
  SB_THREAT_TYPE_API_ABUSE,
};

// The information required to uniquely identify each list the client is
// interested in maintaining and downloading from the SafeBrowsing servers.
// For example, for digests of Malware binaries on Windows:
// platform_type = WINDOWS,
// threat_entry_type = EXECUTABLE,
// threat_type = MALWARE
struct ListIdentifier {
 public:
  ListIdentifier(PlatformType, ThreatEntryType, ThreatType);
  explicit ListIdentifier(const ListUpdateResponse&);

  bool operator==(const ListIdentifier& other) const;
  bool operator!=(const ListIdentifier& other) const;
  size_t hash() const;

  PlatformType platform_type() const { return platform_type_; }
  ThreatEntryType threat_entry_type() const { return threat_entry_type_; }
  ThreatType threat_type() const { return threat_type_; }

 private:
  PlatformType platform_type_;
  ThreatEntryType threat_entry_type_;
  ThreatType threat_type_;

  ListIdentifier();
};

std::ostream& operator<<(std::ostream& os, const ListIdentifier& id);

PlatformType GetCurrentPlatformType();
const ListIdentifier GetCertCsdDownloadWhitelistId();
const ListIdentifier GetChromeExtMalwareId();
const ListIdentifier GetChromeUrlApiId();
const ListIdentifier GetChromeFilenameClientIncidentId();
const ListIdentifier GetChromeUrlClientIncidentId();
const ListIdentifier GetIpMalwareId();
const ListIdentifier GetUrlCsdDownloadWhitelistId();
const ListIdentifier GetUrlCsdWhitelistId();
const ListIdentifier GetUrlMalwareId();
const ListIdentifier GetUrlMalBinId();
const ListIdentifier GetUrlSocEngId();
const ListIdentifier GetUrlUwsId();

// Represents the state of each store.
typedef base::hash_map<ListIdentifier, std::string> StoreStateMap;

// Sever response, parsed in vector form.
typedef std::vector<std::unique_ptr<ListUpdateResponse>> ParsedServerResponse;

// Holds the hash prefix and the store that it matched in.
struct StoreAndHashPrefix {
 public:
  ListIdentifier list_id;
  HashPrefix hash_prefix;

  explicit StoreAndHashPrefix(ListIdentifier, HashPrefix);
  ~StoreAndHashPrefix();

  bool operator==(const StoreAndHashPrefix& other) const;
  bool operator!=(const StoreAndHashPrefix& other) const;
  size_t hash() const;

 private:
  StoreAndHashPrefix();
};

// Used to track the hash prefix and the store in which a full hash's prefix
// matched.
typedef std::vector<StoreAndHashPrefix> StoreAndHashPrefixes;

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
  // Canonicalizes url as per Google Safe Browsing Specification.
  // See: https://developers.google.com/safe-browsing/v4/urls-hashing
  static void CanonicalizeUrl(const GURL& url,
                              std::string* canonicalized_hostname,
                              std::string* canonicalized_path,
                              std::string* canonicalized_query);

  // This method returns the host suffix combinations from the hostname in the
  // URL, as described here:
  // https://developers.google.com/safe-browsing/v4/urls-hashing
  static void GenerateHostVariantsToCheck(const std::string& host,
                                          std::vector<std::string>* hosts);

  // This method returns the path prefix combinations from the path in the
  // URL, as described here:
  // https://developers.google.com/safe-browsing/v4/urls-hashing
  static void GeneratePathVariantsToCheck(const std::string& path,
                                          const std::string& query,
                                          std::vector<std::string>* paths);

  // Given a URL, returns all the patterns we need to check.
  static void GeneratePatternsToCheck(const GURL& url,
                                      std::vector<std::string>* urls);

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

  // Record HTTP response code when there's no error in fetching an HTTP
  // request, and the error code, when there is.
  // |metric_name| is the name of the UMA metric to record the response code or
  // error code against, |status| represents the status of the HTTP request, and
  // |response code| represents the HTTP response code received from the server.
  static void RecordHttpResponseOrErrorCode(const char* metric_name,
                                            const net::URLRequestStatus& status,
                                            int response_code);

  // Generate the set of FullHashes to check for |url|.
  static void UrlToFullHashes(const GURL& url,
                              std::vector<FullHash>* full_hashes);

  static bool FullHashToHashPrefix(const FullHash& full_hash,
                                   PrefixSize prefix_size,
                                   HashPrefix* hash_prefix);

  static bool FullHashToSmallestHashPrefix(const FullHash& full_hash,
                                           HashPrefix* hash_prefix);

  static bool FullHashMatchesHashPrefix(const FullHash& full_hash,
                                        const HashPrefix& hash_prefix);

  static void SetClientInfoFromConfig(ClientInfo* client_info,
                                      const V4ProtocolConfig& config);

  static bool GetIPV6AddressFromString(const std::string& ip_address,
                                       net::IPAddress* address);

  // Converts a IPV4 or IPV6 address in |ip_address| to the SHA1 hash of the
  // corresponding packed IPV6 address in |hashed_encoded_ip|, and adds an
  // extra byte containing the value 128 at the end. This is done to match the
  // server implementation for calculating the hash prefix of an IP address.
  static bool IPAddressToEncodedIPV6Hash(const std::string& ip_address,
                                         FullHash* hashed_encoded_ip);

 private:
  V4ProtocolManagerUtil(){};
  FRIEND_TEST_ALL_PREFIXES(V4ProtocolManagerUtilTest, TestBackOffLogic);
  FRIEND_TEST_ALL_PREFIXES(V4ProtocolManagerUtilTest,
                           TestGetRequestUrlAndUpdateHeaders);
  FRIEND_TEST_ALL_PREFIXES(V4ProtocolManagerUtilTest, UrlParsing);
  FRIEND_TEST_ALL_PREFIXES(V4ProtocolManagerUtilTest, CanonicalizeUrl);

  // Composes a URL using |prefix|, |method| (e.g.: encodedFullHashes).
  // |request_base64|, |client_id|, |version| and |key_param|. |prefix|
  // should contain the entire url prefix including scheme, host and path.
  static std::string ComposeUrl(const std::string& prefix,
                                const std::string& method,
                                const std::string& request_base64,
                                const std::string& key_param);

  // Sets the HTTP headers expected by a standard PVer4 request.
  static void UpdateHeaders(net::HttpRequestHeaders* headers);

  // Given a URL, returns all the hosts we need to check.  They are returned
  // in order of size (i.e. b.c is first, then a.b.c).
  static void GenerateHostsToCheck(const GURL& url,
                                   std::vector<std::string>* hosts);

  // Given a URL, returns all the paths we need to check.
  static void GeneratePathsToCheck(const GURL& url,
                                   std::vector<std::string>* paths);

  static std::string RemoveConsecutiveChars(base::StringPiece str,
                                            const char c);

  DISALLOW_COPY_AND_ASSIGN(V4ProtocolManagerUtil);
};

typedef std::unordered_set<ListIdentifier> StoresToCheck;

}  // namespace safe_browsing

namespace std {
template <>
struct hash<safe_browsing::PlatformType> {
  std::size_t operator()(const safe_browsing::PlatformType& p) const {
    return std::hash<unsigned int>()(p);
  }
};

template <>
struct hash<safe_browsing::ThreatEntryType> {
  std::size_t operator()(const safe_browsing::ThreatEntryType& tet) const {
    return std::hash<unsigned int>()(tet);
  }
};

template <>
struct hash<safe_browsing::ThreatType> {
  std::size_t operator()(const safe_browsing::ThreatType& tt) const {
    return std::hash<unsigned int>()(tt);
  }
};

template <>
struct hash<safe_browsing::ListIdentifier> {
  std::size_t operator()(const safe_browsing::ListIdentifier& id) const {
    return id.hash();
  }
};
}

#endif  // COMPONENTS_SAFE_BROWSING_DB_V4_PROTOCOL_MANAGER_UTIL_H_
