// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/ct_objects_extractor.h"

#include <cert.h>
#include <secasn1.h>
#include <secitem.h>
#include <secoid.h>

#include "base/lazy_instance.h"
#include "base/sha1.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/sha2.h"
#include "net/cert/asn1_util.h"
#include "net/cert/scoped_nss_types.h"
#include "net/cert/signed_certificate_timestamp.h"

namespace net {

namespace ct {

namespace {

// NSS black magic to get the address of externally defined template at runtime.
SEC_ASN1_MKSUB(SEC_IntegerTemplate)
SEC_ASN1_MKSUB(SECOID_AlgorithmIDTemplate)
SEC_ASN1_MKSUB(SEC_GeneralizedTimeTemplate)
SEC_ASN1_MKSUB(CERT_SequenceOfCertExtensionTemplate)

// Wrapper class to convert a X509Certificate::OSCertHandle directly
// into a CERTCertificate* usable with other NSS functions. This is used for
// platforms where X509Certificate::OSCertHandle refers to a different type
// than a CERTCertificate*.
struct NSSCertWrapper {
  explicit NSSCertWrapper(X509Certificate::OSCertHandle cert_handle);
  ~NSSCertWrapper() {}

  ScopedCERTCertificate cert;
};

NSSCertWrapper::NSSCertWrapper(X509Certificate::OSCertHandle cert_handle) {
#if defined(USE_NSS)
  cert.reset(CERT_DupCertificate(cert_handle));
#else
  SECItem der_cert;
  std::string der_data;
  if (!X509Certificate::GetDEREncoded(cert_handle, &der_data))
    return;
  der_cert.data =
      reinterpret_cast<unsigned char*>(const_cast<char*>(der_data.data()));
  der_cert.len = der_data.size();

  // Note: CERT_NewTempCertificate may return NULL if the certificate
  // shares a serial number with another cert issued by the same CA,
  // which is not supposed to happen.
  cert.reset(CERT_NewTempCertificate(
      CERT_GetDefaultCertDB(), &der_cert, NULL, PR_FALSE, PR_TRUE));
#endif
  DCHECK(cert.get() != NULL);
}

// The wire form of the OID 1.3.6.1.4.1.11129.2.4.2. See Section 3.3 of
// RFC6962.
const unsigned char kEmbeddedSCTOid[] = {0x2B, 0x06, 0x01, 0x04, 0x01,
                                         0xD6, 0x79, 0x02, 0x04, 0x02};
const char kEmbeddedSCTDescription[] =
    "X.509v3 Certificate Transparency Embedded Signed Certificate Timestamp "
    "List";

// The wire form of the OID 1.3.6.1.4.1.11129.2.4.5 - OCSP SingleExtension for
// X.509v3 Certificate Transparency Signed Certificate Timestamp List, see
// Section 3.3 of RFC6962.
const unsigned char kOCSPExtensionOid[] = {0x2B, 0x06, 0x01, 0x04, 0x01,
                                           0xD6, 0x79, 0x02, 0x04, 0x05};

const SECItem kOCSPExtensionOidItem = {
  siBuffer, const_cast<unsigned char*>(kOCSPExtensionOid),
  sizeof(kOCSPExtensionOid)
};

// id-ad-ocsp: 1.3.6.1.5.5.7.48.1.1
const unsigned char kBasicOCSPResponseOid[] = {0x2B, 0x06, 0x01, 0x05, 0x05,
                                               0x07, 0x30, 0x01, 0x01};

const SECItem kBasicOCSPResponseOidItem = {
  siBuffer, const_cast<unsigned char*>(kBasicOCSPResponseOid),
  sizeof(kBasicOCSPResponseOid)
};


// Initializes the necessary NSS internals for use with Certificate
// Transparency.
class CTInitSingleton {
 public:
  SECOidTag embedded_oid() const { return embedded_oid_; }

 private:
  friend struct base::DefaultLazyInstanceTraits<CTInitSingleton>;

