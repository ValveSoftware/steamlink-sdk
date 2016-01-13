// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GAIA_GAIA_AUTH_UTIL_H_
#define GOOGLE_APIS_GAIA_GAIA_AUTH_UTIL_H_

#include <string>
#include <utility>
#include <vector>

class GURL;

namespace gaia {

// Perform basic canonicalization of |email_address|, taking into account that
// gmail does not consider '.' or caps inside a username to matter.
std::string CanonicalizeEmail(const std::string& email_address);

// Returns the canonical form of the given domain.
std::string CanonicalizeDomain(const std::string& domain);

// Sanitize emails. Currently, it only ensures all emails have a domain by
// adding gmail.com if no domain is present.
std::string SanitizeEmail(const std::string& email_address);

// Returns true if the two specified email addresses are the same.  Both
// addresses are first sanitized and then canoncialized before comparing.
bool AreEmailsSame(const std::string& email1, const std::string& email2);

// Extract the domain part from the canonical form of the given email.
std::string ExtractDomainName(const std::string& email);

bool IsGaiaSignonRealm(const GURL& url);

// Parses JSON data returned by /ListAccounts call, returning a vector of
// email/valid pairs.  An email addresses is considered valid if a passive
// login would succeed (i.e. the user does not need to reauthenticate).
// If there an error parsing the JSON, then false is returned.
bool ParseListAccountsData(
    const std::string& data,
    std::vector<std::pair<std::string, bool> >* accounts);

}  // namespace gaia

#endif  // GOOGLE_APIS_GAIA_GAIA_AUTH_UTIL_H_
