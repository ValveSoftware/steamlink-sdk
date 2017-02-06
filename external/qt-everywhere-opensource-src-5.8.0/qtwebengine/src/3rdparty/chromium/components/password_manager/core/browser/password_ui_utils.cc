// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_ui_utils.h"

#include <algorithm>
#include <string>

#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/affiliation_utils.h"
#include "components/url_formatter/elide_url.h"

namespace password_manager {

namespace {

// The URL prefixes that are removed from shown origin.
const char* const kRemovedPrefixes[] = {"m.", "mobile.", "www."};

}  // namespace

std::string SplitByDotAndReverse(base::StringPiece host) {
  std::vector<std::string> parts =
      base::SplitString(host, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  std::reverse(parts.begin(), parts.end());
  return base::JoinString(parts, ".");
}

std::string StripAndroidAndReverse(const std::string& origin) {
  const int kAndroidAppSchemeAndDelimiterLength = sizeof("android://") - 1;
  return SplitByDotAndReverse(
      base::StringPiece(&origin[kAndroidAppSchemeAndDelimiterLength],
                        origin.length() - kAndroidAppSchemeAndDelimiterLength));
}

std::string GetShownOriginAndLinkUrl(
    const autofill::PasswordForm& password_form,
    bool* is_android_uri,
    GURL* link_url,
    bool* origin_is_clickable) {
  DCHECK(is_android_uri);
  DCHECK(origin_is_clickable);
  DCHECK(link_url);

  password_manager::FacetURI facet_uri =
      password_manager::FacetURI::FromPotentiallyInvalidSpec(
          password_form.signon_realm);
  *is_android_uri = facet_uri.IsValidAndroidFacetURI();
  if (*is_android_uri) {
    if (password_form.affiliated_web_realm.empty()) {
      *origin_is_clickable = false;
      // Since the full url should be shown in the tooltip even for
      // non-clickable origins, return it as |link_url|.
      *link_url = GURL(password_form.signon_realm);
      return GetHumanReadableOriginForAndroidUri(facet_uri);
    }
    *origin_is_clickable = true;
    *link_url = GURL(password_form.affiliated_web_realm);
    return GetShownOrigin(*link_url);
  }
  *origin_is_clickable = true;
  *link_url = password_form.origin;
  return GetShownOrigin(password_form.origin);
}

std::string GetShownOrigin(const GURL& origin) {
  std::string original =
      base::UTF16ToUTF8(url_formatter::FormatUrlForSecurityDisplay(
          origin, url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS));
  base::StringPiece result = original;
  for (base::StringPiece prefix : kRemovedPrefixes) {
    if (base::StartsWith(result, prefix,
                         base::CompareCase::INSENSITIVE_ASCII)) {
      result.remove_prefix(prefix.length());
      break;  // Remove only one prefix (e.g. www.mobile.de).
    }
  }

  return result.find('.') != base::StringPiece::npos ? result.as_string()
                                                     : original;
}

}  // namespace password_manager
