// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SECURITY_STYLE_EXPLANATION_H_
#define CONTENT_PUBLIC_BROWSER_SECURITY_STYLE_EXPLANATION_H_

#include <string>

#include "content/common/content_export.h"

namespace content {

// A human-readable summary phrase and more detailed description of a
// security property that was used to compute the SecurityStyle of a
// resource. An example summary phrase would be "Expired Certificate",
// with a description along the lines of "This site's certificate chain
// contains errors (net::CERT_DATE_INVALID)".
struct SecurityStyleExplanation {
  CONTENT_EXPORT SecurityStyleExplanation(){};
  CONTENT_EXPORT SecurityStyleExplanation(const std::string& summary,
                                          const std::string& description)
      : summary(summary), description(description), cert_id(0) {}
  CONTENT_EXPORT SecurityStyleExplanation(const std::string& summary,
                                          const std::string& description,
                                          int cert_id)
      : summary(summary), description(description), cert_id(cert_id) {}
  CONTENT_EXPORT ~SecurityStyleExplanation() {}

  std::string summary;
  std::string description;
  int cert_id;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SECURITY_STYLE_EXPLANATION_H_