  CTInitSingleton() : embedded_oid_(SEC_OID_UNKNOWN) {
    embedded_oid_ = RegisterOid(
        kEmbeddedSCTOid, sizeof(kEmbeddedSCTOid), kEmbeddedSCTDescription);
  }

  ~CTInitSingleton() {}

  SECOidTag RegisterOid(const unsigned char* oid,
                        unsigned int oid_len,
                        const char* description) {
    SECOidData oid_data;
    oid_data.oid.len = oid_len;
    oid_data.oid.data = const_cast<unsigned char*>(oid);
    oid_data.offset = SEC_OID_UNKNOWN;
    oid_data.desc = description;
    oid_data.mechanism = CKM_INVALID_MECHANISM;
    // Setting this to SUPPORTED_CERT_EXTENSION ensures that if a certificate
    // contains this extension with the critical bit set, NSS will not reject
    // it. However, because verification of this extension happens after NSS,
    // it is currently left as INVALID_CERT_EXTENSION.
    oid_data.supportedExtension = INVALID_CERT_EXTENSION;

    SECOidTag result = SECOID_AddEntry(&oid_data);
    CHECK_NE(SEC_OID_UNKNOWN, result);

    return result;
  }

  SECOidTag embedded_oid_;

  DISALLOW_COPY_AND_ASSIGN(CTInitSingleton);
};

base::LazyInstance<CTInitSingleton>::Leaky g_ct_singleton =
    LAZY_INSTANCE_INITIALIZER;

// Obtains the data for an X.509v3 certificate extension identified by |oid|
// and encoded as an OCTET STRING. Returns true if the extension was found in
// the certificate, updating |ext_data| to be the extension data after removing
// the DER encoding of OCTET STRING.
bool GetCertOctetStringExtension(CERTCertificate* cert,
                                 SECOidTag oid,
                                 std::string* extension_data) {
  SECItem extension;
  SECStatus rv = CERT_FindCertExtension(cert, oid, &extension);
  if (rv != SECSuccess)
    return false;

  base::StringPiece raw_data(reinterpret_cast<char*>(extension.data),
                             extension.len);
  base::StringPiece parsed_data;
  if (!asn1::GetElement(&raw_data, asn1::kOCTETSTRING, &parsed_data) ||
      raw_data.size() > 0) { // Decoding failure or raw data left
    rv = SECFailure;
  } else {
    parsed_data.CopyToString(extension_data);
  }

  SECITEM_FreeItem(&extension, PR_FALSE);
  return rv == SECSuccess;
}

// NSS offers CERT_FindCertExtension for certificates, but that only accepts
// CERTCertificate* inputs, so the method below extracts the SCT extension
// directly from the CERTCertExtension** of an OCSP response.
//
// Obtains the data for an OCSP extension identified by kOCSPExtensionOidItem
// and encoded as an OCTET STRING. Returns true if the extension was found in
// |extensions|, updating |extension_data| to be the extension data after
// removing the DER encoding of OCTET STRING.
bool GetSCTListFromOCSPExtension(PLArenaPool* arena,
                                 const CERTCertExtension* const* extensions,
                                 std::string* extension_data) {
  if (!extensions)
    return false;

  const CERTCertExtension* match = NULL;

  for (const CERTCertExtension* const* exts = extensions; *exts; ++exts) {
    const CERTCertExtension* ext = *exts;
    if (SECITEM_ItemsAreEqual(&kOCSPExtensionOidItem, &ext->id)) {
      match = ext;
      break;
    }
  }

  if (!match)
    return false;

  SECItem contents;
  // SEC_QuickDERDecodeItem sets |contents| to point to |match|, so it is not
  // necessary to free the contents of |contents|.
  SECStatus rv = SEC_QuickDERDecodeItem(arena, &contents,
                                        SEC_ASN1_GET(SEC_OctetStringTemplate),
                                        &match->value);
  if (rv != SECSuccess)
    return false;

  base::StringPiece parsed_data(reinterpret_cast<char*>(contents.data),
                                contents.len);
  parsed_data.CopyToString(extension_data);
  return true;
}

// Given a |cert|, extract the TBSCertificate from this certificate, also
// removing the X.509 extension with OID 1.3.6.1.4.1.11129.2.4.2 (that is,
// the embedded SCT)
bool ExtractTBSCertWithoutSCTs(CERTCertificate* cert,
                               std::string* to_be_signed) {
  SECOidData* oid = SECOID_FindOIDByTag(g_ct_singleton.Get().embedded_oid());
  if (!oid)
    return false;

  // This is a giant hack, due to the fact that NSS does not expose a good API
  // for simply removing certificate fields from existing certificates.
  CERTCertificate temp_cert;
  temp_cert = *cert;
  temp_cert.extensions = NULL;

  // Strip out the embedded SCT OID from the new certificate by directly
  // mutating the extensions in place.
  std::vector<CERTCertExtension*> new_extensions;
  if (cert->extensions) {
    for (CERTCertExtension** exts = cert->extensions; *exts; ++exts) {
      CERTCertExtension* ext = *exts;
      SECComparison result = SECITEM_CompareItem(&oid->oid, &ext->id);
      if (result != SECEqual)
        new_extensions.push_back(ext);
    }
  }
  if (!new_extensions.empty()) {
    new_extensions.push_back(NULL);
    temp_cert.extensions = &new_extensions[0];
  }

  crypto::ScopedPLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));

