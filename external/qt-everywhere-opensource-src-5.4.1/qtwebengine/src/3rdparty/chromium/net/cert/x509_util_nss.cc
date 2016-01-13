// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/x509_util.h"
#include "net/cert/x509_util_nss.h"

#include <cert.h>  // Must be included before certdb.h
#include <certdb.h>
#include <cryptohi.h>
#include <nss.h>
#include <pk11pub.h>
#include <prerror.h>
#include <secder.h>
#include <secmod.h>
#include <secport.h>

#include "base/debug/leak_annotations.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/pickle.h"
#include "base/strings/stringprintf.h"
#include "crypto/ec_private_key.h"
#include "crypto/nss_util.h"
#include "crypto/nss_util_internal.h"
#include "crypto/rsa_private_key.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/third_party/nss/chromium-nss.h"
#include "net/cert/x509_certificate.h"

namespace net {

namespace {

class DomainBoundCertOIDWrapper {
 public:
  static DomainBoundCertOIDWrapper* GetInstance() {
    // Instantiated as a leaky singleton to allow the singleton to be
    // constructed on a worker thead that is not joined when a process
    // shuts down.
    return Singleton<DomainBoundCertOIDWrapper,
                     LeakySingletonTraits<DomainBoundCertOIDWrapper> >::get();
  }

  SECOidTag domain_bound_cert_oid_tag() const {
    return domain_bound_cert_oid_tag_;
  }

 private:
  friend struct DefaultSingletonTraits<DomainBoundCertOIDWrapper>;

  DomainBoundCertOIDWrapper();

  SECOidTag domain_bound_cert_oid_tag_;

