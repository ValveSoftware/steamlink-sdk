// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/signed_certificate_timestamp.h"

#include <string>

#include "base/pickle.h"
#include "net/test/ct_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace ct {

namespace {

const char kLogDescription[] = "somelog";

class SignedCertificateTimestampTest : public ::testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    GetX509CertSCT(&sample_sct_);
    sample_sct_->origin = SignedCertificateTimestamp::SCT_FROM_OCSP_RESPONSE;
    sample_sct_->log_description = kLogDescription;
  }

 protected:
  scoped_refptr<SignedCertificateTimestamp> sample_sct_;
};

TEST_F(SignedCertificateTimestampTest, PicklesAndUnpickles) {
  Pickle pickle;

  sample_sct_->Persist(&pickle);
  PickleIterator iter(pickle);

  scoped_refptr<SignedCertificateTimestamp> unpickled_sct(
      SignedCertificateTimestamp::CreateFromPickle(&iter));

  SignedCertificateTimestamp::LessThan less_than;

  ASSERT_FALSE(less_than(sample_sct_, unpickled_sct));
  ASSERT_FALSE(less_than(unpickled_sct, sample_sct_));
  ASSERT_EQ(sample_sct_->origin, unpickled_sct->origin);
  ASSERT_EQ(sample_sct_->log_description, unpickled_sct->log_description);
}

}  // namespace

}  // namespace ct

}  // namespace net