  SECItem tbs_data;
  tbs_data.len = 0;
  tbs_data.data = NULL;
  void* result = SEC_ASN1EncodeItem(arena.get(),
                                    &tbs_data,
                                    &temp_cert,
                                    SEC_ASN1_GET(CERT_CertificateTemplate));
  if (!result)
    return false;

  to_be_signed->assign(reinterpret_cast<char*>(tbs_data.data), tbs_data.len);
  return true;
}

// The following code is adapted from the NSS OCSP module, in order to expose
// the internal structure of an OCSP response.

// ResponseBytes ::=       SEQUENCE {
//     responseType   OBJECT IDENTIFIER,
//     response       OCTET STRING }
struct ResponseBytes {
  SECItem response_type;
  SECItem der_response;
};

const SEC_ASN1Template kResponseBytesTemplate[] = {
  { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(ResponseBytes) },
  { SEC_ASN1_OBJECT_ID, offsetof(ResponseBytes, response_type) },
  { SEC_ASN1_OCTET_STRING, offsetof(ResponseBytes, der_response) },
  { 0 }
};

// OCSPResponse ::=     SEQUENCE {
//      responseStatus          OCSPResponseStatus,
//      responseBytes           [0] EXPLICIT ResponseBytes OPTIONAL }
struct OCSPResponse {
  SECItem response_status;
  // This indirection is needed because |response_bytes| is an optional
  // component and we need a way to determine if it is missing.
  ResponseBytes* response_bytes;
};

const SEC_ASN1Template kPointerToResponseBytesTemplate[] = {
  { SEC_ASN1_POINTER, 0, kResponseBytesTemplate }
};

const SEC_ASN1Template kOCSPResponseTemplate[] = {
  { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(OCSPResponse) },
  { SEC_ASN1_ENUMERATED, offsetof(OCSPResponse, response_status) },
  { SEC_ASN1_OPTIONAL | SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED |
    SEC_ASN1_CONTEXT_SPECIFIC | 0, offsetof(OCSPResponse, response_bytes),
    kPointerToResponseBytesTemplate },
  { 0 }
};

// CertID          ::=     SEQUENCE {
//   hashAlgorithm       AlgorithmIdentifier,
//   issuerNameHash      OCTET STRING, -- Hash of Issuer's DN
//   issuerKeyHash       OCTET STRING, -- Hash of Issuers public key
//   serialNumber        CertificateSerialNumber }
struct CertID {
  SECAlgorithmID hash_algorithm;
  SECItem issuer_name_hash;
  SECItem issuer_key_hash;
  SECItem serial_number;
};

const SEC_ASN1Template kCertIDTemplate[] = {
  { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(CertID) },
  { SEC_ASN1_INLINE | SEC_ASN1_XTRN, offsetof(CertID, hash_algorithm),
    SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate) },
  { SEC_ASN1_OCTET_STRING, offsetof(CertID, issuer_name_hash) },
  { SEC_ASN1_OCTET_STRING, offsetof(CertID, issuer_key_hash) },
  { SEC_ASN1_INTEGER, offsetof(CertID, serial_number) },
  { 0 }
};

