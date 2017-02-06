// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_GCD_PRIVATE_PRIVET_V3_SESSION_H_
#define CHROME_BROWSER_EXTENSIONS_API_GCD_PRIVATE_PRIVET_V3_SESSION_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/extensions/api/gcd_private.h"
#include "net/url_request/url_fetcher.h"

namespace base {
class DictionaryValue;
}

namespace crypto {
class P224EncryptedKeyExchange;
}

namespace extensions {

class PrivetV3ContextGetter;

// Manages secure communication between browser and local Privet device.
class PrivetV3Session {
 private:
  class FetcherDelegate;

 public:
  typedef extensions::api::gcd_private::PairingType PairingType;
  typedef extensions::api::gcd_private::Status Result;

  typedef base::Callback<
      void(Result result, const base::DictionaryValue& response)> InitCallback;

  typedef base::Callback<void(Result result)> ResultCallback;
  typedef base::Callback<void(Result result,
                              const base::DictionaryValue& response)>
      MessageCallback;

  PrivetV3Session(const scoped_refptr<PrivetV3ContextGetter>& context_getter,
                  const net::HostPortPair& host_port);
  ~PrivetV3Session();

  // Initializes session. Queries /privet/info and returns supported pairing
  // types in callback.
  void Init(const InitCallback& callback);

  // Starts pairing by calling /privet/v3/pairing/start.
  void StartPairing(PairingType pairing_type, const ResultCallback& callback);

  // Confirms pairing code by calling /privet/v3/pairing/confirm.
  // Calls /privet/v3/pairing/auth after pairing.
  void ConfirmCode(const std::string& code, const ResultCallback& callback);

  // TODO(vitalybuka): Make HTTPS request to device with certificate validation.
  void SendMessage(const std::string& api,
                   const base::DictionaryValue& input,
                   const MessageCallback& callback);

 private:
  friend class PrivetV3SessionTest;
  FRIEND_TEST_ALL_PREFIXES(PrivetV3SessionTest, Pairing);
  FRIEND_TEST_ALL_PREFIXES(PrivetV3SessionTest, SendMessage);

  void OnInfoDone(const InitCallback& callback,
                  Result result,
                  const base::DictionaryValue& response);
  void OnPairingStartDone(const ResultCallback& callback,
                          Result result,
                          const base::DictionaryValue& response);
  void OnPairingConfirmDone(const ResultCallback& callback,
                            Result result,
                            const base::DictionaryValue& response);
  void OnPairedHostAddedToContext(const std::string& auth_code,
                                  const ResultCallback& callback);
  void OnAuthenticateDone(const ResultCallback& callback,
                          Result result,
                          const base::DictionaryValue& response);

  void RunCallback(const base::Closure& callback);
  void StartGetRequest(const std::string& api, const MessageCallback& callback);
  void StartPostRequest(const std::string& api,
                        const base::DictionaryValue& input,
                        const MessageCallback& callback);
  void SwitchToHttps();
  GURL CreatePrivetURL(const std::string& path) const;
  net::URLFetcher* CreateFetcher(const std::string& api,
                                 net::URLFetcher::RequestType request_type,
                                 const MessageCallback& callback);
  void DeleteFetcher(const FetcherDelegate* fetcher);
  void Cancel();

  // Endpoint for the current session.
  net::HostPortPair host_port_;

  // Provides context for client_.
  scoped_refptr<PrivetV3ContextGetter> context_getter_;

  // Current authentication token.
  std::string privet_auth_token_;

  // ID of the session received from pairing/start.
  std::string session_id_;

  // Device commitment received from pairing/start.
  std::string commitment_;

  // Key exchange algorithm for pairing.
  std::unique_ptr<crypto::P224EncryptedKeyExchange> spake_;

  // HTTPS port of the device.
  uint16_t https_port_ = 0;

  // If true, HTTPS will be used.
  bool use_https_ = false;

  // List of fetches to cancel when session is destroyed.
  ScopedVector<FetcherDelegate> fetchers_;

  // Intercepts POST requests. Used by tests only.
  base::Callback<void(const base::DictionaryValue&)> on_post_data_;

  base::WeakPtrFactory<PrivetV3Session> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(PrivetV3Session);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_GCD_PRIVATE_PRIVET_V3_SESSION_H_
