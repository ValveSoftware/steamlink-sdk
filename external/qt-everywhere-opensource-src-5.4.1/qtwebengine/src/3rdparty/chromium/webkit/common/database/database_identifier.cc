// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/common/database/database_identifier.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "url/url_canon.h"

namespace webkit_database {

// static
std::string GetIdentifierFromOrigin(const GURL& origin) {
  return DatabaseIdentifier::CreateFromOrigin(origin).ToString();
}

// static
GURL GetOriginFromIdentifier(const std::string& identifier) {
  return DatabaseIdentifier::Parse(identifier).ToOrigin();
}

static bool SchemeIsUnique(const std::string& scheme) {
  return scheme == "about" || scheme == "data" || scheme == "javascript";
}

// static
const DatabaseIdentifier DatabaseIdentifier::UniqueFileIdentifier() {
  return DatabaseIdentifier("", "", 0, true, true);
}

// static
DatabaseIdentifier DatabaseIdentifier::CreateFromOrigin(const GURL& origin) {
  if (!origin.is_valid() || origin.is_empty() ||
      !origin.IsStandard() || SchemeIsUnique(origin.scheme()))
    return DatabaseIdentifier();

  if (origin.SchemeIsFile())
    return UniqueFileIdentifier();

  int port = origin.IntPort();
  if (port == url::PORT_INVALID)
    return DatabaseIdentifier();

  // We encode the default port for the specified scheme as 0. GURL
  // canonicalizes this as an unspecified port.
  if (port == url::PORT_UNSPECIFIED)
    port = 0;

  return DatabaseIdentifier(origin.scheme(),
                            origin.host(),
                            port,
                            false /* unique */,
                            false /* file */);
}

// static
DatabaseIdentifier DatabaseIdentifier::Parse(const std::string& identifier) {
  if (!base::IsStringASCII(identifier))
    return DatabaseIdentifier();
  if (identifier.find("..") != std::string::npos)
    return DatabaseIdentifier();
  char forbidden[] = {'\\', '/', ':' ,'\0'};
  if (identifier.find_first_of(forbidden, 0, arraysize(forbidden)) !=
          std::string::npos) {
    return DatabaseIdentifier();
  }

  size_t first_underscore = identifier.find_first_of('_');
  if (first_underscore == std::string::npos || first_underscore == 0)
    return DatabaseIdentifier();

  size_t last_underscore = identifier.find_last_of('_');
  if (last_underscore == std::string::npos ||
      last_underscore == first_underscore ||
      last_underscore == identifier.length() - 1)
    return DatabaseIdentifier();

  std::string scheme(identifier.data(), first_underscore);
  if (scheme == "file")
    return UniqueFileIdentifier();

  // This magical set of schemes is always treated as unique.
  if (SchemeIsUnique(scheme))
    return DatabaseIdentifier();

  base::StringPiece port_str(identifier.begin() + last_underscore + 1,
                             identifier.end());
  int port = 0;
  if (!base::StringToInt(port_str, &port) || port < 0 || port >= 1 << 16)
    return DatabaseIdentifier();

  std::string hostname(identifier.data() + first_underscore + 1,
                       last_underscore - first_underscore - 1);
  GURL url(scheme + "://" + hostname + "/");

  if (!url.IsStandard())
    hostname = "";

  // If a url doesn't parse cleanly or doesn't round trip, reject it.
  if (!url.is_valid() || url.scheme() != scheme || url.host() != hostname)
    return DatabaseIdentifier();

  return DatabaseIdentifier(scheme, hostname, port, false /* unique */, false);
}

DatabaseIdentifier::DatabaseIdentifier()
    : port_(0),
      is_unique_(true),
      is_file_(false) {
}

DatabaseIdentifier::DatabaseIdentifier(const std::string& scheme,
                                       const std::string& hostname,
                                       int port,
                                       bool is_unique,
                                       bool is_file)
    : scheme_(scheme),
      hostname_(StringToLowerASCII(hostname)),
      port_(port),
      is_unique_(is_unique),
      is_file_(is_file) {
}

DatabaseIdentifier::~DatabaseIdentifier() {}

std::string DatabaseIdentifier::ToString() const {
  if (is_file_)
    return "file__0";
  if (is_unique_)
    return "__0";
  return scheme_ + "_" + hostname_ + "_" + base::IntToString(port_);
}

GURL DatabaseIdentifier::ToOrigin() const {
  if (is_file_)
    return GURL("file:///");
  if (is_unique_)
    return GURL();
  if (port_ == 0)
    return GURL(scheme_ + "://" + hostname_);
  return GURL(scheme_ + "://" + hostname_ + ":" + base::IntToString(port_));
}

}  // namespace webkit_database
