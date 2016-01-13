// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/asn1_util.h"

namespace net {

namespace asn1 {

bool ParseElement(base::StringPiece* in,
                  unsigned tag_value,
                  base::StringPiece* out,
                  unsigned *out_header_len) {
  const uint8* data = reinterpret_cast<const uint8*>(in->data());

  // We don't support kAny and kOptional at the same time.
  if ((tag_value & kAny) && (tag_value & kOptional))
    return false;

  if (in->empty() && (tag_value & kOptional)) {
    if (out_header_len)
      *out_header_len = 0;
    if (out)
      *out = base::StringPiece();
    return true;
  }

  if (in->size() < 2)
    return false;

  if (tag_value != kAny &&
      static_cast<unsigned char>(data[0]) != (tag_value & 0xff)) {
    if (tag_value & kOptional) {
      if (out_header_len)
        *out_header_len = 0;
      if (out)
        *out = base::StringPiece();
      return true;
    }
    return false;
  }

  size_t len = 0;
  if ((data[1] & 0x80) == 0) {
    // short form length
    if (out_header_len)
      *out_header_len = 2;
    len = static_cast<size_t>(data[1]) + 2;
  } else {
    // long form length
    const unsigned num_bytes = data[1] & 0x7f;
    if (num_bytes == 0 || num_bytes > 2)
      return false;
    if (in->size() < 2 + num_bytes)
      return false;
    len = data[2];
    if (num_bytes == 2) {
      if (len == 0) {
        // the length encoding must be minimal.
        return false;
      }
      len <<= 8;
      len += data[3];
    }
    if (len < 128) {
      // the length should have been encoded in short form. This distinguishes
      // DER from BER encoding.
      return false;
    }
    if (out_header_len)
      *out_header_len = 2 + num_bytes;
    len += 2 + num_bytes;
  }

  if (in->size() < len)
    return false;
  if (out)
    *out = base::StringPiece(in->data(), len);
  in->remove_prefix(len);
  return true;
}

bool GetElement(base::StringPiece* in,
                unsigned tag_value,
                base::StringPiece* out) {
  unsigned header_len;
  if (!ParseElement(in, tag_value, out, &header_len))
    return false;
  if (out)
    out->remove_prefix(header_len);
  return true;
}

// SeekToSPKI changes |cert| so that it points to a suffix of the
// TBSCertificate where the suffix begins at the start of the ASN.1
// SubjectPublicKeyInfo value.
static bool SeekToSPKI(base::StringPiece* cert) {
  // From RFC 5280, section 4.1
  //    Certificate  ::=  SEQUENCE  {
  //      tbsCertificate       TBSCertificate,
  //      signatureAlgorithm   AlgorithmIdentifier,
  //      signatureValue       BIT STRING  }

  // TBSCertificate  ::=  SEQUENCE  {
  //      version         [0]  EXPLICIT Version DEFAULT v1,
  //      serialNumber         CertificateSerialNumber,
  //      signature            AlgorithmIdentifier,
  //      issuer               Name,
  //      validity             Validity,
  //      subject              Name,
  //      subjectPublicKeyInfo SubjectPublicKeyInfo,

  base::StringPiece certificate;
  if (!GetElement(cert, kSEQUENCE, &certificate))
    return false;

  // We don't allow junk after the certificate.
  if (!cert->empty())
    return false;

  base::StringPiece tbs_certificate;
  if (!GetElement(&certificate, kSEQUENCE, &tbs_certificate))
    return false;

  if (!GetElement(&tbs_certificate,
                  kOptional | kConstructed | kContextSpecific | 0,
                  NULL)) {
    return false;
  }

  // serialNumber
  if (!GetElement(&tbs_certificate, kINTEGER, NULL))
    return false;
  // signature
  if (!GetElement(&tbs_certificate, kSEQUENCE, NULL))
    return false;
  // issuer
  if (!GetElement(&tbs_certificate, kSEQUENCE, NULL))
    return false;
  // validity
  if (!GetElement(&tbs_certificate, kSEQUENCE, NULL))
    return false;
  // subject
  if (!GetElement(&tbs_certificate, kSEQUENCE, NULL))
    return false;
  *cert = tbs_certificate;
  return true;
}

bool ExtractSPKIFromDERCert(base::StringPiece cert,
                            base::StringPiece* spki_out) {
  if (!SeekToSPKI(&cert))
    return false;
  if (!ParseElement(&cert, kSEQUENCE, spki_out, NULL))
    return false;
  return true;
}

bool ExtractSubjectPublicKeyFromSPKI(base::StringPiece spki,
                                     base::StringPiece* spk_out) {
  // From RFC 5280, Section 4.1
  //   SubjectPublicKeyInfo  ::=  SEQUENCE  {
  //     algorithm            AlgorithmIdentifier,
  //     subjectPublicKey     BIT STRING  }
  //
  //   AlgorithmIdentifier  ::=  SEQUENCE  {
  //     algorithm               OBJECT IDENTIFIER,
  //     parameters              ANY DEFINED BY algorithm OPTIONAL  }

  // Step into SubjectPublicKeyInfo sequence.
  base::StringPiece spki_contents;
  if (!asn1::GetElement(&spki, asn1::kSEQUENCE, &spki_contents))
    return false;

  // Step over algorithm field (a SEQUENCE).
  base::StringPiece algorithm;
  if (!asn1::GetElement(&spki_contents, asn1::kSEQUENCE, &algorithm))
    return false;

  // Extract the subjectPublicKey field.
  if (!asn1::GetElement(&spki_contents, asn1::kBITSTRING, spk_out))
    return false;
  return true;
}


bool ExtractCRLURLsFromDERCert(base::StringPiece cert,
                               std::vector<base::StringPiece>* urls_out) {
  urls_out->clear();
  std::vector<base::StringPiece> tmp_urls_out;

  if (!SeekToSPKI(&cert))
    return false;

  // From RFC 5280, section 4.1
  // TBSCertificate  ::=  SEQUENCE  {
  //      ...
  //      subjectPublicKeyInfo SubjectPublicKeyInfo,
  //      issuerUniqueID  [1]  IMPLICIT UniqueIdentifier OPTIONAL,
  //      subjectUniqueID [2]  IMPLICIT UniqueIdentifier OPTIONAL,
  //      extensions      [3]  EXPLICIT Extensions OPTIONAL

  // subjectPublicKeyInfo
  if (!GetElement(&cert, kSEQUENCE, NULL))
    return false;
  // issuerUniqueID
  if (!GetElement(&cert, kOptional | kConstructed | kContextSpecific | 1, NULL))
    return false;
  // subjectUniqueID
  if (!GetElement(&cert, kOptional | kConstructed | kContextSpecific | 2, NULL))
    return false;

  base::StringPiece extensions_seq;
  if (!GetElement(&cert, kOptional | kConstructed | kContextSpecific | 3,
                  &extensions_seq)) {
    return false;
  }

  if (extensions_seq.empty())
    return true;

  // Extensions  ::=  SEQUENCE SIZE (1..MAX) OF Extension
  // Extension   ::=  SEQUENCE  {
  //      extnID      OBJECT IDENTIFIER,
  //      critical    BOOLEAN DEFAULT FALSE,
  //      extnValue   OCTET STRING

  // |extensions_seq| was EXPLICITly tagged, so we still need to remove the
  // ASN.1 SEQUENCE header.
  base::StringPiece extensions;
  if (!GetElement(&extensions_seq, kSEQUENCE, &extensions))
    return false;

  while (extensions.size() > 0) {
    base::StringPiece extension;
    if (!GetElement(&extensions, kSEQUENCE, &extension))
      return false;

    base::StringPiece oid;
    if (!GetElement(&extension, kOID, &oid))
      return false;

    // kCRLDistributionPointsOID is the DER encoding of the OID for the X.509
    // CRL Distribution Points extension.
    static const uint8 kCRLDistributionPointsOID[] = {0x55, 0x1d, 0x1f};

    if (oid.size() != sizeof(kCRLDistributionPointsOID) ||
        memcmp(oid.data(), kCRLDistributionPointsOID, oid.size()) != 0) {
      continue;
    }

    // critical
    GetElement(&extension, kBOOLEAN, NULL);

    // extnValue
    base::StringPiece extension_value;
    if (!GetElement(&extension, kOCTETSTRING, &extension_value))
      return false;

    // RFC 5280, section 4.2.1.13.
    //
    // CRLDistributionPoints ::= SEQUENCE SIZE (1..MAX) OF DistributionPoint
    //
    // DistributionPoint ::= SEQUENCE {
    //  distributionPoint       [0]     DistributionPointName OPTIONAL,
    //  reasons                 [1]     ReasonFlags OPTIONAL,
    //  cRLIssuer               [2]     GeneralNames OPTIONAL }

    base::StringPiece distribution_points;
    if (!GetElement(&extension_value, kSEQUENCE, &distribution_points))
      return false;

    while (distribution_points.size() > 0) {
      base::StringPiece distrib_point;
      if (!GetElement(&distribution_points, kSEQUENCE, &distrib_point))
        return false;

      base::StringPiece name;
      if (!GetElement(&distrib_point, kContextSpecific | kConstructed | 0,
                      &name)) {
        // If it doesn't contain a name then we skip it.
        continue;
      }

      if (GetElement(&distrib_point, kContextSpecific | 1, NULL)) {
        // If it contains a subset of reasons then we skip it. We aren't
        // interested in subsets of CRLs and the RFC states that there MUST be
        // a CRL that covers all reasons.
        continue;
      }

      if (GetElement(&distrib_point,
                     kContextSpecific | kConstructed | 2, NULL)) {
        // If it contains a alternative issuer, then we skip it.
        continue;
      }

      // DistributionPointName ::= CHOICE {
      //   fullName                [0]     GeneralNames,
      //   nameRelativeToCRLIssuer [1]     RelativeDistinguishedName }
      base::StringPiece general_names;
      if (!GetElement(&name,
                      kContextSpecific | kConstructed | 0, &general_names)) {
        continue;
      }

      // GeneralNames ::= SEQUENCE SIZE (1..MAX) OF GeneralName
      // GeneralName ::= CHOICE {
      //   ...
      //   uniformResourceIdentifier [6]  IA5String,
      //   ...
      while (general_names.size() > 0) {
        base::StringPiece url;
        if (GetElement(&general_names, kContextSpecific | 6, &url)) {
          tmp_urls_out.push_back(url);
        } else {
          if (!GetElement(&general_names, kAny, NULL))
            return false;
        }
      }
    }
  }

  urls_out->swap(tmp_urls_out);
  return true;
}

} // namespace asn1

} // namespace net
