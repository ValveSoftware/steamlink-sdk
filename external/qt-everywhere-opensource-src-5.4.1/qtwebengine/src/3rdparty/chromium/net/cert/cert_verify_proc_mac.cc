// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_verify_proc_mac.h"

#include <CommonCrypto/CommonDigest.h>
#include <CoreServices/CoreServices.h>
#include <Security/Security.h>

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/mac/mac_logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/sha1.h"
#include "base/strings/string_piece.h"
#include "base/synchronization/lock.h"
#include "crypto/mac_security_services_lock.h"
#include "crypto/sha2.h"
#include "net/base/net_errors.h"
#include "net/cert/asn1_util.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/crl_set.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_certificate_known_roots_mac.h"
#include "net/cert/x509_util_mac.h"

// From 10.7.2 libsecurity_keychain-55035/lib/SecTrustPriv.h, for use with
// SecTrustCopyExtendedResult.
#ifndef kSecEVOrganizationName
#define kSecEVOrganizationName CFSTR("Organization")
#endif

using base::ScopedCFTypeRef;

namespace net {

namespace {

typedef OSStatus (*SecTrustCopyExtendedResultFuncPtr)(SecTrustRef,
                                                      CFDictionaryRef*);

int NetErrorFromOSStatus(OSStatus status) {
  switch (status) {
    case noErr:
      return OK;
    case errSecNotAvailable:
    case errSecNoCertificateModule:
    case errSecNoPolicyModule:
      return ERR_NOT_IMPLEMENTED;
    case errSecAuthFailed:
      return ERR_ACCESS_DENIED;
    default: {
      OSSTATUS_LOG(ERROR, status) << "Unknown error mapped to ERR_FAILED";
      return ERR_FAILED;
    }
  }
}

CertStatus CertStatusFromOSStatus(OSStatus status) {
  switch (status) {
    case noErr:
      return 0;

    case CSSMERR_TP_INVALID_ANCHOR_CERT:
    case CSSMERR_TP_NOT_TRUSTED:
    case CSSMERR_TP_INVALID_CERT_AUTHORITY:
      return CERT_STATUS_AUTHORITY_INVALID;

    case CSSMERR_TP_CERT_EXPIRED:
    case CSSMERR_TP_CERT_NOT_VALID_YET:
      // "Expired" and "not yet valid" collapse into a single status.
      return CERT_STATUS_DATE_INVALID;

    case CSSMERR_TP_CERT_REVOKED:
    case CSSMERR_TP_CERT_SUSPENDED:
      return CERT_STATUS_REVOKED;

    case CSSMERR_APPLETP_HOSTNAME_MISMATCH:
      return CERT_STATUS_COMMON_NAME_INVALID;

    case CSSMERR_APPLETP_CRL_NOT_FOUND:
    case CSSMERR_APPLETP_OCSP_UNAVAILABLE:
    case CSSMERR_APPLETP_INCOMPLETE_REVOCATION_CHECK:
      return CERT_STATUS_NO_REVOCATION_MECHANISM;

    case CSSMERR_APPLETP_CRL_EXPIRED:
    case CSSMERR_APPLETP_CRL_NOT_VALID_YET:
    case CSSMERR_APPLETP_CRL_SERVER_DOWN:
    case CSSMERR_APPLETP_CRL_NOT_TRUSTED:
    case CSSMERR_APPLETP_CRL_INVALID_ANCHOR_CERT:
    case CSSMERR_APPLETP_CRL_POLICY_FAIL:
    case CSSMERR_APPLETP_OCSP_BAD_RESPONSE:
    case CSSMERR_APPLETP_OCSP_BAD_REQUEST:
    case CSSMERR_APPLETP_OCSP_STATUS_UNRECOGNIZED:
    case CSSMERR_APPLETP_NETWORK_FAILURE:
    case CSSMERR_APPLETP_OCSP_NOT_TRUSTED:
    case CSSMERR_APPLETP_OCSP_INVALID_ANCHOR_CERT:
    case CSSMERR_APPLETP_OCSP_SIG_ERROR:
    case CSSMERR_APPLETP_OCSP_NO_SIGNER:
    case CSSMERR_APPLETP_OCSP_RESP_MALFORMED_REQ:
    case CSSMERR_APPLETP_OCSP_RESP_INTERNAL_ERR:
    case CSSMERR_APPLETP_OCSP_RESP_TRY_LATER:
    case CSSMERR_APPLETP_OCSP_RESP_SIG_REQUIRED:
    case CSSMERR_APPLETP_OCSP_RESP_UNAUTHORIZED:
    case CSSMERR_APPLETP_OCSP_NONCE_MISMATCH:
      // We asked for a revocation check, but didn't get it.
      return CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;

    case CSSMERR_APPLETP_SSL_BAD_EXT_KEY_USE:
      // TODO(wtc): Should we add CERT_STATUS_WRONG_USAGE?
      return CERT_STATUS_INVALID;

    case CSSMERR_APPLETP_CRL_BAD_URI:
    case CSSMERR_APPLETP_IDP_FAIL:
      return CERT_STATUS_INVALID;

    case CSSMERR_CSP_UNSUPPORTED_KEY_SIZE:
      // Mapping UNSUPPORTED_KEY_SIZE to CERT_STATUS_WEAK_KEY is not strictly
      // accurate, as the error may have been returned due to a key size
      // that exceeded the maximum supported. However, within
      // CertVerifyProcMac::VerifyInternal(), this code should only be
      // encountered as a certificate status code, and only when the key size
      // is smaller than the minimum required (1024 bits).
      return CERT_STATUS_WEAK_KEY;

    default: {
      // Failure was due to something Chromium doesn't define a
      // specific status for (such as basic constraints violation, or
      // unknown critical extension)
      OSSTATUS_LOG(WARNING, status)
          << "Unknown error mapped to CERT_STATUS_INVALID";
      return CERT_STATUS_INVALID;
    }
  }
}

// Creates a series of SecPolicyRefs to be added to a SecTrustRef used to
// validate a certificate for an SSL server. |hostname| contains the name of
// the SSL server that the certificate should be verified against. |flags| is
// a bitwise-OR of VerifyFlags that can further alter how trust is validated,
// such as how revocation is checked. If successful, returns noErr, and
// stores the resultant array of SecPolicyRefs in |policies|.
OSStatus CreateTrustPolicies(const std::string& hostname,
                             int flags,
                             ScopedCFTypeRef<CFArrayRef>* policies) {
  ScopedCFTypeRef<CFMutableArrayRef> local_policies(
      CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks));
  if (!local_policies)
    return memFullErr;