  DISALLOW_COPY_AND_ASSIGN(DomainBoundCertOIDWrapper);
};

DomainBoundCertOIDWrapper::DomainBoundCertOIDWrapper()
    : domain_bound_cert_oid_tag_(SEC_OID_UNKNOWN) {
  // 1.3.6.1.4.1.11129.2.1.6
  // (iso.org.dod.internet.private.enterprises.google.googleSecurity.
  //  certificateExtensions.originBoundCertificate)
  static const uint8 kObCertOID[] = {
    0x2b, 0x06, 0x01, 0x04, 0x01, 0xd6, 0x79, 0x02, 0x01, 0x06
  };
  SECOidData oid_data;
  memset(&oid_data, 0, sizeof(oid_data));
  oid_data.oid.data = const_cast<uint8*>(kObCertOID);
  oid_data.oid.len = sizeof(kObCertOID);
  oid_data.offset = SEC_OID_UNKNOWN;
  oid_data.desc = "Origin Bound Certificate";
  oid_data.mechanism = CKM_INVALID_MECHANISM;
  oid_data.supportedExtension = SUPPORTED_CERT_EXTENSION;
  domain_bound_cert_oid_tag_ = SECOID_AddEntry(&oid_data);
  if (domain_bound_cert_oid_tag_ == SEC_OID_UNKNOWN)
    LOG(ERROR) << "OB_CERT OID tag creation failed";
}

// Creates a Certificate object that may be passed to the SignCertificate
// method to generate an X509 certificate.
// Returns NULL if an error is encountered in the certificate creation
// process.
// Caller responsible for freeing returned certificate object.
CERTCertificate* CreateCertificate(
    SECKEYPublicKey* public_key,
    const std::string& subject,
    uint32 serial_number,
    base::Time not_valid_before,
    base::Time not_valid_after) {
  // Create info about public key.
  CERTSubjectPublicKeyInfo* spki =
      SECKEY_CreateSubjectPublicKeyInfo(public_key);
  if (!spki)
    return NULL;

  // Create the certificate request.
  CERTName* subject_name =
      CERT_AsciiToName(const_cast<char*>(subject.c_str()));
  CERTCertificateRequest* cert_request =
      CERT_CreateCertificateRequest(subject_name, spki, NULL);
  SECKEY_DestroySubjectPublicKeyInfo(spki);

  if (!cert_request) {
    PRErrorCode prerr = PR_GetError();
    LOG(ERROR) << "Failed to create certificate request: " << prerr;
    CERT_DestroyName(subject_name);
    return NULL;
  }

  CERTValidity* validity = CERT_CreateValidity(
      crypto::BaseTimeToPRTime(not_valid_before),
      crypto::BaseTimeToPRTime(not_valid_after));
  if (!validity) {
    PRErrorCode prerr = PR_GetError();
    LOG(ERROR) << "Failed to create certificate validity object: " << prerr;
    CERT_DestroyName(subject_name);
    CERT_DestroyCertificateRequest(cert_request);
    return NULL;
  }
  CERTCertificate* cert = CERT_CreateCertificate(serial_number, subject_name,
                                                 validity, cert_request);
  if (!cert) {
    PRErrorCode prerr = PR_GetError();
    LOG(ERROR) << "Failed to create certificate: " << prerr;
  }

  // Cleanup for resources used to generate the cert.
  CERT_DestroyName(subject_name);
  CERT_DestroyValidity(validity);
  CERT_DestroyCertificateRequest(cert_request);

  return cert;
}

SECOidTag ToSECOid(x509_util::DigestAlgorithm alg) {
  switch (alg) {
    case x509_util::DIGEST_SHA1:
      return SEC_OID_SHA1;
    case x509_util::DIGEST_SHA256:
      return SEC_OID_SHA256;
  }
  return SEC_OID_UNKNOWN;
}

// Signs a certificate object, with |key| generating a new X509Certificate
// and destroying the passed certificate object (even when NULL is returned).
// The logic of this method references SignCert() in NSS utility certutil:
// http://mxr.mozilla.org/security/ident?i=SignCert.
// Returns true on success or false if an error is encountered in the
// certificate signing process.
bool SignCertificate(
    CERTCertificate* cert,
    SECKEYPrivateKey* key,
    SECOidTag hash_algorithm) {
  // |arena| is used to encode the cert.
  PLArenaPool* arena = cert->arena;
  SECOidTag algo_id = SEC_GetSignatureAlgorithmOidTag(key->keyType,
                                                      hash_algorithm);
  if (algo_id == SEC_OID_UNKNOWN)
    return false;

  SECStatus rv = SECOID_SetAlgorithmID(arena, &cert->signature, algo_id, 0);
  if (rv != SECSuccess)
    return false;

  // Generate a cert of version 3.
  *(cert->version.data) = 2;
  cert->version.len = 1;

  SECItem der = { siBuffer, NULL, 0 };

  // Use ASN1 DER to encode the cert.
  void* encode_result = SEC_ASN1EncodeItem(
      NULL, &der, cert, SEC_ASN1_GET(CERT_CertificateTemplate));
  if (!encode_result)
    return false;

  // Allocate space to contain the signed cert.
  SECItem result = { siBuffer, NULL, 0 };

  // Sign the ASN1 encoded cert and save it to |result|.
  rv = DerSignData(arena, &result, &der, key, algo_id);
  PORT_Free(der.data);
  if (rv != SECSuccess) {
    DLOG(ERROR) << "DerSignData: " << PORT_GetError();
    return false;
  }

  // Save the signed result to the cert.
  cert->derCert = result;

  return true;
}

#if defined(USE_NSS) || defined(OS_IOS)
// Callback for CERT_DecodeCertPackage(), used in
// CreateOSCertHandlesFromBytes().
SECStatus PR_CALLBACK CollectCertsCallback(void* arg,
                                           SECItem** certs,
                                           int num_certs) {
  X509Certificate::OSCertHandles* results =
      reinterpret_cast<X509Certificate::OSCertHandles*>(arg);

  for (int i = 0; i < num_certs; ++i) {
    X509Certificate::OSCertHandle handle =
        X509Certificate::CreateOSCertHandleFromBytes(
            reinterpret_cast<char*>(certs[i]->data), certs[i]->len);
    if (handle)
      results->push_back(handle);
  }

  return SECSuccess;
}

typedef scoped_ptr<
    CERTName,
    crypto::NSSDestroyer<CERTName, CERT_DestroyName> > ScopedCERTName;

// Create a new CERTName object from its encoded representation.
// |arena| is the allocation pool to use.
// |data| points to a DER-encoded X.509 DistinguishedName.
// Return a new CERTName pointer on success, or NULL.
CERTName* CreateCertNameFromEncoded(PLArenaPool* arena,
                                    const base::StringPiece& data) {
  if (!arena)
    return NULL;

  ScopedCERTName name(PORT_ArenaZNew(arena, CERTName));
  if (!name.get())
    return NULL;

  SECItem item;
  item.len = static_cast<unsigned int>(data.length());
  item.data = reinterpret_cast<unsigned char*>(
      const_cast<char*>(data.data()));

  SECStatus rv = SEC_ASN1DecodeItem(
      arena, name.get(), SEC_ASN1_GET(CERT_NameTemplate), &item);
  if (rv != SECSuccess)
    return NULL;

  return name.release();
}

#endif  // defined(USE_NSS) || defined(OS_IOS)

}  // namespace

namespace x509_util {

bool CreateSelfSignedCert(crypto::RSAPrivateKey* key,
                          DigestAlgorithm alg,
                          const std::string& subject,
                          uint32 serial_number,
                          base::Time not_valid_before,
                          base::Time not_valid_after,
                          std::string* der_cert) {
  DCHECK(key);
  DCHECK(!strncmp(subject.c_str(), "CN=", 3U));
  CERTCertificate* cert = CreateCertificate(key->public_key(),
                                            subject,
                                            serial_number,
                                            not_valid_before,
                                            not_valid_after);
  if (!cert)
    return false;

  if (!SignCertificate(cert, key->key(), ToSECOid(alg))) {
    CERT_DestroyCertificate(cert);
    return false;
  }

  der_cert->assign(reinterpret_cast<char*>(cert->derCert.data),
                   cert->derCert.len);
  CERT_DestroyCertificate(cert);
  return true;
}

bool IsSupportedValidityRange(base::Time not_valid_before,
                              base::Time not_valid_after) {
  CERTValidity* validity = CERT_CreateValidity(
      crypto::BaseTimeToPRTime(not_valid_before),
      crypto::BaseTimeToPRTime(not_valid_after));

  if (!validity)
    return false;

  CERT_DestroyValidity(validity);
  return true;
}

bool CreateDomainBoundCertEC(crypto::ECPrivateKey* key,
                             DigestAlgorithm alg,
                             const std::string& domain,
                             uint32 serial_number,
                             base::Time not_valid_before,
                             base::Time not_valid_after,
                             std::string* der_cert) {
  DCHECK(key);

  CERTCertificate* cert = CreateCertificate(key->public_key(),
                                            "CN=anonymous.invalid",
                                            serial_number,
                                            not_valid_before,
                                            not_valid_after);

  if (!cert)
    return false;

  // Create opaque handle used to add extensions later.
  void* cert_handle;
  if ((cert_handle = CERT_StartCertExtensions(cert)) == NULL) {
    LOG(ERROR) << "Unable to get opaque handle for adding extensions";
    CERT_DestroyCertificate(cert);
    return false;
  }

  // Create SECItem for IA5String encoding.
  SECItem domain_string_item = {
    siAsciiString,
    (unsigned char*)domain.data(),
    static_cast<unsigned>(domain.size())
  };

  // IA5Encode and arena allocate SECItem
  SECItem* asn1_domain_string = SEC_ASN1EncodeItem(
      cert->arena, NULL, &domain_string_item,
      SEC_ASN1_GET(SEC_IA5StringTemplate));
  if (asn1_domain_string == NULL) {
    LOG(ERROR) << "Unable to get ASN1 encoding for domain in domain_bound_cert"
                  " extension";
    CERT_DestroyCertificate(cert);
    return false;
  }

  // Add the extension to the opaque handle
  if (CERT_AddExtension(
      cert_handle,
      DomainBoundCertOIDWrapper::GetInstance()->domain_bound_cert_oid_tag(),
      asn1_domain_string,
      PR_TRUE,
      PR_TRUE) != SECSuccess){
    LOG(ERROR) << "Unable to add domain bound cert extension to opaque handle";
    CERT_DestroyCertificate(cert);
    return false;
  }

  // Copy extension into x509 cert
  if (CERT_FinishExtensions(cert_handle) != SECSuccess){
    LOG(ERROR) << "Unable to copy extension to X509 cert";
    CERT_DestroyCertificate(cert);
    return false;
  }

  if (!SignCertificate(cert, key->key(), ToSECOid(alg))) {
    CERT_DestroyCertificate(cert);
    return false;
  }

  DCHECK(cert->derCert.len);
  // XXX copied from X509Certificate::GetDEREncoded
  der_cert->clear();
  der_cert->append(reinterpret_cast<char*>(cert->derCert.data),
                   cert->derCert.len);
  CERT_DestroyCertificate(cert);
  return true;
}

#if defined(USE_NSS) || defined(OS_IOS)
void ParsePrincipal(CERTName* name, CertPrincipal* principal) {
// Starting in NSS 3.15, CERTGetNameFunc takes a const CERTName* argument.
#if NSS_VMINOR >= 15
  typedef char* (*CERTGetNameFunc)(const CERTName* name);
#else
  typedef char* (*CERTGetNameFunc)(CERTName* name);
#endif

  // TODO(jcampan): add business_category and serial_number.
  // TODO(wtc): NSS has the CERT_GetOrgName, CERT_GetOrgUnitName, and
  // CERT_GetDomainComponentName functions, but they return only the most
  // general (the first) RDN.  NSS doesn't have a function for the street
  // address.
  static const SECOidTag kOIDs[] = {
    SEC_OID_AVA_STREET_ADDRESS,
    SEC_OID_AVA_ORGANIZATION_NAME,
    SEC_OID_AVA_ORGANIZATIONAL_UNIT_NAME,
    SEC_OID_AVA_DC };

  std::vector<std::string>* values[] = {
    &principal->street_addresses,
    &principal->organization_names,
    &principal->organization_unit_names,
    &principal->domain_components };
  DCHECK_EQ(arraysize(kOIDs), arraysize(values));

  CERTRDN** rdns = name->rdns;
  for (size_t rdn = 0; rdns[rdn]; ++rdn) {
    CERTAVA** avas = rdns[rdn]->avas;
    for (size_t pair = 0; avas[pair] != 0; ++pair) {
      SECOidTag tag = CERT_GetAVATag(avas[pair]);
      for (size_t oid = 0; oid < arraysize(kOIDs); ++oid) {
        if (kOIDs[oid] == tag) {
          SECItem* decode_item = CERT_DecodeAVAValue(&avas[pair]->value);
          if (!decode_item)
            break;
          // TODO(wtc): Pass decode_item to CERT_RFC1485_EscapeAndQuote.
          std::string value(reinterpret_cast<char*>(decode_item->data),
                            decode_item->len);
          values[oid]->push_back(value);
          SECITEM_FreeItem(decode_item, PR_TRUE);
          break;
        }
      }
    }
  }

  // Get CN, L, S, and C.
  CERTGetNameFunc get_name_funcs[4] = {
    CERT_GetCommonName, CERT_GetLocalityName,
    CERT_GetStateName, CERT_GetCountryName };
  std::string* single_values[4] = {
    &principal->common_name, &principal->locality_name,
    &principal->state_or_province_name, &principal->country_name };
  for (size_t i = 0; i < arraysize(get_name_funcs); ++i) {
    char* value = get_name_funcs[i](name);
    if (value) {
      single_values[i]->assign(value);
      PORT_Free(value);
    }
  }
}

void ParseDate(const SECItem* der_date, base::Time* result) {
  PRTime prtime;
  SECStatus rv = DER_DecodeTimeChoice(&prtime, der_date);
  DCHECK_EQ(SECSuccess, rv);
  *result = crypto::PRTimeToBaseTime(prtime);
}

std::string ParseSerialNumber(const CERTCertificate* certificate) {
  return std::string(reinterpret_cast<char*>(certificate->serialNumber.data),
                     certificate->serialNumber.len);
}

void GetSubjectAltName(CERTCertificate* cert_handle,
                       std::vector<std::string>* dns_names,
                       std::vector<std::string>* ip_addrs) {
  if (dns_names)
    dns_names->clear();
  if (ip_addrs)
    ip_addrs->clear();

  SECItem alt_name;
  SECStatus rv = CERT_FindCertExtension(cert_handle,
                                        SEC_OID_X509_SUBJECT_ALT_NAME,
                                        &alt_name);
  if (rv != SECSuccess)
    return;

  PLArenaPool* arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
  DCHECK(arena != NULL);

  CERTGeneralName* alt_name_list;
  alt_name_list = CERT_DecodeAltNameExtension(arena, &alt_name);
  SECITEM_FreeItem(&alt_name, PR_FALSE);

  CERTGeneralName* name = alt_name_list;
  while (name) {
    // DNSName and IPAddress are encoded as IA5String and OCTET STRINGs
    // respectively, both of which can be byte copied from
    // SECItemType::data into the appropriate output vector.
    if (dns_names && name->type == certDNSName) {
      dns_names->push_back(std::string(
          reinterpret_cast<char*>(name->name.other.data),
          name->name.other.len));
    } else if (ip_addrs && name->type == certIPAddress) {
      ip_addrs->push_back(std::string(
          reinterpret_cast<char*>(name->name.other.data),
          name->name.other.len));
    }
    name = CERT_GetNextGeneralName(name);
    if (name == alt_name_list)
      break;
  }
  PORT_FreeArena(arena, PR_FALSE);
}

X509Certificate::OSCertHandles CreateOSCertHandlesFromBytes(
    const char* data,
    int length,
    X509Certificate::Format format) {
  X509Certificate::OSCertHandles results;
  if (length < 0)
    return results;

  crypto::EnsureNSSInit();

  if (!NSS_IsInitialized())
    return results;

  switch (format) {
    case X509Certificate::FORMAT_SINGLE_CERTIFICATE: {
      X509Certificate::OSCertHandle handle =
          X509Certificate::CreateOSCertHandleFromBytes(data, length);
      if (handle)
        results.push_back(handle);
      break;
    }
    case X509Certificate::FORMAT_PKCS7: {
      // Make a copy since CERT_DecodeCertPackage may modify it
      std::vector<char> data_copy(data, data + length);

      SECStatus result = CERT_DecodeCertPackage(&data_copy[0],
          length, CollectCertsCallback, &results);
      if (result != SECSuccess)
        results.clear();
      break;
    }
    default:
      NOTREACHED() << "Certificate format " << format << " unimplemented";
      break;
  }

  return results;
}

X509Certificate::OSCertHandle ReadOSCertHandleFromPickle(
    PickleIterator* pickle_iter) {
  const char* data;
  int length;
  if (!pickle_iter->ReadData(&data, &length))
    return NULL;

  return X509Certificate::CreateOSCertHandleFromBytes(data, length);
}

void GetPublicKeyInfo(CERTCertificate* handle,
                      size_t* size_bits,
                      X509Certificate::PublicKeyType* type) {
  // Since we might fail, set the output parameters to default values first.
  *type = X509Certificate::kPublicKeyTypeUnknown;
  *size_bits = 0;

  crypto::ScopedSECKEYPublicKey key(CERT_ExtractPublicKey(handle));
  if (!key.get())
    return;

  *size_bits = SECKEY_PublicKeyStrengthInBits(key.get());

  switch (key->keyType) {
    case rsaKey:
      *type = X509Certificate::kPublicKeyTypeRSA;
      break;
    case dsaKey:
      *type = X509Certificate::kPublicKeyTypeDSA;
      break;
    case dhKey:
      *type = X509Certificate::kPublicKeyTypeDH;
      break;
    case ecKey:
      *type = X509Certificate::kPublicKeyTypeECDSA;
      break;
    default:
      *type = X509Certificate::kPublicKeyTypeUnknown;
      *size_bits = 0;
      break;
  }
}

bool GetIssuersFromEncodedList(
    const std::vector<std::string>& encoded_issuers,
    PLArenaPool* arena,
    std::vector<CERTName*>* out) {
  std::vector<CERTName*> result;
  for (size_t n = 0; n < encoded_issuers.size(); ++n) {
    CERTName* name = CreateCertNameFromEncoded(arena, encoded_issuers[n]);
    if (name != NULL)
      result.push_back(name);
  }

  if (result.size() == encoded_issuers.size()) {
    out->swap(result);
    return true;
  }

  for (size_t n = 0; n < result.size(); ++n)
    CERT_DestroyName(result[n]);
  return false;
}


bool IsCertificateIssuedBy(const std::vector<CERTCertificate*>& cert_chain,
                           const std::vector<CERTName*>& valid_issuers) {
  for (size_t n = 0; n < cert_chain.size(); ++n) {
    CERTName* cert_issuer = &cert_chain[n]->issuer;
    for (size_t i = 0; i < valid_issuers.size(); ++i) {
      if (CERT_CompareName(valid_issuers[i], cert_issuer) == SECEqual)
        return true;
    }
  }
  return false;
}

std::string GetUniqueNicknameForSlot(const std::string& nickname,
                                     const SECItem* subject,
                                     PK11SlotInfo* slot) {
  int index = 2;
  std::string new_name = nickname;
  std::string temp_nickname = new_name;
  std::string token_name;

  if (!slot)
    return new_name;

  if (!PK11_IsInternalKeySlot(slot)) {
    token_name.assign(PK11_GetTokenName(slot));
    token_name.append(":");

    temp_nickname = token_name + new_name;
  }

  while (SEC_CertNicknameConflict(temp_nickname.c_str(),
                                  const_cast<SECItem*>(subject),
                                  CERT_GetDefaultCertDB())) {
    base::SStringPrintf(&new_name, "%s #%d", nickname.c_str(), index++);
    temp_nickname = token_name + new_name;
  }

  return new_name;
}

#endif  // defined(USE_NSS) || defined(OS_IOS)

} // namespace x509_util

} // namespace net
