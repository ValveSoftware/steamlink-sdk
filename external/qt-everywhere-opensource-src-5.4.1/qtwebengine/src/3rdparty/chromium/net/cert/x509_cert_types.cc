// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/x509_cert_types.h"

#include <cstdlib>
#include <cstring>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "net/cert/x509_certificate.h"

namespace net {

namespace {

// Helper for ParseCertificateDate. |*field| must contain at least
// |field_len| characters. |*field| will be advanced by |field_len| on exit.
// |*ok| is set to false if there is an error in parsing the number, but left
// untouched otherwise. Returns the parsed integer.
int ParseIntAndAdvance(const char** field, size_t field_len, bool* ok) {
  int result = 0;
  *ok &= base::StringToInt(base::StringPiece(*field, field_len), &result);
  *field += field_len;
  return result;
}

}

CertPrincipal::CertPrincipal() {
}

CertPrincipal::CertPrincipal(const std::string& name) : common_name(name) {}

CertPrincipal::~CertPrincipal() {
}

std::string CertPrincipal::GetDisplayName() const {
  if (!common_name.empty())
    return common_name;
  if (!organization_names.empty())
    return organization_names[0];
  if (!organization_unit_names.empty())
    return organization_unit_names[0];

  return std::string();
}

CertPolicy::CertPolicy() {
}

CertPolicy::~CertPolicy() {
}

// For a denial, we consider a given |cert| to be a match to a saved denied
// cert if the |error| intersects with the saved error status. For an
// allowance, we consider a given |cert| to be a match to a saved allowed
// cert if the |error| is an exact match to or subset of the errors in the
// saved CertStatus.
CertPolicy::Judgment CertPolicy::Check(
    X509Certificate* cert, CertStatus error) const {
  // It shouldn't matter which set we check first, but we check denied first
  // in case something strange has happened.
  bool denied = false;
  std::map<SHA1HashValue, CertStatus, SHA1HashValueLessThan>::const_iterator
      denied_iter = denied_.find(cert->fingerprint());
  if ((denied_iter != denied_.end()) && (denied_iter->second & error))
    denied = true;

  std::map<SHA1HashValue, CertStatus, SHA1HashValueLessThan>::const_iterator
      allowed_iter = allowed_.find(cert->fingerprint());
  if ((allowed_iter != allowed_.end()) &&
      (allowed_iter->second & error) &&
      !(~(allowed_iter->second & error) ^ ~error)) {
    DCHECK(!denied);
    return ALLOWED;
  }

  if (denied)
    return DENIED;
  return UNKNOWN;  // We don't have a policy for this cert.
}

void CertPolicy::Allow(X509Certificate* cert, CertStatus error) {
  // Put the cert in the allowed set and (maybe) remove it from the denied set.
  denied_.erase(cert->fingerprint());
  // If this same cert had already been saved with a different error status,
  // this will replace it with the new error status.
  allowed_[cert->fingerprint()] = error;
}

void CertPolicy::Deny(X509Certificate* cert, CertStatus error) {
  // Put the cert in the denied set and (maybe) remove it from the allowed set.
  std::map<SHA1HashValue, CertStatus, SHA1HashValueLessThan>::const_iterator
      allowed_iter = allowed_.find(cert->fingerprint());
  if ((allowed_iter != allowed_.end()) && (allowed_iter->second & error))
    allowed_.erase(cert->fingerprint());
  denied_[cert->fingerprint()] |= error;
}

bool CertPolicy::HasAllowedCert() const {
  return !allowed_.empty();
}

bool CertPolicy::HasDeniedCert() const {
  return !denied_.empty();
}

bool ParseCertificateDate(const base::StringPiece& raw_date,
                          CertDateFormat format,
                          base::Time* time) {
  size_t year_length = format == CERT_DATE_FORMAT_UTC_TIME ? 2 : 4;

  if (raw_date.length() < 11 + year_length)
    return false;

  const char* field = raw_date.data();
  bool valid = true;
  base::Time::Exploded exploded = {0};

  exploded.year =         ParseIntAndAdvance(&field, year_length, &valid);
  exploded.month =        ParseIntAndAdvance(&field, 2, &valid);
  exploded.day_of_month = ParseIntAndAdvance(&field, 2, &valid);
  exploded.hour =         ParseIntAndAdvance(&field, 2, &valid);
  exploded.minute =       ParseIntAndAdvance(&field, 2, &valid);
  exploded.second =       ParseIntAndAdvance(&field, 2, &valid);
  if (valid && year_length == 2)
    exploded.year += exploded.year < 50 ? 2000 : 1900;

  valid &= exploded.HasValidValues();

  if (!valid)
    return false;

  *time = base::Time::FromUTCExploded(exploded);
  return true;
}

}  // namespace net
