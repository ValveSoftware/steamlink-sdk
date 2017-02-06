// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CERTIFICATE_TRANSPARENCY_LOG_PROOF_FETCHER_H_
#define COMPONENTS_CERTIFICATE_TRANSPARENCY_LOG_PROOF_FETCHER_H_

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace base {
class Value;
}  // namespace base

namespace net {

class URLRequestContext;

namespace ct {
struct SignedTreeHead;
}  // namespace ct

}  // namespace net

class GURL;

namespace certificate_transparency {

class LogResponseHandler;

// Fetches Signed Tree Heads (STHs) and consistency proofs from Certificate
// Transparency logs using the URLRequestContext provided during the instance
// construction.
// Must outlive the provided URLRequestContext.
class LogProofFetcher {
 public:
  // Buffer size for log replies - currently the reply to
  // get-consistency-proof is the biggest one this class handles. 1500 bytes
  // should be enough to accommodate 31 proof nodes + JSON overhead, supporting
  // trees with up to 100 million entries.
  static const size_t kMaxLogResponseSizeInBytes = 1500;

  // Callback for successful retrieval of Signed Tree Heads. Called
  // with the log_id of the log the STH belogs to (as supplied by the caller
  // to FetchSignedTreeHead) and the STH itself.
  using SignedTreeHeadFetchedCallback =
      base::Callback<void(const std::string& log_id,
                          const net::ct::SignedTreeHead& signed_tree_head)>;

  // Callback for failure of Signed Tree Head retrieval. Called with the log_id
  // that the log fetching was requested for and a net error code of the
  // failure.
  // |http_response_code| is meaningful only if |net_error| is net::OK.
  using FetchFailedCallback = base::Callback<
      void(const std::string& log_id, int net_error, int http_response_code)>;

  // Callback for successful retrieval of consistency proofs between two
  // STHs. Called with the log_id of the log the consistency belongs to (as
  // supplied by the caller to FetchConsistencyProof) and the vector of
  // proof nodes.
  using ConsistencyProofFetchedCallback =
      base::Callback<void(const std::string& log_id,
                          const std::vector<std::string>& consistency_proof)>;

  explicit LogProofFetcher(net::URLRequestContext* request_context);
  ~LogProofFetcher();

  // Fetch the latest Signed Tree Head from the log identified by |log_id|
  // from |base_log_url|. The |log_id| will be passed into the callbacks to
  // identify the log the retrieved Signed Tree Head belongs to.
  // The callbacks won't be invoked if the request is destroyed before
  // fetching is completed.
  // It is possible, but does not make a lot of sense, to have multiple
  // Signed Tree Head fetching requests going out to the same log, since
  // they are likely to return the same result.
  // TODO(eranm): Think further about whether multiple requests to the same
  // log imply cancellation of previous requests, should be coalesced or handled
  // independently.
  void FetchSignedTreeHead(
      const GURL& base_log_url,
      const std::string& log_id,
      const SignedTreeHeadFetchedCallback& fetched_callback,
      const FetchFailedCallback& failed_callback);

  // Fetch a consistency proof between the Merkle trees identified by
  // |old_tree_size| and |new_tree_size| of the log identified by |log_id|
  // from |base_log_url|.
  //
  // See the documentation of FetchSignedTreeHead regarding request destruction
  // and multiple requests to the same log.
  void FetchConsistencyProof(
      const GURL& base_log_url,
      const std::string& log_id,
      uint64_t old_tree_size,
      uint64_t new_tree_size,
      const ConsistencyProofFetchedCallback& fetched_callback,
      const FetchFailedCallback& failed_callback);

 private:
  // Starts the fetch (by delegating to the LogResponseHandler)
  // and stores the |log_handler| in |inflight_fetches_| for later
  // cleanup.
  void StartFetch(const GURL& request_url, LogResponseHandler* log_handler);

  // Callback for when the fetch was done (successfully or not).
  // Deletes, and removes, the |log_handler| from the |inflight_fetches_|.
  // Additionally, invokes |caller_callback| which is typically
  // one of the callbacks provided to the Fetch... method, bound to
  // success/failure parameters.
  void OnFetchDone(LogResponseHandler* log_handler,
                   const base::Closure& caller_callback);

  net::URLRequestContext* const request_context_;

  std::set<LogResponseHandler*> inflight_fetches_;

  base::WeakPtrFactory<LogProofFetcher> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(LogProofFetcher);
};

}  // namespace certificate_transparency

#endif  // COMPONENTS_CERTIFICATE_TRANSPARENCY_LOG_PROOF_FETCHER_H_