// SingleResponse ::= SEQUENCE {
//   certID                       CertID,
//   certStatus                   CertStatus,
//   thisUpdate                   GeneralizedTime,
//   nextUpdate           [0]     EXPLICIT GeneralizedTime OPTIONAL,
//   singleExtensions     [1]     EXPLICIT Extensions OPTIONAL }
struct SingleResponse {
  CertID cert_id;
  // The following three fields are not used.
  SECItem der_cert_status;
  SECItem this_update;
  SECItem next_update;
  CERTCertExtension** single_extensions;
};

const SEC_ASN1Template kSingleResponseTemplate[] = {
  { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SingleResponse) },
  { SEC_ASN1_INLINE, offsetof(SingleResponse, cert_id), kCertIDTemplate },
  // Really a CHOICE but we make it an ANY because  we don't care about the
  // contents of this field.
  // TODO(ekasper): use SEC_ASN1_CHOICE.
  { SEC_ASN1_ANY, offsetof(SingleResponse, der_cert_status) },
  { SEC_ASN1_GENERALIZED_TIME, offsetof(SingleResponse, this_update) },
  { SEC_ASN1_OPTIONAL | SEC_ASN1_EXPLICIT |
    SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_XTRN | 0,
    offsetof(SingleResponse, next_update),
    SEC_ASN1_SUB(SEC_GeneralizedTimeTemplate) },
  { SEC_ASN1_OPTIONAL | SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED |
    SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_XTRN | 1,
    offsetof(SingleResponse, single_extensions),
    SEC_ASN1_SUB(CERT_SequenceOfCertExtensionTemplate) },
  { 0 }
};

// ResponseData ::= SEQUENCE {
//   version              [0] EXPLICIT Version DEFAULT v1,
//   responderID              ResponderID,
//   producedAt               GeneralizedTime,
//   responses                SEQUENCE OF SingleResponse,
//   responseExtensions   [1] EXPLICIT Extensions OPTIONAL }
struct ResponseData {
  // The first three fields are not used.
  SECItem version;
  SECItem der_responder_id;
  SECItem produced_at;
  SingleResponse** single_responses;
  // Skip extensions.
};

const SEC_ASN1Template kResponseDataTemplate[] = {
  { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(ResponseData) },
  { SEC_ASN1_OPTIONAL | SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED |
    SEC_ASN1_CONTEXT_SPECIFIC | SEC_ASN1_XTRN | 0,
    offsetof(ResponseData, version), SEC_ASN1_SUB(SEC_IntegerTemplate) },
  // Really a CHOICE but we make it an ANY because  we don't care about the
  // contents of this field.
  // TODO(ekasper): use SEC_ASN1_CHOICE.
  { SEC_ASN1_ANY, offsetof(ResponseData, der_responder_id) },
  { SEC_ASN1_GENERALIZED_TIME, offsetof(ResponseData, produced_at) },
  { SEC_ASN1_SEQUENCE_OF, offsetof(ResponseData, single_responses),
    kSingleResponseTemplate },
  { SEC_ASN1_SKIP_REST },
  { 0 }
};

// BasicOCSPResponse       ::= SEQUENCE {
//   tbsResponseData      ResponseData,
//   signatureAlgorithm   AlgorithmIdentifier,
//   signature            BIT STRING,
//   certs                [0] EXPLICIT SEQUENCE OF Certificate OPTIONAL }
struct BasicOCSPResponse {
  ResponseData tbs_response_data;
  // We do not care about the rest.
};

const SEC_ASN1Template kBasicOCSPResponseTemplate[] = {
  { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(BasicOCSPResponse) },
  { SEC_ASN1_INLINE, offsetof(BasicOCSPResponse, tbs_response_data),
    kResponseDataTemplate },
  { SEC_ASN1_SKIP_REST },
  { 0 }
};

