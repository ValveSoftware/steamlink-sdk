// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_CERT_VERIFY_PROC_H_
#define NET_CERT_CERT_VERIFY_PROC_H_

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "net/base/net_export.h"
#include "net/cert/x509_cert_types.h"

namespace net {

class CertVerifyResult;
class CRLSet;
class X509Certificate;
typedef std::vector<scoped_refptr<X509Certificate> > CertificateList;

// Class to perform certificate path building and verification for various
// certificate uses. All methods of this class must be thread-safe, as they
// may be called from various non-joinable worker threads.
class NET_EXPORT CertVerifyProc
    : public base::RefCountedThreadSafe<CertVerifyProc> {
 public:
  // Creates and returns the default CertVerifyProc.
  static CertVerifyProc* CreateDefault();

  // Verifies the certificate against the given hostname as an SSL server
  // certificate. Returns OK if successful or an error code upon failure.
  //
  // The |*verify_result| structure, including the |verify_result->cert_status|
  // bitmask, is always filled out regardless of the return value. If the
  // certificate has multiple errors, the corresponding status flags are set in
  // |verify_result->cert_status|, and the error code for the most serious
  // error is returned.
  //
  // |flags| is bitwise OR'd of VerifyFlags:
  //
  // If VERIFY_REV_CHECKING_ENABLED is set in |flags|, online certificate
  // revocation checking is performed (i.e. OCSP and downloading CRLs). CRLSet
  // based revocation checking is always enabled, regardless of this flag, if
  // |crl_set| is given.
  //
  // If VERIFY_EV_CERT is set in |flags| too, EV certificate verification is
  // performed.
  //
  // |crl_set| points to an optional CRLSet structure which can be used to
  // avoid revocation checks over the network.
  //
  // |additional_trust_anchors| lists certificates that can be trusted when
  // building a certificate chain, in addition to the anchors known to the
  // implementation.
  int Verify(X509Certificate* cert,
             const std::string& hostname,
             int flags,
             CRLSet* crl_set,
             const CertificateList& additional_trust_anchors,
             CertVerifyResult* verify_result);

  // Returns true if the implementation supports passing additional trust
  // anchors to the Verify() call. The |additional_trust_anchors| parameter
  // passed to Verify() is ignored when this returns false.
  virtual bool SupportsAdditionalTrustAnchors() const = 0;

 protected:
  CertVerifyProc();
  virtual ~CertVerifyProc();

 private:
  friend class base::RefCountedThreadSafe<CertVerifyProc>;
  FRIEND_TEST_ALL_PREFIXES(CertVerifyProcTest, DigiNotarCerts);

  // Performs the actual verification using the desired underlying
  // cryptographic library.
  virtual int VerifyInternal(X509Certificate* cert,
                             const std::string& hostname,
                             int flags,
                             CRLSet* crl_set,
                             const CertificateList& additional_trust_anchors,
                             CertVerifyResult* verify_result) = 0;

  // Returns true if |cert| is explicitly blacklisted.
  static bool IsBlacklisted(X509Certificate* cert);

  // IsPublicKeyBlacklisted returns true iff one of |public_key_hashes| (which
  // are hashes of SubjectPublicKeyInfo structures) is explicitly blocked.
  static bool IsPublicKeyBlacklisted(const HashValueVector& public_key_hashes);

  // HasNameConstraintsViolation returns true iff one of |public_key_hashes|
  // (which are hashes of SubjectPublicKeyInfo structures) has name constraints
  // imposed on it and the names in |dns_names| are not permitted.
  static bool HasNameConstraintsViolation(
      const HashValueVector& public_key_hashes,
      const std::string& common_name,
      const std::vector<std::string>& dns_names,
      const std::vector<std::string>& ip_addrs);

  DISALLOW_COPY_AND_ASSIGN(CertVerifyProc);
};

}  // namespace net

#endif  // NET_CERT_CERT_VERIFY_PROC_H_