  SecPolicyRef ssl_policy;
  OSStatus status = x509_util::CreateSSLServerPolicy(hostname, &ssl_policy);
  if (status)
    return status;
  CFArrayAppendValue(local_policies, ssl_policy);
  CFRelease(ssl_policy);

  // Explicitly add revocation policies, in order to override system
  // revocation checking policies and instead respect the application-level
  // revocation preference.
  status = x509_util::CreateRevocationPolicies(
      (flags & CertVerifier::VERIFY_REV_CHECKING_ENABLED),
      (flags & CertVerifier::VERIFY_REV_CHECKING_ENABLED_EV_ONLY),
      local_policies);
  if (status)
    return status;

  policies->reset(local_policies.release());
  return noErr;
}

// Saves some information about the certificate chain |cert_chain| in
// |*verify_result|. The caller MUST initialize |*verify_result| before
// calling this function.
void GetCertChainInfo(CFArrayRef cert_chain,
                      CSSM_TP_APPLE_EVIDENCE_INFO* chain_info,
                      CertVerifyResult* verify_result) {
  SecCertificateRef verified_cert = NULL;
  std::vector<SecCertificateRef> verified_chain;
  for (CFIndex i = 0, count = CFArrayGetCount(cert_chain); i < count; ++i) {
    SecCertificateRef chain_cert = reinterpret_cast<SecCertificateRef>(
        const_cast<void*>(CFArrayGetValueAtIndex(cert_chain, i)));
    if (i == 0) {
      verified_cert = chain_cert;
    } else {
      verified_chain.push_back(chain_cert);
    }

    if ((chain_info[i].StatusBits & CSSM_CERT_STATUS_IS_IN_ANCHORS) ||
        (chain_info[i].StatusBits & CSSM_CERT_STATUS_IS_ROOT)) {
      // The current certificate is either in the user's trusted store or is
      // a root (self-signed) certificate. Ignore the signature algorithm for
      // these certificates, as it is meaningless for security. We allow
      // self-signed certificates (i == 0 & IS_ROOT), since we accept that
      // any security assertions by such a cert are inherently meaningless.
      continue;
    }

    x509_util::CSSMCachedCertificate cached_cert;
    OSStatus status = cached_cert.Init(chain_cert);
    if (status)
      continue;
    x509_util::CSSMFieldValue signature_field;
    status = cached_cert.GetField(&CSSMOID_X509V1SignatureAlgorithm,
                                  &signature_field);
    if (status || !signature_field.field())
      continue;
    // Match the behaviour of OS X system tools and defensively check that
    // sizes are appropriate. This would indicate a critical failure of the
    // OS X certificate library, but based on history, it is best to play it
    // safe.
    const CSSM_X509_ALGORITHM_IDENTIFIER* sig_algorithm =
        signature_field.GetAs<CSSM_X509_ALGORITHM_IDENTIFIER>();
    if (!sig_algorithm)
      continue;

    const CSSM_OID* alg_oid = &sig_algorithm->algorithm;
    if (CSSMOIDEqual(alg_oid, &CSSMOID_MD2WithRSA)) {
      verify_result->has_md2 = true;
    } else if (CSSMOIDEqual(alg_oid, &CSSMOID_MD4WithRSA)) {
      verify_result->has_md4 = true;
    } else if (CSSMOIDEqual(alg_oid, &CSSMOID_MD5WithRSA)) {
      verify_result->has_md5 = true;
    }
  }
  if (!verified_cert)
    return;

  verify_result->verified_cert =
      X509Certificate::CreateFromHandle(verified_cert, verified_chain);
}

void AppendPublicKeyHashes(CFArrayRef chain,
                           HashValueVector* hashes) {
  const CFIndex n = CFArrayGetCount(chain);
  for (CFIndex i = 0; i < n; i++) {
    SecCertificateRef cert = reinterpret_cast<SecCertificateRef>(
        const_cast<void*>(CFArrayGetValueAtIndex(chain, i)));

    CSSM_DATA cert_data;
    OSStatus err = SecCertificateGetData(cert, &cert_data);
    DCHECK_EQ(err, noErr);
    base::StringPiece der_bytes(reinterpret_cast<const char*>(cert_data.Data),
                               cert_data.Length);
    base::StringPiece spki_bytes;
    if (!asn1::ExtractSPKIFromDERCert(der_bytes, &spki_bytes))
      continue;

    HashValue sha1(HASH_VALUE_SHA1);
    CC_SHA1(spki_bytes.data(), spki_bytes.size(), sha1.data());
    hashes->push_back(sha1);

    HashValue sha256(HASH_VALUE_SHA256);
    CC_SHA256(spki_bytes.data(), spki_bytes.size(), sha256.data());
    hashes->push_back(sha256);
  }
}

bool CheckRevocationWithCRLSet(CFArrayRef chain, CRLSet* crl_set) {
  if (CFArrayGetCount(chain) == 0)
    return true;

  // We iterate from the root certificate down to the leaf, keeping track of
  // the issuer's SPKI at each step.
  std::string issuer_spki_hash;
  for (CFIndex i = CFArrayGetCount(chain) - 1; i >= 0; i--) {
    SecCertificateRef cert = reinterpret_cast<SecCertificateRef>(
        const_cast<void*>(CFArrayGetValueAtIndex(chain, i)));

    CSSM_DATA cert_data;
    OSStatus err = SecCertificateGetData(cert, &cert_data);
    if (err != noErr) {
      NOTREACHED();
      continue;
    }
    base::StringPiece der_bytes(reinterpret_cast<const char*>(cert_data.Data),
                                cert_data.Length);
    base::StringPiece spki;
    if (!asn1::ExtractSPKIFromDERCert(der_bytes, &spki)) {
      NOTREACHED();
      continue;
    }

    const std::string spki_hash = crypto::SHA256HashString(spki);
    x509_util::CSSMCachedCertificate cached_cert;
    if (cached_cert.Init(cert) != CSSM_OK) {
      NOTREACHED();
      continue;
    }
    x509_util::CSSMFieldValue serial_number;
    err = cached_cert.GetField(&CSSMOID_X509V1SerialNumber, &serial_number);
    if (err || !serial_number.field()) {
      NOTREACHED();
      continue;
    }

    base::StringPiece serial(
        reinterpret_cast<const char*>(serial_number.field()->Data),
        serial_number.field()->Length);

    CRLSet::Result result = crl_set->CheckSPKI(spki_hash);

    if (result != CRLSet::REVOKED && !issuer_spki_hash.empty())
      result = crl_set->CheckSerial(serial, issuer_spki_hash);

    issuer_spki_hash = spki_hash;

    switch (result) {
      case CRLSet::REVOKED:
        return false;
      case CRLSet::UNKNOWN:
      case CRLSet::GOOD:
        continue;
      default:
        NOTREACHED();
        return false;
    }
  }

  return true;
}

// IsIssuedByKnownRoot returns true if the given chain is rooted at a root CA
// that we recognise as a standard root.
// static
bool IsIssuedByKnownRoot(CFArrayRef chain) {
  int n = CFArrayGetCount(chain);
  if (n < 1)
    return false;
  SecCertificateRef root_ref = reinterpret_cast<SecCertificateRef>(
      const_cast<void*>(CFArrayGetValueAtIndex(chain, n - 1)));
  SHA1HashValue hash = X509Certificate::CalculateFingerprint(root_ref);
  return IsSHA1HashInSortedArray(
      hash, &kKnownRootCertSHA1Hashes[0][0], sizeof(kKnownRootCertSHA1Hashes));
}

// Builds and evaluates a SecTrustRef for the certificate chain contained
// in |cert_array|, using the verification policies in |trust_policies|. On
// success, returns OK, and updates |trust_ref|, |trust_result|,
// |verified_chain|, and |chain_info| with the verification results. On
// failure, no output parameters are modified.
//
// Note: An OK return does not mean that |cert_array| is trusted, merely that
// verification was performed successfully.
//
// This function should only be called while the Mac Security Services lock is
// held.
int BuildAndEvaluateSecTrustRef(CFArrayRef cert_array,
                                CFArrayRef trust_policies,
                                int flags,
                                ScopedCFTypeRef<SecTrustRef>* trust_ref,
                                SecTrustResultType* trust_result,
                                ScopedCFTypeRef<CFArrayRef>* verified_chain,
                                CSSM_TP_APPLE_EVIDENCE_INFO** chain_info) {
  SecTrustRef tmp_trust = NULL;
  OSStatus status = SecTrustCreateWithCertificates(cert_array, trust_policies,
                                                   &tmp_trust);
  if (status)
    return NetErrorFromOSStatus(status);
  ScopedCFTypeRef<SecTrustRef> scoped_tmp_trust(tmp_trust);

  if (TestRootCerts::HasInstance()) {
    status = TestRootCerts::GetInstance()->FixupSecTrustRef(tmp_trust);
    if (status)
      return NetErrorFromOSStatus(status);
  }

  CSSM_APPLE_TP_ACTION_DATA tp_action_data;
  memset(&tp_action_data, 0, sizeof(tp_action_data));
  tp_action_data.Version = CSSM_APPLE_TP_ACTION_VERSION;
  // Allow CSSM to download any missing intermediate certificates if an
  // authorityInfoAccess extension or issuerAltName extension is present.
  tp_action_data.ActionFlags = CSSM_TP_ACTION_FETCH_CERT_FROM_NET |
                               CSSM_TP_ACTION_TRUST_SETTINGS;

  // Note: For EV certificates, the Apple TP will handle setting these flags
  // as part of EV evaluation.
  if (flags & CertVerifier::VERIFY_REV_CHECKING_ENABLED) {
    // Require a positive result from an OCSP responder or a CRL (or both)
    // for every certificate in the chain. The Apple TP automatically
    // excludes the self-signed root from this requirement. If a certificate
    // is missing both a crlDistributionPoints extension and an
    // authorityInfoAccess extension with an OCSP responder URL, then we
    // will get a kSecTrustResultRecoverableTrustFailure back from
    // SecTrustEvaluate(), with a
    // CSSMERR_APPLETP_INCOMPLETE_REVOCATION_CHECK error code. In that case,
    // we'll set our own result to include
    // CERT_STATUS_NO_REVOCATION_MECHANISM. If one or both extensions are
    // present, and a check fails (server unavailable, OCSP retry later,
    // signature mismatch), then we'll set our own result to include
    // CERT_STATUS_UNABLE_TO_CHECK_REVOCATION.
    tp_action_data.ActionFlags |= CSSM_TP_ACTION_REQUIRE_REV_PER_CERT;

    // Note, even if revocation checking is disabled, SecTrustEvaluate() will
    // modify the OCSP options so as to attempt OCSP checking if it believes a
    // certificate may chain to an EV root. However, because network fetches
    // are disabled in CreateTrustPolicies() when revocation checking is
    // disabled, these will only go against the local cache.
  }

  CFDataRef action_data_ref =
      CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,
                                  reinterpret_cast<UInt8*>(&tp_action_data),
                                  sizeof(tp_action_data), kCFAllocatorNull);
  if (!action_data_ref)
    return ERR_OUT_OF_MEMORY;
  ScopedCFTypeRef<CFDataRef> scoped_action_data_ref(action_data_ref);
  status = SecTrustSetParameters(tmp_trust, CSSM_TP_ACTION_DEFAULT,
                                 action_data_ref);
  if (status)
    return NetErrorFromOSStatus(status);

