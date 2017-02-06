// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CERTIFICATE_TRANSPARENCY_SINGLE_TREE_TRACKER_H_
#define COMPONENTS_CERTIFICATE_TRANSPARENCY_SINGLE_TREE_TRACKER_H_

#include <map>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "net/cert/ct_verifier.h"
#include "net/cert/signed_tree_head.h"
#include "net/cert/sth_observer.h"

namespace net {
class CTLogVerifier;
class X509Certificate;

namespace ct {
struct SignedCertificateTimestamp;
}  // namespace ct

}  // namespace net

namespace certificate_transparency {

// Tracks the state of an individual Certificate Transparency Log's Merkle Tree.
// A CT Log constantly issues Signed Tree Heads, for which every older STH must
// be incorporated into the current/newer STH. As new certificates are logged,
// new SCTs are produced, and eventually, those SCTs are incorporated into the
// log and a new STH is produced, with there being an inclusion proof between
// the SCTs and the new STH, and a consistency proof between the old STH and the
// new STH.
// This class receives STHs provided by/observed by the embedder, with the
// assumption that STHs have been checked for consistency already. As SCTs are
// observed, their status is checked against the latest STH to ensure they were
// properly logged. If an SCT is newer than the latest STH, then this class
// verifies that when an STH is observed that should have incorporated those
// SCTs, the SCTs (and their corresponding entries) are present in the log.
//
// To accomplish this, this class needs to be notified of when new SCTs are
// observed (which it does by implementing net::CTVerifier::Observer) and when
// new STHs are observed (which it does by implementing net::ct::STHObserver).
// Once connected to sources providing that data, the status for a given SCT
// can be queried by calling GetLogEntryInclusionCheck.
class SingleTreeTracker : public net::CTVerifier::Observer,
                          public net::ct::STHObserver {
 public:
  // TODO(eranm): This enum will expand to include check success/failure,
  // see crbug.com/506227
  enum SCTInclusionStatus {
    // SCT was not observed by this class and is not currently pending
    // inclusion check. As there's no evidence the SCT this status relates
    // to is verified (it was never observed via OnSCTVerified), nothing
    // is done with it.
    SCT_NOT_OBSERVED,

    // SCT was observed but the STH known to this class is not old
    // enough to check for inclusion, so a newer STH is needed first.
    SCT_PENDING_NEWER_STH,

    // SCT is known and there's a new-enough STH to check inclusion against.
    // Actual inclusion check has to be performed.
    SCT_PENDING_INCLUSION_CHECK
  };

  explicit SingleTreeTracker(scoped_refptr<const net::CTLogVerifier> ct_log);
  ~SingleTreeTracker() override;

  // net::ct::CTVerifier::Observer implementation.

  // TODO(eranm): Extract CTVerifier::Observer to SCTObserver
  // Performs an inclusion check for the given certificate if the latest
  // STH known for this log is older than sct.timestamp + Maximum Merge Delay,
  // enqueues the SCT for future checking later on.
  // Should only be called with SCTs issued by the log this instance tracks.
  // TODO(eranm): Make sure not to perform any synchronous, blocking operation
  // here as this callback is invoked during certificate validation.
  void OnSCTVerified(net::X509Certificate* cert,
                     const net::ct::SignedCertificateTimestamp* sct) override;

  // net::ct::STHObserver implementation.
  // After verification of the signature over the |sth|, uses this
  // STH for future inclusion checks.
  // Must only be called for STHs issued by the log this instance tracks.
  void NewSTHObserved(const net::ct::SignedTreeHead& sth) override;

  // Returns the status of a given log entry that is assembled from
  // |cert| and |sct|. If |cert| and |sct| were not previously observed,
  // |sct| is not an SCT for |cert| or |sct| is not for this log,
  // SCT_NOT_OBSERVED will be returned.
  SCTInclusionStatus GetLogEntryInclusionStatus(
      net::X509Certificate* cert,
      const net::ct::SignedCertificateTimestamp* sct);

 private:
  // Holds the latest STH fetched and verified for this log.
  net::ct::SignedTreeHead verified_sth_;

  // The log being tracked.
  scoped_refptr<const net::CTLogVerifier> ct_log_;

  // List of log entries pending inclusion check.
  // TODO(eranm): Rather than rely on the timestamp, extend to to use the
  // whole MerkleTreeLeaf (RFC6962, section 3.4.) as a key. See
  // https://crbug.com/506227#c22 and https://crbug.com/613495
  std::map<base::Time, SCTInclusionStatus> entries_status_;

  DISALLOW_COPY_AND_ASSIGN(SingleTreeTracker);
};

}  // namespace certificate_transparency

#endif  // COMPONENTS_CERTIFICATE_TRANSPARENCY_SINGLE_TREE_TRACKER_H_
