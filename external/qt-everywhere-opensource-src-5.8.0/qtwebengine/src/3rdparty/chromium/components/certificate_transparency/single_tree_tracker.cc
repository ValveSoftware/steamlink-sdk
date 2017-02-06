// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/certificate_transparency/single_tree_tracker.h"

#include <utility>

#include "net/cert/ct_log_verifier.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/cert/x509_certificate.h"

using net::ct::SignedTreeHead;

namespace certificate_transparency {

SingleTreeTracker::SingleTreeTracker(
    scoped_refptr<const net::CTLogVerifier> ct_log)
    : ct_log_(std::move(ct_log)) {}

SingleTreeTracker::~SingleTreeTracker() {}

void SingleTreeTracker::OnSCTVerified(
    net::X509Certificate* cert,
    const net::ct::SignedCertificateTimestamp* sct) {
  DCHECK_EQ(ct_log_->key_id(), sct->log_id);

  // SCT was previously observed, so its status should not be changed.
  if (entries_status_.find(sct->timestamp) != entries_status_.end())
    return;

  // If there isn't a valid STH or the STH is not fresh enough to check
  // inclusion against, store the SCT for future checking and return.
  if (verified_sth_.timestamp.is_null() ||
      (verified_sth_.timestamp <
       (sct->timestamp + base::TimeDelta::FromHours(24)))) {
    // TODO(eranm): UMA - how often SCTs have to wait for a newer STH for
    // inclusion check.
    entries_status_.insert(
        std::make_pair(sct->timestamp, SCT_PENDING_NEWER_STH));
    return;
  }

  // TODO(eranm): Check inclusion here.
  // TODO(eranm): UMA - how often inclusion can be checked immediately.
  entries_status_.insert(
      std::make_pair(sct->timestamp, SCT_PENDING_INCLUSION_CHECK));
}

void SingleTreeTracker::NewSTHObserved(const SignedTreeHead& sth) {
  DCHECK_EQ(ct_log_->key_id(), sth.log_id);

  if (!ct_log_->VerifySignedTreeHead(sth)) {
    // Sanity check the STH; the caller should have done this
    // already, but being paranoid here.
    // NOTE(eranm): Right now there's no way to get rid of this check here
    // as this is the first object in the chain that has an instance of
    // a CTLogVerifier to verify the STH.
    return;
  }

  // In order to avoid updating |verified_sth_| to an older STH in case
  // an older STH is observed, check that either the observed STH is for
  // a larger tree size or that it is for the same tree size but has
  // a newer timestamp.
  const bool sths_for_same_tree = verified_sth_.tree_size == sth.tree_size;
  const bool received_sth_is_for_larger_tree =
      (verified_sth_.tree_size > sth.tree_size);
  const bool received_sth_is_newer = (sth.timestamp > verified_sth_.timestamp);

  if (verified_sth_.timestamp.is_null() || received_sth_is_for_larger_tree ||
      (sths_for_same_tree && received_sth_is_newer)) {
    verified_sth_ = sth;
  }

  // Find out which SCTs can now be checked for inclusion.
  // TODO(eranm): Keep two maps of MerkleTreeLeaf instances, one for leaves
  // pending inclusion checks and one for leaves pending a new STH.
  // The comparison function between MerkleTreeLeaf instances should use the
  // timestamp to determine sorting order, so that bulk moving from one
  // map to the other can happen.
  auto entry = entries_status_.begin();
  while (entry != entries_status_.end() &&
         entry->first < verified_sth_.timestamp) {
    entry->second = SCT_PENDING_INCLUSION_CHECK;
    ++entry;
    // TODO(eranm): Check inclusion here.
  }
}

SingleTreeTracker::SCTInclusionStatus
SingleTreeTracker::GetLogEntryInclusionStatus(
    net::X509Certificate* cert,
    const net::ct::SignedCertificateTimestamp* sct) {
  auto it = entries_status_.find(sct->timestamp);

  return it == entries_status_.end() ? SCT_NOT_OBSERVED : it->second;
}

}  // namespace certificate_transparency
