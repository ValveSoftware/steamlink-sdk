// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/psl_matching_helper.h"

#include <memory>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "components/autofill/core/common/password_form.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

using autofill::PasswordForm;

namespace password_manager {

bool ShouldPSLDomainMatchingApply(
    const std::string& registry_controlled_domain) {
  return !registry_controlled_domain.empty() &&
         registry_controlled_domain != "google.com";
}

bool IsPublicSuffixDomainMatch(const std::string& url1,
                               const std::string& url2) {
  GURL gurl1(url1);
  GURL gurl2(url2);

  if (!gurl1.is_valid() || !gurl2.is_valid())
    return false;

  if (gurl1 == gurl2)
    return true;

  std::string domain1(GetRegistryControlledDomain(gurl1));
  std::string domain2(GetRegistryControlledDomain(gurl2));

  if (domain1.empty() || domain2.empty())
    return false;

  return gurl1.scheme() == gurl2.scheme() && domain1 == domain2 &&
         gurl1.port() == gurl2.port();
}

std::string GetRegistryControlledDomain(const GURL& signon_realm) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      signon_realm,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

bool IsFederatedMatch(const std::string& signon_realm, const GURL& origin) {
  // The format should be "federation://origin.host/federation.host;
  std::string federated_realm = "federation://" + origin.host() + "/";
  return signon_realm.size() > federated_realm.size() &&
         base::StartsWith(signon_realm, federated_realm,
                          base::CompareCase::INSENSITIVE_ASCII);
}

}  // namespace password_manager