bool StringEqualToSECItem(const std::string& value1, const SECItem& value2) {
  if (value1.size() != value2.len)
    return false;
  return memcmp(value1.data(), value2.data, value2.len) == 0;
}

// TODO(ekasper): also use the issuer name hash in matching.
bool CertIDMatches(const CertID& cert_id,
                   const std::string& serial_number,
                   const std::string& issuer_key_sha1_hash,
                   const std::string& issuer_key_sha256_hash) {
  if (!StringEqualToSECItem(serial_number, cert_id.serial_number))
    return false;

  SECOidTag hash_alg = SECOID_FindOIDTag(&cert_id.hash_algorithm.algorithm);
  switch (hash_alg) {
    case SEC_OID_SHA1:
      return StringEqualToSECItem(issuer_key_sha1_hash,
                                  cert_id.issuer_key_hash);
    case SEC_OID_SHA256:
      return StringEqualToSECItem(issuer_key_sha256_hash,
                                  cert_id.issuer_key_hash);
    default:
      return false;
  }
}

}  // namespace

bool ExtractEmbeddedSCTList(X509Certificate::OSCertHandle cert,
                            std::string* sct_list) {
  DCHECK(cert);

  NSSCertWrapper leaf_cert(cert);
  if (!leaf_cert.cert)
    return false;

  return GetCertOctetStringExtension(leaf_cert.cert.get(),
                                     g_ct_singleton.Get().embedded_oid(),
                                     sct_list);
}

bool GetPrecertLogEntry(X509Certificate::OSCertHandle leaf,
                        X509Certificate::OSCertHandle issuer,
                        LogEntry* result) {
  DCHECK(leaf);
  DCHECK(issuer);

  NSSCertWrapper leaf_cert(leaf);
  NSSCertWrapper issuer_cert(issuer);

  result->Reset();
  // XXX(rsleevi): This check may be overkill, since we should be able to
  // generate precerts for certs without the extension. For now, just a sanity
  // check to match the reference implementation.
  SECItem extension;
  SECStatus rv = CERT_FindCertExtension(
      leaf_cert.cert.get(), g_ct_singleton.Get().embedded_oid(), &extension);
  if (rv != SECSuccess)
    return false;
  SECITEM_FreeItem(&extension, PR_FALSE);

  std::string to_be_signed;
  if (!ExtractTBSCertWithoutSCTs(leaf_cert.cert.get(), &to_be_signed))
    return false;

  if (!issuer_cert.cert) {
    // This can happen when the issuer and leaf certs share the same serial
    // number and are from the same CA, which should never be the case
    // (but happened with bad test certs).
    VLOG(1) << "Issuer cert is null, cannot generate Precert entry.";
    return false;
  }

  SECKEYPublicKey* issuer_pub_key =
      SECKEY_ExtractPublicKey(&(issuer_cert.cert->subjectPublicKeyInfo));
  if (!issuer_pub_key) {
    VLOG(1) << "Could not extract issuer public key, "
            << "cannot generate Precert entry.";
    return false;
  }

  SECItem* encoded_issuer_pubKey =
      SECKEY_EncodeDERSubjectPublicKeyInfo(issuer_pub_key);
  if (!encoded_issuer_pubKey) {
    SECKEY_DestroyPublicKey(issuer_pub_key);
    return false;
  }

  result->type = ct::LogEntry::LOG_ENTRY_TYPE_PRECERT;
  result->tbs_certificate.swap(to_be_signed);

  crypto::SHA256HashString(
      base::StringPiece(reinterpret_cast<char*>(encoded_issuer_pubKey->data),
                        encoded_issuer_pubKey->len),
      result->issuer_key_hash.data,
      sizeof(result->issuer_key_hash.data));

  SECITEM_FreeItem(encoded_issuer_pubKey, PR_TRUE);
  SECKEY_DestroyPublicKey(issuer_pub_key);

  return true;
}

