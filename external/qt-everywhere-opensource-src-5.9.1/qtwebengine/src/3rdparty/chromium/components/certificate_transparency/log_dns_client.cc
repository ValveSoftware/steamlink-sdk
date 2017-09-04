// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/certificate_transparency/log_dns_client.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/format_macros.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/base32/base32.h"
#include "crypto/sha2.h"
#include "net/cert/merkle_audit_proof.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_config_service.h"
#include "net/dns/dns_protocol.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_transaction.h"
#include "net/dns/record_parsed.h"
#include "net/dns/record_rdata.h"

namespace certificate_transparency {

namespace {

// Parses the DNS response and extracts a single string from the TXT RDATA.
// If the response is malformed, not a TXT record, or contains any number of
// strings other than 1, this returns false and extracts nothing.
// Otherwise, it returns true and the extracted string is assigned to |*txt|.
bool ParseTxtResponse(const net::DnsResponse& response, std::string* txt) {
  DCHECK(txt);

  net::DnsRecordParser parser = response.Parser();
  // We don't care about the creation time, since we're going to throw
  // |parsed_record| away as soon as we've extracted the payload, so provide
  // the "null" time.
  auto parsed_record = net::RecordParsed::CreateFrom(&parser, base::Time());
  if (!parsed_record)
    return false;

  auto* txt_record = parsed_record->rdata<net::TxtRecordRdata>();
  if (!txt_record)
    return false;

  // The draft CT-over-DNS RFC says that there MUST be exactly one string in the
  // TXT record.
  if (txt_record->texts().size() != 1)
    return false;

  *txt = txt_record->texts().front();
  return true;
}

// Extracts a leaf index value from a DNS response's TXT RDATA.
// Returns true on success, false otherwise.
bool ParseLeafIndex(const net::DnsResponse& response, uint64_t* index) {
  DCHECK(index);

  std::string index_str;
  if (!ParseTxtResponse(response, &index_str))
    return false;

  return base::StringToUint64(index_str, index);
}

// Extracts audit proof nodes from a DNS response's TXT RDATA.
// Returns true on success, false otherwise.
// It will fail if there is not a whole number of nodes present > 0.
// There must only be one string in the TXT RDATA.
// The nodes will be appended to |proof->nodes|
bool ParseAuditPath(const net::DnsResponse& response,
                    net::ct::MerkleAuditProof* proof) {
  DCHECK(proof);

  std::string audit_path;
  if (!ParseTxtResponse(response, &audit_path))
    return false;
  // If empty or not a multiple of the node size, it is considered invalid.
  // It's important to consider empty audit paths as invalid, as otherwise an
  // infinite loop could occur if the server consistently returned empty
  // responses.
  if (audit_path.empty() || audit_path.size() % crypto::kSHA256Length != 0)
    return false;

  for (size_t i = 0; i < audit_path.size(); i += crypto::kSHA256Length) {
    proof->nodes.push_back(audit_path.substr(i, crypto::kSHA256Length));
  }

  return true;
}

}  // namespace

// Encapsulates the state machine required to get an audit proof from a Merkle
// leaf hash. This requires a DNS request to obtain the leaf index, then a
// series of DNS requests to get the nodes of the proof.
class LogDnsClient::AuditProofQuery {
 public:
  // The LogDnsClient is guaranteed to outlive the AuditProofQuery, so it's safe
  // to leave ownership of |dns_client| with LogDnsClient.
  AuditProofQuery(net::DnsClient* dns_client,
                  const std::string& domain_for_log,
                  const net::NetLogWithSource& net_log);

  // Begins the process of getting an audit proof for the CT log entry with a
  // leaf hash of |leaf_hash|. The proof will be for a tree of size |tree_size|.
  // If it cannot be obtained synchronously, net::ERR_IO_PENDING will be
  // returned and |callback| will be invoked when the operation has completed
  // asynchronously. Ownership of |proof| remains with the caller, and it must
  // not be deleted until the operation is complete.
  net::Error Start(std::string leaf_hash,
                   uint64_t tree_size,
                   const net::CompletionCallback& callback,
                   net::ct::MerkleAuditProof* out_proof);

 private:
  enum class State {
    NONE,
    REQUEST_LEAF_INDEX,
    REQUEST_LEAF_INDEX_COMPLETE,
    REQUEST_AUDIT_PROOF_NODES,
    REQUEST_AUDIT_PROOF_NODES_COMPLETE,
  };

  net::Error DoLoop(net::Error result);

