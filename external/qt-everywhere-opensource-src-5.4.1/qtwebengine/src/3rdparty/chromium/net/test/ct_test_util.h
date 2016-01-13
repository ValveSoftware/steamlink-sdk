// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_CT_TEST_UTIL_H_
#define NET_CERT_CT_TEST_UTIL_H_

#include <string>

#include "base/memory/ref_counted.h"

namespace net {

namespace ct {

struct LogEntry;
struct SignedCertificateTimestamp;
struct SignedTreeHead;

// Note: unless specified otherwise, all test data is taken from Certificate
// Transparency test data repository.

// Fills |entry| with test data for an X.509 entry.
void GetX509CertLogEntry(LogEntry* entry);

// Returns a DER-encoded X509 cert. The SCT provided by
// GetX509CertSCT is signed over this certificate.
std::string GetDerEncodedX509Cert();

// Fills |entry| with test data for a Precertificate entry.
void GetPrecertLogEntry(LogEntry* entry);

// Returns the binary representation of a test DigitallySigned
std::string GetTestDigitallySigned();

// Returns the binary representation of a test serialized SCT.
std::string GetTestSignedCertificateTimestamp();

// Test log key
std::string GetTestPublicKey();

// ID of test log key
std::string GetTestPublicKeyId();

// SCT for the X509Certificate provided above.
void GetX509CertSCT(scoped_refptr<SignedCertificateTimestamp>* sct);

// SCT for the Precertificate log entry provided above.
void GetPrecertSCT(scoped_refptr<SignedCertificateTimestamp>* sct);

// Issuer key hash
std::string GetDefaultIssuerKeyHash();

// Fake OCSP response with an embedded SCT list.
std::string GetDerEncodedFakeOCSPResponse();

// The SCT list embedded in the response above.
std::string GetFakeOCSPExtensionValue();

// The cert the OCSP response is for.
std::string GetDerEncodedFakeOCSPResponseCert();

// The issuer of the previous cert.
std::string GetDerEncodedFakeOCSPResponseIssuerCert();

// A sample, valid STH
void GetSignedTreeHead(SignedTreeHead* sth);

// The SHA256 root hash for the sample STH
std::string GetSampleSTHSHA256RootHash();

}  // namespace ct

}  // namespace net

#endif  // NET_CERT_CT_TEST_UTIL_H_
