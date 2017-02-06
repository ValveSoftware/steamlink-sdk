// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/certificate_transparency/single_tree_tracker.h"

#include <string>
#include <utility>

#include "base/strings/string_piece.h"
#include "net/cert/ct_log_verifier.h"
#include "net/cert/ct_serialization.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/cert/signed_tree_head.h"
#include "net/cert/x509_certificate.h"
#include "net/test/ct_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace certificate_transparency {

namespace {

bool GetOldSignedTreeHead(net::ct::SignedTreeHead* sth) {
  sth->version = net::ct::SignedTreeHead::V1;
  sth->timestamp = base::Time::UnixEpoch() +
                   base::TimeDelta::FromMilliseconds(INT64_C(1348589665525));
  sth->tree_size = 12u;

  const uint8_t kOldSTHRootHash[] = {
      0x18, 0x04, 0x1b, 0xd4, 0x66, 0x50, 0x83, 0x00, 0x1f, 0xba, 0x8c,
      0x54, 0x11, 0xd2, 0xd7, 0x48, 0xe8, 0xab, 0xbf, 0xdc, 0xdf, 0xd9,
      0x21, 0x8c, 0xb0, 0x2b, 0x68, 0xa7, 0x8e, 0x7d, 0x4c, 0x23};
  memcpy(sth->sha256_root_hash, kOldSTHRootHash, net::ct::kSthRootHashLength);

  sth->log_id = net::ct::GetTestPublicKeyId();

  const uint8_t kOldSTHSignatureData[] = {
      0x04, 0x03, 0x00, 0x47, 0x30, 0x45, 0x02, 0x20, 0x15, 0x7b, 0x23,
      0x42, 0xa2, 0x5f, 0x88, 0xc9, 0x0b, 0x30, 0xa6, 0xb4, 0x49, 0x50,
      0xb3, 0xab, 0xf5, 0x25, 0xfe, 0x27, 0xf0, 0x3f, 0x9a, 0xbf, 0xc1,
      0x16, 0x5a, 0x7a, 0xc0, 0x62, 0x2b, 0xbb, 0x02, 0x21, 0x00, 0xe6,
      0x57, 0xa3, 0xfe, 0xfc, 0x5a, 0x82, 0x9b, 0x29, 0x46, 0x15, 0x1d,
      0xbc, 0xfd, 0x9e, 0x87, 0x7f, 0xd0, 0x00, 0x5d, 0x62, 0x4f, 0x9a,
      0x1a, 0x9f, 0x20, 0x79, 0xd0, 0xc1, 0x34, 0x2e, 0x08};
  base::StringPiece sp(reinterpret_cast<const char*>(kOldSTHSignatureData),
                       sizeof(kOldSTHSignatureData));
  return DecodeDigitallySigned(&sp, &(sth->signature)) && sp.empty();
}

}  // namespace

class SingleTreeTrackerTest : public ::testing::Test {
  void SetUp() override {
    log_ = net::CTLogVerifier::Create(net::ct::GetTestPublicKey(), "testlog",
                                      "https://ct.example.com");

    ASSERT_TRUE(log_);
    ASSERT_EQ(log_->key_id(), net::ct::GetTestPublicKeyId());

    tree_tracker_.reset(new SingleTreeTracker(log_));
    const std::string der_test_cert(net::ct::GetDerEncodedX509Cert());
    chain_ = net::X509Certificate::CreateFromBytes(der_test_cert.data(),
                                                   der_test_cert.length());
    ASSERT_TRUE(chain_.get());
    net::ct::GetX509CertSCT(&cert_sct_);
  }

 protected:
  scoped_refptr<const net::CTLogVerifier> log_;
  std::unique_ptr<SingleTreeTracker> tree_tracker_;
  scoped_refptr<net::X509Certificate> chain_;
  scoped_refptr<net::ct::SignedCertificateTimestamp> cert_sct_;
};