  // When a DnsTransaction completes, store the response and resume the state
  // machine. It is safe to store a pointer to |response| because |transaction|
  // is kept alive in |current_dns_transaction_|.
  void OnDnsTransactionComplete(net::DnsTransaction* transaction,
                                int net_error,
                                const net::DnsResponse* response);

  // Requests the leaf index for the CT log entry with |leaf_hash_|.
  net::Error RequestLeafIndex();

  // Stores the received leaf index in |proof_->leaf_index|.
  // If successful, the audit proof nodes will be requested next.
  net::Error RequestLeafIndexComplete(net::Error result);

  // Requests the next batch of audit proof nodes from a CT log.
  // The index of the first node required is determined by looking at how many
  // nodes are already in |proof_->nodes|.
  // The CT log may return up to 7 nodes - this is the maximum allowed by the
  // CT-over-DNS draft RFC, as a TXT RDATA string can have a maximum length of
  // 255 bytes and each node is 32 bytes long (a SHA-256 hash).
  //
  // The performance of this could be improved by sending all of the expected
  // requests up front. Each response can contain a maximum of 7 audit path
  // nodes, so for an audit proof of size 20, it could send 3 queries (for nodes
  // 0-6, 7-13 and 14-19) immediately. Currently, it sends only the first and
  // then, based on the number of nodes received, sends the next query.
  // The complexity of the code would increase though, as it would need to
  // detect gaps in the audit proof caused by the server not responding with the
  // anticipated number of nodes. It would also undermine LogDnsClient's ability
  // to rate-limit DNS requests.
  net::Error RequestAuditProofNodes();

  // Appends the received audit proof nodes to |proof_->nodes|.
  // If any nodes are missing, another request will follow this one.
  net::Error RequestAuditProofNodesComplete(net::Error result);

  // Sends a TXT record request for the domain |qname|.
  // Returns true if the request could be started.
  // OnDnsTransactionComplete() will be invoked with the result of the request.
  bool StartDnsTransaction(const std::string& qname);

