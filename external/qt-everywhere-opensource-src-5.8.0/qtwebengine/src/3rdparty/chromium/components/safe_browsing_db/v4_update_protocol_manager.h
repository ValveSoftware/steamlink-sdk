// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_DB_V4_UPDATE_PROTOCOL_MANAGER_H_
#define COMPONENTS_SAFE_BROWSING_DB_V4_UPDATE_PROTOCOL_MANAGER_H_

// A class that implements Chrome's interface with the SafeBrowsing V4 update
// protocol.
//
// The V4UpdateProtocolManager handles formatting and making requests of, and
// handling responses from, Google's SafeBrowsing servers. The purpose of this
// class is to get hash prefixes from the SB server for the given set of lists.

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/safe_browsing_db/safebrowsing.pb.h"
#include "components/safe_browsing_db/util.h"
#include "components/safe_browsing_db/v4_protocol_manager_util.h"
#include "net/url_request/url_fetcher_delegate.h"

class GURL;

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}  // namespace net

namespace safe_browsing {

class V4UpdateProtocolManagerFactory;

// V4UpdateCallback is invoked when a scheduled update completes.
// Parameters:
//   - The vector of update response protobufs received from the server for
//     each list type.
typedef base::Callback<void(std::unique_ptr<ParsedServerResponse>)>
    V4UpdateCallback;

class V4UpdateProtocolManager : public net::URLFetcherDelegate,
                                public base::NonThreadSafe {
 public:
  ~V4UpdateProtocolManager() override;

  // Makes the passed |factory| the factory used to instantiate
  // a V4UpdateProtocolManager. Useful for tests.
  static void RegisterFactory(V4UpdateProtocolManagerFactory* factory) {
    factory_ = factory;
  }

  // Create an instance of the safe browsing v4 protocol manager.
  static std::unique_ptr<V4UpdateProtocolManager> Create(
      net::URLRequestContextGetter* request_context_getter,
      const V4ProtocolConfig& config,
      V4UpdateCallback callback);

  // net::URLFetcherDelegate interface.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // Schedule the next update without backoff.
  void ScheduleNextUpdate(std::unique_ptr<StoreStateMap> store_state_map);

 protected:
  // Constructs a V4UpdateProtocolManager that issues network requests using
  // |request_context_getter|. It schedules updates to get the hash prefixes for
  // SafeBrowsing lists, and invoke |callback| when the results are retrieved.
  // The callback may be invoked synchronously.
  V4UpdateProtocolManager(
      net::URLRequestContextGetter* request_context_getter,
      const V4ProtocolConfig& config,
      V4UpdateCallback callback);

 private:
  FRIEND_TEST_ALL_PREFIXES(V4UpdateProtocolManagerTest,
                           TestGetUpdatesErrorHandlingNetwork);
  FRIEND_TEST_ALL_PREFIXES(V4UpdateProtocolManagerTest,
                           TestGetUpdatesErrorHandlingResponseCode);
  FRIEND_TEST_ALL_PREFIXES(V4UpdateProtocolManagerTest, TestGetUpdatesNoError);
  FRIEND_TEST_ALL_PREFIXES(V4UpdateProtocolManagerTest,
                           TestGetUpdatesWithOneBackoff);
  FRIEND_TEST_ALL_PREFIXES(V4UpdateProtocolManagerTest,
                           TestBase64EncodingUsesUrlEncoding);
  friend class V4UpdateProtocolManagerFactoryImpl;

  // The method to populate |gurl| with the URL to be sent to the server.
  // |request_base64| is the base64 encoded form of an instance of the protobuf
  // FetchThreatListUpdatesRequest. Also sets the appropriate header values for
  // sending PVer4 requests in |headers|.
  void GetUpdateUrlAndHeaders(const std::string& request_base64,
                              GURL* gurl,
                              net::HttpRequestHeaders* headers) const;

  // Fills a FetchThreatListUpdatesRequest protocol buffer for a request.
  // Returns the serialized and base64 URL encoded request as a string.
  static std::string GetBase64SerializedUpdateRequestProto(
      const StoreStateMap& store_state_map);

  // Parses the base64 encoded response received from the server as a
  // FetchThreatListUpdatesResponse protobuf and returns each of the
  // ListUpdateResponse protobufs contained in it as a vector.
  // Returns true if parsing is successful, false otherwise.
  bool ParseUpdateResponse(const std::string& data_base64,
                           ParsedServerResponse* parsed_server_response);

  // Resets the update error counter and multiplier.
  void ResetUpdateErrors();

  // Updates internal update and backoff state for each update response error,
  // assuming that the current time is |now|.
  void HandleUpdateError(const base::Time& now);

  // Generates the URL for the update request and issues the request for the
  // lists passed to the constructor.
  void IssueUpdateRequest();

  // Returns whether another update is currently scheduled.
  bool IsUpdateScheduled() const;

  // Schedule the next update with backoff specified.
  void ScheduleNextUpdateWithBackoff(bool back_off);

  // Schedule the next update, after the given interval.
  void ScheduleNextUpdateAfterInterval(base::TimeDelta interval);

  // Get the next update interval, considering whether we are in backoff.
  base::TimeDelta GetNextUpdateInterval(bool back_off);

  // The factory that controls the creation of V4UpdateProtocolManager.
  // This is used by tests.
  static V4UpdateProtocolManagerFactory* factory_;

  // The last known state of the lists.
  // Updated after every successful update of the database.
  std::unique_ptr<StoreStateMap> store_state_map_;

  // The number of HTTP response errors since the the last successful HTTP
  // response, used for request backoff timing.
  size_t update_error_count_;

  // Multiplier for the backoff error after the second.
  size_t update_back_off_mult_;

  // The time delta after which the next update request may be sent.
  // It is set to a random interval between 60 and 300 seconds at start.
  // The server can set it by setting the minimum_wait_duration.
  base::TimeDelta next_update_interval_;

  // The config of the client making Pver4 requests.
  const V4ProtocolConfig config_;

  // The context we use to issue network requests.
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;

  // ID for URLFetchers for testing.
  int url_fetcher_id_;

  // The callback that's called when GetUpdates completes.
  V4UpdateCallback update_callback_;

  // The pending update request. The request must be canceled when the object is
  // destroyed.
  std::unique_ptr<net::URLFetcher> request_;

  // Timer to setup the next update request.
  base::OneShotTimer update_timer_;

  base::Time last_response_time_;

  DISALLOW_COPY_AND_ASSIGN(V4UpdateProtocolManager);
};

// Interface of a factory to create V4UpdateProtocolManager.  Useful for tests.
class V4UpdateProtocolManagerFactory {
 public:
  V4UpdateProtocolManagerFactory() {}
  virtual ~V4UpdateProtocolManagerFactory() {}
  virtual std::unique_ptr<V4UpdateProtocolManager> CreateProtocolManager(
      net::URLRequestContextGetter* request_context_getter,
      const V4ProtocolConfig& config,
      V4UpdateCallback callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(V4UpdateProtocolManagerFactory);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_DB_V4_UPDATE_PROTOCOL_MANAGER_H_