bool GetX509LogEntry(X509Certificate::OSCertHandle leaf, LogEntry* result) {
  DCHECK(leaf);

  std::string encoded;
  if (!X509Certificate::GetDEREncoded(leaf, &encoded))
    return false;

  result->Reset();
  result->type = ct::LogEntry::LOG_ENTRY_TYPE_X509;
  result->leaf_certificate.swap(encoded);
  return true;
}

bool ExtractSCTListFromOCSPResponse(X509Certificate::OSCertHandle issuer,
                                    const std::string& cert_serial_number,
                                    const std::string& ocsp_response,
                                    std::string* sct_list) {
  DCHECK(issuer);

  // Any OCSP response is unlikely to be even close to 2^24 bytes; further, CT
  // only uses stapled OCSP responses which have this limit imposed by the TLS
  // protocol.
  if (ocsp_response.size() > 0xffffff)
    return false;

  crypto::ScopedPLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));

  OCSPResponse response;
  memset(&response, 0, sizeof(response));

  SECItem src = { siBuffer,
                  reinterpret_cast<unsigned char*>(const_cast<char*>(
                      ocsp_response.data())),
                  static_cast<unsigned int>(ocsp_response.size()) };

  // |response| will point directly into |src|, so it's not necessary to
  // free the |response| contents, but they may only be used while |src|
  // is valid (i.e., in this method).
  SECStatus rv = SEC_QuickDERDecodeItem(arena.get(), &response,
                                        kOCSPResponseTemplate, &src);
  if (rv != SECSuccess)
    return false;

  if (!response.response_bytes)
    return false;

  if (!SECITEM_ItemsAreEqual(&kBasicOCSPResponseOidItem,
                             &response.response_bytes->response_type)) {
    return false;
  }

  BasicOCSPResponse basic_response;
  memset(&basic_response, 0, sizeof(basic_response));

  rv = SEC_QuickDERDecodeItem(arena.get(), &basic_response,
                              kBasicOCSPResponseTemplate,
                              &response.response_bytes->der_response);
  if (rv != SECSuccess)
    return false;

  SingleResponse** responses =
      basic_response.tbs_response_data.single_responses;
  if (!responses)
    return false;

  std::string issuer_der;
  if (!X509Certificate::GetDEREncoded(issuer, &issuer_der))
    return false;

  base::StringPiece issuer_spki;
  if (!asn1::ExtractSPKIFromDERCert(issuer_der, &issuer_spki))
    return false;

  // In OCSP, only the key itself is under hash.
  base::StringPiece issuer_spk;
  if (!asn1::ExtractSubjectPublicKeyFromSPKI(issuer_spki, &issuer_spk))
    return false;

  // ExtractSubjectPublicKey... does not remove the initial octet encoding the
  // number of unused bits in the ASN.1 BIT STRING so we do it here. For public
  // keys, the bitstring is in practice always byte-aligned.
  if (issuer_spk.empty() || issuer_spk[0] != 0)
    return false;
  issuer_spk.remove_prefix(1);

  // NSS OCSP lib recognizes SHA1, MD5 and MD2; MD5 and MD2 are dead but
  // https://bugzilla.mozilla.org/show_bug.cgi?id=663315 will add SHA-256
  // and SHA-384.
  // TODO(ekasper): add SHA-384 to crypto/sha2.h and here if it proves
  // necessary.
  // TODO(ekasper): only compute the hashes on demand.
  std::string issuer_key_sha256_hash = crypto::SHA256HashString(issuer_spk);
  std::string issuer_key_sha1_hash = base::SHA1HashString(
      issuer_spk.as_string());

  const SingleResponse* match = NULL;
  for (const SingleResponse* const* resps = responses; *resps; ++resps) {
    const SingleResponse* resp = *resps;
    if (CertIDMatches(resp->cert_id, cert_serial_number,
                      issuer_key_sha1_hash, issuer_key_sha256_hash)) {
      match = resp;
      break;
    }
  }

  if (!match)
    return false;

  return GetSCTListFromOCSPExtension(arena.get(), match->single_extensions,
                                     sct_list);
}

}  // namespace ct

}  // namespace net
