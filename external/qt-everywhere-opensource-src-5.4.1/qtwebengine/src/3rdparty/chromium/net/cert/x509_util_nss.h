// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_X509_UTIL_NSS_H_
#define NET_CERT_X509_UTIL_NSS_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/cert/x509_certificate.h"

class PickleIterator;

typedef struct CERTCertificateStr CERTCertificate;
typedef struct CERTNameStr CERTName;
typedef struct PK11SlotInfoStr PK11SlotInfo;
typedef struct PLArenaPool PLArenaPool;
typedef struct SECItemStr SECItem;

namespace net {

namespace x509_util {

#if defined(USE_NSS) || defined(OS_IOS)
// Parses the Principal attribute from |name| and outputs the result in
// |principal|.
void ParsePrincipal(CERTName* name,
                    CertPrincipal* principal);

// Parses the date from |der_date| and outputs the result in |result|.
void ParseDate(const SECItem* der_date, base::Time* result);

// Parses the serial number from |certificate|.
std::string ParseSerialNumber(const CERTCertificate* certificate);

// Gets the subjectAltName extension field from the certificate, if any.
void GetSubjectAltName(CERTCertificate* cert_handle,
                       std::vector<std::string>* dns_names,
                       std::vector<std::string>* ip_addrs);

// Creates all possible OS certificate handles from |data| encoded in a specific
// |format|. Returns an empty collection on failure.
X509Certificate::OSCertHandles CreateOSCertHandlesFromBytes(
    const char* data,
    int length,
    X509Certificate::Format format);

// Reads a single certificate from |pickle_iter| and returns a platform-specific
// certificate handle. Returns an invalid handle, NULL, on failure.
X509Certificate::OSCertHandle ReadOSCertHandleFromPickle(
    PickleIterator* pickle_iter);

// Sets |*size_bits| to be the length of the public key in bits, and sets
// |*type| to one of the |PublicKeyType| values. In case of
// |kPublicKeyTypeUnknown|, |*size_bits| will be set to 0.
void GetPublicKeyInfo(CERTCertificate* handle,
                      size_t* size_bits,
                      X509Certificate::PublicKeyType* type);

// Create a list of CERTName objects from a list of DER-encoded X.509
// DistinguishedName items. All objects are created in a given arena.
// |encoded_issuers| is the list of encoded DNs.
// |arena| is the arena used for all allocations.
// |out| will receive the result list on success.
// Return true on success. On failure, the caller must free the
// intermediate CERTName objects pushed to |out|.
bool GetIssuersFromEncodedList(
    const std::vector<std::string>& issuers,
    PLArenaPool* arena,
    std::vector<CERTName*>* out);

// Returns true iff a certificate is issued by any of the issuers listed
// by name in |valid_issuers|.
// |cert_chain| is the certificate's chain.
// |valid_issuers| is a list of strings, where each string contains
// a DER-encoded X.509 Distinguished Name.
bool IsCertificateIssuedBy(const std::vector<CERTCertificate*>& cert_chain,
                           const std::vector<CERTName*>& valid_issuers);

// Generates a unique nickname for |slot|, returning |nickname| if it is
// already unique.
//
// Note: The nickname returned will NOT include the token name, thus the
// token name must be prepended if calling an NSS function that expects
// <token>:<nickname>.
// TODO(gspencer): Internationalize this: it's wrong to hard-code English.
std::string GetUniqueNicknameForSlot(const std::string& nickname,
                                     const SECItem* subject,
                                     PK11SlotInfo* slot);
#endif  // defined(USE_NSS) || defined(OS_IOS)

} // namespace x509_util

} // namespace net

#endif  // NET_CERT_X509_UTIL_NSS_H_