  // Verify the certificate. A non-zero result from SecTrustGetResult()
  // indicates that some fatal error occurred and the chain couldn't be
  // processed, not that the chain contains no errors. We need to examine the
  // output of SecTrustGetResult() to determine that.
  SecTrustResultType tmp_trust_result;
  status = SecTrustEvaluate(tmp_trust, &tmp_trust_result);
  if (status)
    return NetErrorFromOSStatus(status);
  CFArrayRef tmp_verified_chain = NULL;
  CSSM_TP_APPLE_EVIDENCE_INFO* tmp_chain_info;
  status = SecTrustGetResult(tmp_trust, &tmp_trust_result, &tmp_verified_chain,
                             &tmp_chain_info);
  if (status)
    return NetErrorFromOSStatus(status);

  trust_ref->swap(scoped_tmp_trust);
  *trust_result = tmp_trust_result;
  verified_chain->reset(tmp_verified_chain);
  *chain_info = tmp_chain_info;

  return OK;
}

// OS X ships with both "GTE CyberTrust Global Root" and "Baltimore CyberTrust
// Root" as part of its trusted root store. However, a cross-certified version
// of the "Baltimore CyberTrust Root" exists that chains to "GTE CyberTrust
// Global Root". When OS X/Security.framework attempts to evaluate such a
// certificate chain, it disregards the "Baltimore CyberTrust Root" that exists
// within Keychain and instead attempts to terminate the chain in the "GTE
// CyberTrust Global Root". However, the GTE root is scheduled to be removed in
// a future OS X update (for sunsetting purposes), and once removed, such
// chains will fail validation, even though a trust anchor still exists.
//
// Rather than over-generalizing a solution that may mask a number of TLS
// misconfigurations, attempt to specifically match the affected
// cross-certified certificate and remove it from certificate chain processing.
bool IsBadBaltimoreGTECertificate(SecCertificateRef cert) {
  // Matches the GTE-signed Baltimore CyberTrust Root
  // https://cacert.omniroot.com/Baltimore-to-GTE-04-12.pem
  static const SHA1HashValue kBadBaltimoreHashNew =
    { { 0x4D, 0x34, 0xEA, 0x92, 0x76, 0x4B, 0x3A, 0x31, 0x49, 0x11,
        0x99, 0x52, 0xF4, 0x19, 0x30, 0xCA, 0x11, 0x34, 0x83, 0x61 } };
  // Matches the legacy GTE-signed Baltimore CyberTrust Root
  // https://cacert.omniroot.com/gte-2-2025.pem
  static const SHA1HashValue kBadBaltimoreHashOld =
    { { 0x54, 0xD8, 0xCB, 0x49, 0x1F, 0xA1, 0x6D, 0xF8, 0x87, 0xDC,
        0x94, 0xA9, 0x34, 0xCC, 0x83, 0x6B, 0xDA, 0xA8, 0xA3, 0x69 } };

  SHA1HashValue fingerprint = X509Certificate::CalculateFingerprint(cert);

  return fingerprint.Equals(kBadBaltimoreHashNew) ||
         fingerprint.Equals(kBadBaltimoreHashOld);
}