  // The next state that this query will enter.
  State next_state_;
  // The DNS domain of the CT log that is being queried.
  std::string domain_for_log_;
  // The Merkle leaf hash of the CT log entry an audit proof is required for.
  std::string leaf_hash_;
  // The audit proof to populate.
  net::ct::MerkleAuditProof* proof_;
  // The callback to invoke when the query is complete.
  net::CompletionCallback callback_;
  // The DnsClient to use for sending DNS requests to the CT log.
  net::DnsClient* dns_client_;
  // The most recent DNS request. Null if no DNS requests have been made.
  std::unique_ptr<net::DnsTransaction> current_dns_transaction_;
  // The most recent DNS response. Only valid so long as the corresponding DNS
  // request is stored in |current_dns_transaction_|.
  const net::DnsResponse* last_dns_response_;
  // The NetLog that DNS transactions will log to.
  net::NetLogWithSource net_log_;
  // Produces WeakPtrs to |this| for binding callbacks.
  base::WeakPtrFactory<AuditProofQuery> weak_ptr_factory_;
};

LogDnsClient::AuditProofQuery::AuditProofQuery(
    net::DnsClient* dns_client,
    const std::string& domain_for_log,
    const net::NetLogWithSource& net_log)
    : next_state_(State::NONE),
      domain_for_log_(domain_for_log),
      dns_client_(dns_client),
      net_log_(net_log),
      weak_ptr_factory_(this) {
  DCHECK(dns_client_);
  DCHECK(!domain_for_log_.empty());
}

// |leaf_hash| is not a const-ref to allow callers to std::move that string into
// the method, avoiding the need to make a copy.
net::Error LogDnsClient::AuditProofQuery::Start(
    std::string leaf_hash,
    uint64_t tree_size,
    const net::CompletionCallback& callback,
    net::ct::MerkleAuditProof* proof) {
  // It should not already be in progress.
  DCHECK_EQ(State::NONE, next_state_);
  proof_ = proof;
  proof_->tree_size = tree_size;
  leaf_hash_ = std::move(leaf_hash);
  callback_ = callback;
  // The first step in the query is to request the leaf index corresponding to
  // |leaf_hash| from the CT log.
  next_state_ = State::REQUEST_LEAF_INDEX;
  // Begin the state machine.
  return DoLoop(net::OK);
}

net::Error LogDnsClient::AuditProofQuery::DoLoop(net::Error result) {
  CHECK_NE(State::NONE, next_state_);
  do {
    State state = next_state_;
    next_state_ = State::NONE;
    switch (state) {
      case State::REQUEST_LEAF_INDEX:
        result = RequestLeafIndex();
        break;
      case State::REQUEST_LEAF_INDEX_COMPLETE:
        result = RequestLeafIndexComplete(result);
        break;
      case State::REQUEST_AUDIT_PROOF_NODES:
        result = RequestAuditProofNodes();
        break;
      case State::REQUEST_AUDIT_PROOF_NODES_COMPLETE:
        result = RequestAuditProofNodesComplete(result);
        break;
      case State::NONE:
        NOTREACHED();
        break;
    }
  } while (result != net::ERR_IO_PENDING && next_state_ != State::NONE);

  return result;
}

void LogDnsClient::AuditProofQuery::OnDnsTransactionComplete(
    net::DnsTransaction* transaction,
    int net_error,
    const net::DnsResponse* response) {
  DCHECK_EQ(current_dns_transaction_.get(), transaction);
  last_dns_response_ = response;
  net::Error result = DoLoop(static_cast<net::Error>(net_error));

  // If DoLoop() indicates that I/O is pending, don't invoke the completion
  // callback. OnDnsTransactionComplete() will be invoked again once the I/O
  // is complete, and can invoke the completion callback then if appropriate.
  if (result != net::ERR_IO_PENDING) {
    // The callback will delete this query (now that it has finished), so copy
    // |callback_| before running it so that it is not deleted along with the
    // query, mid-callback-execution (which would result in a crash).
    base::ResetAndReturn(&callback_).Run(result);
  }
}

net::Error LogDnsClient::AuditProofQuery::RequestLeafIndex() {
  std::string encoded_leaf_hash = base32::Base32Encode(
      leaf_hash_, base32::Base32EncodePolicy::OMIT_PADDING);
  DCHECK_EQ(encoded_leaf_hash.size(), 52u);

  std::string qname = base::StringPrintf(
      "%s.hash.%s.", encoded_leaf_hash.c_str(), domain_for_log_.c_str());

  if (!StartDnsTransaction(qname)) {
    return net::ERR_NAME_RESOLUTION_FAILED;
  }

  next_state_ = State::REQUEST_LEAF_INDEX_COMPLETE;
  return net::ERR_IO_PENDING;
}

// Stores the received leaf index in |proof_->leaf_index|.
// If successful, the audit proof nodes will be requested next.
net::Error LogDnsClient::AuditProofQuery::RequestLeafIndexComplete(
    net::Error result) {
  if (result != net::OK) {
    return result;
  }

  DCHECK(last_dns_response_);
  if (!ParseLeafIndex(*last_dns_response_, &proof_->leaf_index)) {
    return net::ERR_DNS_MALFORMED_RESPONSE;
  }

  // Reject leaf index if it is out-of-range.
  // This indicates either:
  // a) the wrong tree_size was provided.
  // b) the wrong leaf hash was provided.
  // c) there is a bug server-side.
  // The first two are more likely, so return ERR_INVALID_ARGUMENT.
  if (proof_->leaf_index >= proof_->tree_size) {
    return net::ERR_INVALID_ARGUMENT;
  }

  next_state_ = State::REQUEST_AUDIT_PROOF_NODES;
  return net::OK;
}

net::Error LogDnsClient::AuditProofQuery::RequestAuditProofNodes() {
  // Test pre-conditions (should be guaranteed by DNS response validation).
  if (proof_->leaf_index >= proof_->tree_size ||
      proof_->nodes.size() >= net::ct::CalculateAuditPathLength(
                                  proof_->leaf_index, proof_->tree_size)) {
    return net::ERR_UNEXPECTED;
  }

  std::string qname = base::StringPrintf(
      "%zu.%" PRIu64 ".%" PRIu64 ".tree.%s.", proof_->nodes.size(),
      proof_->leaf_index, proof_->tree_size, domain_for_log_.c_str());

  if (!StartDnsTransaction(qname)) {
    return net::ERR_NAME_RESOLUTION_FAILED;
  }

  next_state_ = State::REQUEST_AUDIT_PROOF_NODES_COMPLETE;
  return net::ERR_IO_PENDING;
}

net::Error LogDnsClient::AuditProofQuery::RequestAuditProofNodesComplete(
    net::Error result) {
  if (result != net::OK) {
    return result;
  }

  const uint64_t audit_path_length =
      net::ct::CalculateAuditPathLength(proof_->leaf_index, proof_->tree_size);

  // The calculated |audit_path_length| can't ever be greater than 64, so
  // deriving the amount of memory to reserve from the untrusted |leaf_index|
  // is safe.
  proof_->nodes.reserve(audit_path_length);

  DCHECK(last_dns_response_);
  if (!ParseAuditPath(*last_dns_response_, proof_)) {
    return net::ERR_DNS_MALFORMED_RESPONSE;
  }

  // Keep requesting more proof nodes until all of them are received.
  if (proof_->nodes.size() < audit_path_length) {
    next_state_ = State::REQUEST_AUDIT_PROOF_NODES;
  }

  return net::OK;
}

bool LogDnsClient::AuditProofQuery::StartDnsTransaction(
    const std::string& qname) {
  net::DnsTransactionFactory* factory = dns_client_->GetTransactionFactory();
  if (!factory) {
    return false;
  }

  current_dns_transaction_ = factory->CreateTransaction(
      qname, net::dns_protocol::kTypeTXT,
      base::Bind(&LogDnsClient::AuditProofQuery::OnDnsTransactionComplete,
                 weak_ptr_factory_.GetWeakPtr()),
      net_log_);

  current_dns_transaction_->Start();
  return true;
}

LogDnsClient::LogDnsClient(std::unique_ptr<net::DnsClient> dns_client,
                           const net::NetLogWithSource& net_log,
                           size_t max_concurrent_queries)
    : dns_client_(std::move(dns_client)),
      net_log_(net_log),
      max_concurrent_queries_(max_concurrent_queries),
      weak_ptr_factory_(this) {
  CHECK(dns_client_);
  net::NetworkChangeNotifier::AddDNSObserver(this);
  UpdateDnsConfig();
}

LogDnsClient::~LogDnsClient() {
  net::NetworkChangeNotifier::RemoveDNSObserver(this);
}

void LogDnsClient::OnDNSChanged() {
  UpdateDnsConfig();
}

void LogDnsClient::OnInitialDNSConfigRead() {
  UpdateDnsConfig();
}

void LogDnsClient::NotifyWhenNotThrottled(const base::Closure& callback) {
  DCHECK(HasMaxConcurrentQueriesInProgress());
  not_throttled_callbacks_.push_back(callback);
}

// |leaf_hash| is not a const-ref to allow callers to std::move that string into
// the method, avoiding LogDnsClient::AuditProofQuery having to make a copy.
net::Error LogDnsClient::QueryAuditProof(
    base::StringPiece domain_for_log,
    std::string leaf_hash,
    uint64_t tree_size,
    net::ct::MerkleAuditProof* proof,
    const net::CompletionCallback& callback) {
  DCHECK(proof);

  if (domain_for_log.empty() || leaf_hash.size() != crypto::kSHA256Length) {
    return net::ERR_INVALID_ARGUMENT;
  }

  if (HasMaxConcurrentQueriesInProgress()) {
    return net::ERR_TEMPORARILY_THROTTLED;
  }

  AuditProofQuery* query = new AuditProofQuery(
      dns_client_.get(), domain_for_log.as_string(), net_log_);
  // Transfers ownership of |query| to |audit_proof_queries_|.
  audit_proof_queries_.emplace_back(query);

  return query->Start(std::move(leaf_hash), tree_size,
                      base::Bind(&LogDnsClient::QueryAuditProofComplete,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 base::Unretained(query), callback),
                      proof);
}

void LogDnsClient::QueryAuditProofComplete(
    AuditProofQuery* query,
    const net::CompletionCallback& callback,
    int net_error) {
  DCHECK(query);

  // Finished with the query - destroy it.
  auto query_iterator =
      std::find_if(audit_proof_queries_.begin(), audit_proof_queries_.end(),
                   [query](const std::unique_ptr<AuditProofQuery>& p) {
                     return p.get() == query;
                   });
  DCHECK(query_iterator != audit_proof_queries_.end());
  audit_proof_queries_.erase(query_iterator);

  callback.Run(net_error);

  // Notify interested parties that the next query will not be throttled.
  std::list<base::Closure> callbacks = std::move(not_throttled_callbacks_);
  for (const base::Closure& callback : callbacks) {
    callback.Run();
  }
}

bool LogDnsClient::HasMaxConcurrentQueriesInProgress() const {
  return max_concurrent_queries_ != 0 &&
         audit_proof_queries_.size() >= max_concurrent_queries_;
}

void LogDnsClient::UpdateDnsConfig() {
  net::DnsConfig config;
  net::NetworkChangeNotifier::GetDnsConfig(&config);
  if (config.IsValid())
    dns_client_->SetConfig(config);
}

}  // namespace certificate_transparency
