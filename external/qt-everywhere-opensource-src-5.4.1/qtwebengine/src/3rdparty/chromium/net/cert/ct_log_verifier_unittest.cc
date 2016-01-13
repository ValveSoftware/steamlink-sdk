// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/ct_log_verifier.h"

#include <string>

#include "base/time/time.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/cert/signed_tree_head.h"
#include "net/test/ct_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

class CTLogVerifierTest : public ::testing::Test {
 public:
  CTLogVerifierTest() {}

  virtual void SetUp() OVERRIDE {
    log_ = CTLogVerifier::Create(ct::GetTestPublicKey(), "testlog").Pass();

    ASSERT_TRUE(log_);
    ASSERT_EQ(log_->key_id(), ct::GetTestPublicKeyId());
  }

 protected:
  scoped_ptr<CTLogVerifier> log_;
};

TEST_F(CTLogVerifierTest, VerifiesCertSCT) {
  ct::LogEntry cert_entry;
  ct::GetX509CertLogEntry(&cert_entry);

  scoped_refptr<ct::SignedCertificateTimestamp> cert_sct;
  ct::GetX509CertSCT(&cert_sct);

  EXPECT_TRUE(log_->Verify(cert_entry, *cert_sct));
}

TEST_F(CTLogVerifierTest, VerifiesPrecertSCT) {
  ct::LogEntry precert_entry;
  ct::GetPrecertLogEntry(&precert_entry);

  scoped_refptr<ct::SignedCertificateTimestamp> precert_sct;
  ct::GetPrecertSCT(&precert_sct);

  EXPECT_TRUE(log_->Verify(precert_entry, *precert_sct));
}

TEST_F(CTLogVerifierTest, FailsInvalidTimestamp) {
  ct::LogEntry cert_entry;
  ct::GetX509CertLogEntry(&cert_entry);

  scoped_refptr<ct::SignedCertificateTimestamp> cert_sct;
  ct::GetX509CertSCT(&cert_sct);

  // Mangle the timestamp, so that it should fail signature validation.
  cert_sct->timestamp = base::Time::Now();

  EXPECT_FALSE(log_->Verify(cert_entry, *cert_sct));
}

TEST_F(CTLogVerifierTest, FailsInvalidLogID) {
  ct::LogEntry cert_entry;
  ct::GetX509CertLogEntry(&cert_entry);

  scoped_refptr<ct::SignedCertificateTimestamp> cert_sct;
  ct::GetX509CertSCT(&cert_sct);

  // Mangle the log ID, which should cause it to match a different log before
  // attempting signature validation.
  cert_sct->log_id.assign(cert_sct->log_id.size(), '\0');

  EXPECT_FALSE(log_->Verify(cert_entry, *cert_sct));
}

TEST_F(CTLogVerifierTest, SetsValidSTH) {
  scoped_ptr<ct::SignedTreeHead> sth(new ct::SignedTreeHead());
  ct::GetSignedTreeHead(sth.get());
  ASSERT_TRUE(log_->SetSignedTreeHead(sth.Pass()));
}

TEST_F(CTLogVerifierTest, DoesNotSetInvalidSTH) {
  scoped_ptr<ct::SignedTreeHead> sth(new ct::SignedTreeHead());
  ct::GetSignedTreeHead(sth.get());
  sth->sha256_root_hash[0] = '\x0';
  ASSERT_FALSE(log_->SetSignedTreeHead(sth.Pass()));
}

}  // namespace net
