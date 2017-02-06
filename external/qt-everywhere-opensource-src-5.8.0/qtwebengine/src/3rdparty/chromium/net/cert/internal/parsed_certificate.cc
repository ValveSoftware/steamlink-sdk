// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/parsed_certificate.h"

#include "net/cert/internal/name_constraints.h"
#include "net/cert/internal/signature_algorithm.h"
#include "net/cert/internal/verify_name_match.h"
#include "net/der/parser.h"

namespace net {

namespace {

WARN_UNUSED_RESULT bool GetSequenceValue(const der::Input& tlv,
                                         der::Input* value) {
  der::Parser parser(tlv);
  return parser.ReadTag(der::kSequence, value) && !parser.HasMore();
}

}  // namespace

ParsedCertificate::ParsedCertificate() {}
ParsedCertificate::~ParsedCertificate() {}

scoped_refptr<ParsedCertificate> ParsedCertificate::CreateFromCertificateData(
    const uint8_t* data,
    size_t length,
    DataSource source,
    const ParseCertificateOptions& options) {
  scoped_refptr<ParsedCertificate> result(new ParsedCertificate);

  switch (source) {
    case DataSource::INTERNAL_COPY:
      result->cert_data_.assign(data, data + length);
      result->cert_ =
          der::Input(result->cert_data_.data(), result->cert_data_.size());
      break;
    case DataSource::EXTERNAL_REFERENCE:
      result->cert_ = der::Input(data, length);
      break;
  }

  if (!ParseCertificate(result->cert_, &result->tbs_certificate_tlv_,
                        &result->signature_algorithm_tlv_,
                        &result->signature_value_)) {
    return nullptr;
  }

  if (!ParseTbsCertificate(result->tbs_certificate_tlv_, options,
                           &result->tbs_)) {
    return nullptr;
  }

  // Attempt to parse the signature algorithm contained in the Certificate.
  // Do not give up on failure here, since SignatureAlgorithm::CreateFromDer
  // will fail on valid but unsupported signature algorithms.
  // TODO(mattm): should distinguish between unsupported algorithms and parsing
  // errors.
  result->signature_algorithm_ =
      SignatureAlgorithm::CreateFromDer(result->signature_algorithm_tlv_);

  der::Input subject_value;
  if (!GetSequenceValue(result->tbs_.subject_tlv, &subject_value) ||
      !NormalizeName(subject_value, &result->normalized_subject_)) {
    return nullptr;
  }
  der::Input issuer_value;
  if (!GetSequenceValue(result->tbs_.issuer_tlv, &issuer_value) ||
      !NormalizeName(issuer_value, &result->normalized_issuer_)) {
    return nullptr;
  }

  // Parse the standard X.509 extensions and remove them from
  // |unparsed_extensions|.
  if (result->tbs_.has_extensions) {
    // ParseExtensions() ensures there are no duplicates, and maps the (unique)
    // OID to the extension value.
    if (!ParseExtensions(result->tbs_.extensions_tlv,
                         &result->unparsed_extensions_)) {
      return nullptr;
    }

    ParsedExtension extension;

    // Basic constraints.
    if (ConsumeExtension(BasicConstraintsOid(), &result->unparsed_extensions_,
                         &extension)) {
      result->has_basic_constraints_ = true;
      if (!ParseBasicConstraints(extension.value, &result->basic_constraints_))
        return nullptr;
    }

    // KeyUsage.
    if (ConsumeExtension(KeyUsageOid(), &result->unparsed_extensions_,
                         &extension)) {
      result->has_key_usage_ = true;
      if (!ParseKeyUsage(extension.value, &result->key_usage_))
        return nullptr;
    }

    // Subject alternative name.
    if (ConsumeExtension(SubjectAltNameOid(), &result->unparsed_extensions_,
                         &result->subject_alt_names_extension_)) {
      // RFC 5280 section 4.2.1.6:
      // SubjectAltName ::= GeneralNames
      result->subject_alt_names_ = GeneralNames::CreateFromDer(
          result->subject_alt_names_extension_.value);
      if (!result->subject_alt_names_)
        return nullptr;
      // RFC 5280 section 4.1.2.6:
      // If subject naming information is present only in the subjectAltName
      // extension (e.g., a key bound only to an email address or URI), then the
      // subject name MUST be an empty sequence and the subjectAltName extension
      // MUST be critical.
      if (subject_value.Length() == 0 &&
          !result->subject_alt_names_extension_.critical) {
        return nullptr;
      }
    }

    // Name constraints.
    if (ConsumeExtension(NameConstraintsOid(), &result->unparsed_extensions_,
                         &extension)) {
      result->name_constraints_ =
          NameConstraints::CreateFromDer(extension.value, extension.critical);
      if (!result->name_constraints_)
        return nullptr;
    }

    // Authority information access.
    if (ConsumeExtension(AuthorityInfoAccessOid(),
                         &result->unparsed_extensions_,
                         &result->authority_info_access_extension_)) {
      result->has_authority_info_access_ = true;
      if (!ParseAuthorityInfoAccess(
              result->authority_info_access_extension_.value,
              &result->ca_issuers_uris_, &result->ocsp_uris_))
        return nullptr;
    }

    // NOTE: if additional extensions are consumed here, the verification code
    // must be updated to process those extensions, since the
    // VerifyNoUnconsumedCriticalExtensions uses the unparsed_extensions_
    // variable to tell which extensions were processed.
  }

  return result;
}

scoped_refptr<ParsedCertificate> ParsedCertificate::CreateFromCertificateCopy(
    const base::StringPiece& data,
    const ParseCertificateOptions& options) {
  return ParsedCertificate::CreateFromCertificateData(
      reinterpret_cast<const uint8_t*>(data.data()), data.size(),
      DataSource::INTERNAL_COPY, options);
}

bool ParsedCertificate::CreateAndAddToVector(
    const uint8_t* data,
    size_t length,
    DataSource source,
    const ParseCertificateOptions& options,
    std::vector<scoped_refptr<ParsedCertificate>>* chain) {
  scoped_refptr<ParsedCertificate> cert(
      CreateFromCertificateData(data, length, source, options));
  if (!cert)
    return false;
  chain->push_back(std::move(cert));
  return true;
}

}  // namespace net