// Test that an SCT is classified as pending for a newer STH if the
// SingleTreeTracker has not seen any STHs so far.
TEST_F(SingleTreeTrackerTest, CorrectlyClassifiesUnobservedSCTNoSTH) {
  // First make sure the SCT has not been observed at all.
  EXPECT_EQ(
      SingleTreeTracker::SCT_NOT_OBSERVED,
      tree_tracker_->GetLogEntryInclusionStatus(chain_.get(), cert_sct_.get()));

  tree_tracker_->OnSCTVerified(chain_.get(), cert_sct_.get());

  // Since no STH was provided to the tree_tracker_ the status should be that
  // the SCT is pending a newer STH.
  EXPECT_EQ(
      SingleTreeTracker::SCT_PENDING_NEWER_STH,
      tree_tracker_->GetLogEntryInclusionStatus(chain_.get(), cert_sct_.get()));
}

// Test that an SCT is classified as pending an inclusion check if the
// SingleTreeTracker has a fresh-enough STH to check inclusion against.
TEST_F(SingleTreeTrackerTest, CorrectlyClassifiesUnobservedSCTWithRecentSTH) {
  // Provide an STH to the tree_tracker_.
  net::ct::SignedTreeHead sth;
  net::ct::GetSampleSignedTreeHead(&sth);
  tree_tracker_->NewSTHObserved(sth);

  // Make sure the SCT status is the same as if there's no STH for
  // this log.
  EXPECT_EQ(
      SingleTreeTracker::SCT_NOT_OBSERVED,
      tree_tracker_->GetLogEntryInclusionStatus(chain_.get(), cert_sct_.get()));

  tree_tracker_->OnSCTVerified(chain_.get(), cert_sct_.get());

  // The status for this SCT should be 'pending inclusion check' since the STH
  // provided at the beginning of the test is newer than the SCT.
  EXPECT_EQ(
      SingleTreeTracker::SCT_PENDING_INCLUSION_CHECK,
      tree_tracker_->GetLogEntryInclusionStatus(chain_.get(), cert_sct_.get()));
}

// Test that the SingleTreeTracker correctly queues verified SCTs for inclusion
// checking such that, upon receiving a fresh STH, it changes the SCT's status
// from pending newer STH to pending inclusion check.
TEST_F(SingleTreeTrackerTest, CorrectlyUpdatesSCTStatusOnNewSTH) {
  // Report an observed SCT and make sure it's in the pending newer STH
  // state.
  tree_tracker_->OnSCTVerified(chain_.get(), cert_sct_.get());
  EXPECT_EQ(
      SingleTreeTracker::SCT_PENDING_NEWER_STH,
      tree_tracker_->GetLogEntryInclusionStatus(chain_.get(), cert_sct_.get()));

  // Provide with a fresh STH
  net::ct::SignedTreeHead sth;
  net::ct::GetSampleSignedTreeHead(&sth);
  tree_tracker_->NewSTHObserved(sth);

  // Test that its status has changed.
  EXPECT_EQ(
      SingleTreeTracker::SCT_PENDING_INCLUSION_CHECK,
      tree_tracker_->GetLogEntryInclusionStatus(chain_.get(), cert_sct_.get()));
}

// Test that the SingleTreeTracker does not change an SCT's status if an STH
// from the log it was issued by is observed, but that STH is too old to check
// inclusion against.
TEST_F(SingleTreeTrackerTest, DoesNotUpdatesSCTStatusOnOldSTH) {
  // Notify of an SCT and make sure it's in the 'pending newer STH' state.
  tree_tracker_->OnSCTVerified(chain_.get(), cert_sct_.get());
  EXPECT_EQ(
      SingleTreeTracker::SCT_PENDING_NEWER_STH,
      tree_tracker_->GetLogEntryInclusionStatus(chain_.get(), cert_sct_.get()));

  // Provide an old STH for the same log.
  net::ct::SignedTreeHead sth;
  GetOldSignedTreeHead(&sth);
  tree_tracker_->NewSTHObserved(sth);

  // Make sure the SCT's state hasn't changed.
  EXPECT_EQ(
      SingleTreeTracker::SCT_PENDING_NEWER_STH,
      tree_tracker_->GetLogEntryInclusionStatus(chain_.get(), cert_sct_.get()));
}

}  // namespace certificate_transparency