// Attempts to re-verify |cert_array| after adjusting the inputs to work around
// known issues in OS X. To be used if BuildAndEvaluateSecTrustRef fails to
// return a positive result for verification.
//
// This function should only be called while the Mac Security Services lock is
// held.
void RetrySecTrustEvaluateWithAdjustedChain(
    CFArrayRef cert_array,
    CFArrayRef trust_policies,
    int flags,
    ScopedCFTypeRef<SecTrustRef>* trust_ref,
    SecTrustResultType* trust_result,
    ScopedCFTypeRef<CFArrayRef>* verified_chain,
    CSSM_TP_APPLE_EVIDENCE_INFO** chain_info) {
  CFIndex count = CFArrayGetCount(*verified_chain);
  CFIndex slice_point = 0;

  for (CFIndex i = 1; i < count; ++i) {
    SecCertificateRef cert = reinterpret_cast<SecCertificateRef>(
        const_cast<void*>(CFArrayGetValueAtIndex(*verified_chain, i)));
    if (cert == NULL)
      return;  // Strange times; can't fix things up.

    if (IsBadBaltimoreGTECertificate(cert)) {
      slice_point = i;
      break;
    }
  }
  if (slice_point == 0)
    return;  // Nothing to do.

  ScopedCFTypeRef<CFMutableArrayRef> adjusted_cert_array(
      CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks));
  // Note: This excludes the certificate at |slice_point|.
  CFArrayAppendArray(adjusted_cert_array, cert_array,
                     CFRangeMake(0, slice_point));

  // Ignore the result; failure will preserve the old verification results.
  BuildAndEvaluateSecTrustRef(
      adjusted_cert_array, trust_policies, flags, trust_ref, trust_result,
      verified_chain, chain_info);
}

}  // namespace

CertVerifyProcMac::CertVerifyProcMac() {}

CertVerifyProcMac::~CertVerifyProcMac() {}

bool CertVerifyProcMac::SupportsAdditionalTrustAnchors() const {
  return false;
}

int CertVerifyProcMac::VerifyInternal(
    X509Certificate* cert,
    const std::string& hostname,
    int flags,
    CRLSet* crl_set,
    const CertificateList& additional_trust_anchors,
    CertVerifyResult* verify_result) {
  ScopedCFTypeRef<CFArrayRef> trust_policies;
  OSStatus status = CreateTrustPolicies(hostname, flags, &trust_policies);
  if (status)
    return NetErrorFromOSStatus(status);

  // Create and configure a SecTrustRef, which takes our certificate(s)
  // and our SSL SecPolicyRef. SecTrustCreateWithCertificates() takes an
  // array of certificates, the first of which is the certificate we're
  // verifying, and the subsequent (optional) certificates are used for
  // chain building.
  ScopedCFTypeRef<CFArrayRef> cert_array(cert->CreateOSCertChainForCert());

  // Serialize all calls that may use the Keychain, to work around various
  // issues in OS X 10.6+ with multi-threaded access to Security.framework.
  base::AutoLock lock(crypto::GetMacSecurityServicesLock());

  ScopedCFTypeRef<SecTrustRef> trust_ref;
  SecTrustResultType trust_result = kSecTrustResultDeny;
  ScopedCFTypeRef<CFArrayRef> completed_chain;
  CSSM_TP_APPLE_EVIDENCE_INFO* chain_info = NULL;

  int rv = BuildAndEvaluateSecTrustRef(
      cert_array, trust_policies, flags, &trust_ref, &trust_result,
      &completed_chain, &chain_info);
  if (rv != OK)
    return rv;
  if (trust_result != kSecTrustResultUnspecified &&
      trust_result != kSecTrustResultProceed) {
    RetrySecTrustEvaluateWithAdjustedChain(
        cert_array, trust_policies, flags, &trust_ref, &trust_result,
        &completed_chain, &chain_info);
  }

  if (flags & CertVerifier::VERIFY_REV_CHECKING_ENABLED)
    verify_result->cert_status |= CERT_STATUS_REV_CHECKING_ENABLED;

  if (crl_set && !CheckRevocationWithCRLSet(completed_chain, crl_set))
    verify_result->cert_status |= CERT_STATUS_REVOKED;

  GetCertChainInfo(completed_chain, chain_info, verify_result);

  // As of Security Update 2012-002/OS X 10.7.4, when an RSA key < 1024 bits
  // is encountered, CSSM returns CSSMERR_TP_VERIFY_ACTION_FAILED and adds
  // CSSMERR_CSP_UNSUPPORTED_KEY_SIZE as a certificate status. Avoid mapping
  // the CSSMERR_TP_VERIFY_ACTION_FAILED to CERT_STATUS_INVALID if the only
  // error was due to an unsupported key size.
  bool policy_failed = false;
  bool weak_key_or_signature_algorithm = false;

  // Evaluate the results
  OSStatus cssm_result;
  switch (trust_result) {
    case kSecTrustResultUnspecified:
    case kSecTrustResultProceed:
      // Certificate chain is valid and trusted ("unspecified" indicates that
      // the user has not explicitly set a trust setting)
      break;

    // According to SecTrust.h, kSecTrustResultConfirm isn't returned on 10.5+,
    // and it is marked deprecated in the 10.9 SDK.
    case kSecTrustResultDeny:
      // Certificate chain is explicitly untrusted.
      verify_result->cert_status |= CERT_STATUS_AUTHORITY_INVALID;
      break;

    case kSecTrustResultRecoverableTrustFailure:
      // Certificate chain has a failure that can be overridden by the user.
      status = SecTrustGetCssmResultCode(trust_ref, &cssm_result);
      if (status)
        return NetErrorFromOSStatus(status);
      if (cssm_result == CSSMERR_TP_VERIFY_ACTION_FAILED) {
        policy_failed = true;
      } else {
        verify_result->cert_status |= CertStatusFromOSStatus(cssm_result);
      }
      // Walk the chain of error codes in the CSSM_TP_APPLE_EVIDENCE_INFO
      // structure which can catch multiple errors from each certificate.
      for (CFIndex index = 0, chain_count = CFArrayGetCount(completed_chain);
           index < chain_count; ++index) {
        if (chain_info[index].StatusBits & CSSM_CERT_STATUS_EXPIRED ||
            chain_info[index].StatusBits & CSSM_CERT_STATUS_NOT_VALID_YET)
          verify_result->cert_status |= CERT_STATUS_DATE_INVALID;
        if (!IsCertStatusError(verify_result->cert_status) &&
            chain_info[index].NumStatusCodes == 0) {
          LOG(WARNING) << "chain_info[" << index << "].NumStatusCodes is 0"
                          ", chain_info[" << index << "].StatusBits is "
                       << chain_info[index].StatusBits;
        }
        for (uint32 status_code_index = 0;
             status_code_index < chain_info[index].NumStatusCodes;
             ++status_code_index) {
          // As of OS X 10.9, attempting to verify a certificate chain that
          // contains a weak signature algorithm (MD2, MD5) in an intermediate
          // or leaf cert will be treated as a (recoverable) policy validation
          // failure, with the status code CSSMERR_TP_INVALID_CERTIFICATE
          // added to the Status Codes. Don't treat this code as an invalid
          // certificate; instead, map it to a weak key. Any truly invalid
          // certificates will have the major error (cssm_result) set to
          // CSSMERR_TP_INVALID_CERTIFICATE, rather than
          // CSSMERR_TP_VERIFY_ACTION_FAILED.
          CertStatus mapped_status = 0;
          if (policy_failed &&
              chain_info[index].StatusCodes[status_code_index] ==
                  CSSMERR_TP_INVALID_CERTIFICATE) {
              mapped_status = CERT_STATUS_WEAK_SIGNATURE_ALGORITHM;
              weak_key_or_signature_algorithm = true;
          } else {
              mapped_status = CertStatusFromOSStatus(
                  chain_info[index].StatusCodes[status_code_index]);
              if (mapped_status == CERT_STATUS_WEAK_KEY)
                weak_key_or_signature_algorithm = true;
          }
          verify_result->cert_status |= mapped_status;
        }
      }
      if (policy_failed && !weak_key_or_signature_algorithm) {
        // If CSSMERR_TP_VERIFY_ACTION_FAILED wasn't returned due to a weak
        // key, map it back to an appropriate error code.
        verify_result->cert_status |= CertStatusFromOSStatus(cssm_result);
      }
      if (!IsCertStatusError(verify_result->cert_status)) {
        LOG(ERROR) << "cssm_result=" << cssm_result;
        verify_result->cert_status |= CERT_STATUS_INVALID;
        NOTREACHED();
      }
      break;

    default:
      status = SecTrustGetCssmResultCode(trust_ref, &cssm_result);
      if (status)
        return NetErrorFromOSStatus(status);
      verify_result->cert_status |= CertStatusFromOSStatus(cssm_result);
      if (!IsCertStatusError(verify_result->cert_status)) {
        LOG(WARNING) << "trust_result=" << trust_result;
        verify_result->cert_status |= CERT_STATUS_INVALID;
      }
      break;
  }

  // Perform hostname verification independent of SecTrustEvaluate. In order to
  // do so, mask off any reported name errors first.
  verify_result->cert_status &= ~CERT_STATUS_COMMON_NAME_INVALID;
  if (!cert->VerifyNameMatch(hostname,
                             &verify_result->common_name_fallback_used)) {
    verify_result->cert_status |= CERT_STATUS_COMMON_NAME_INVALID;
  }

  // TODO(wtc): Suppress CERT_STATUS_NO_REVOCATION_MECHANISM for now to be
  // compatible with Windows, which in turn implements this behavior to be
  // compatible with WinHTTP, which doesn't report this error (bug 3004).
  verify_result->cert_status &= ~CERT_STATUS_NO_REVOCATION_MECHANISM;

  AppendPublicKeyHashes(completed_chain, &verify_result->public_key_hashes);
  verify_result->is_issued_by_known_root = IsIssuedByKnownRoot(completed_chain);

  if (IsCertStatusError(verify_result->cert_status))
    return MapCertStatusToNetError(verify_result->cert_status);

  if (flags & CertVerifier::VERIFY_EV_CERT) {
    // Determine the certificate's EV status using SecTrustCopyExtendedResult(),
    // which is an internal/private API function added in OS X 10.5.7.
    // Note: "ExtendedResult" means extended validation results.
    CFBundleRef bundle =
        CFBundleGetBundleWithIdentifier(CFSTR("com.apple.security"));
    if (bundle) {
      SecTrustCopyExtendedResultFuncPtr copy_extended_result =
          reinterpret_cast<SecTrustCopyExtendedResultFuncPtr>(
              CFBundleGetFunctionPointerForName(bundle,
                  CFSTR("SecTrustCopyExtendedResult")));
      if (copy_extended_result) {
        CFDictionaryRef ev_dict_temp = NULL;
        status = copy_extended_result(trust_ref, &ev_dict_temp);
        ScopedCFTypeRef<CFDictionaryRef> ev_dict(ev_dict_temp);
        ev_dict_temp = NULL;
        if (status == noErr && ev_dict) {
          // In 10.7.3, SecTrustCopyExtendedResult returns noErr and populates
          // ev_dict even for non-EV certificates, but only EV certificates
          // will cause ev_dict to contain kSecEVOrganizationName. In previous
          // releases, SecTrustCopyExtendedResult would only return noErr and
          // populate ev_dict for EV certificates, but would always include
          // kSecEVOrganizationName in that case, so checking for this key is
          // appropriate for all known versions of SecTrustCopyExtendedResult.
          // The actual organization name is unneeded here and can be accessed
          // through other means. All that matters here is the OS' conception
          // of whether or not the certificate is EV.
          if (CFDictionaryContainsKey(ev_dict,
                                      kSecEVOrganizationName)) {
            verify_result->cert_status |= CERT_STATUS_IS_EV;
            if (flags & CertVerifier::VERIFY_REV_CHECKING_ENABLED_EV_ONLY)
              verify_result->cert_status |= CERT_STATUS_REV_CHECKING_ENABLED;
          }
        }
      }
    }
  }

  return OK;
}

}  // namespace net
