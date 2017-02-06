// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SECURITY_STYLE_EXPLANATIONS_H_
#define CONTENT_PUBLIC_BROWSER_SECURITY_STYLE_EXPLANATIONS_H_

#include <vector>

#include "content/common/content_export.h"
#include "content/public/browser/security_style_explanation.h"
#include "content/public/common/security_style.h"

namespace content {

// SecurityStyleExplanations contains information about why a particular
// SecurityStyle was chosen for a page. This information includes the
// mixed content status of the page and whether the page was loaded over
// a cryptographically secure transport. Additionally,
// SecurityStyleExplanations contains human-readable
// SecurityStyleExplanation objects that the embedder can use to
// describe embedder-specific security policies. Each
// SecurityStyleExplanation is a single security property of a page (for
// example, an expired certificate, a valid certificate, or the presence
// of a deprecated crypto algorithm). A single site may have multiple
// different explanations of "secure", "warning", and "broken" severity
// levels.
struct SecurityStyleExplanations {
  CONTENT_EXPORT SecurityStyleExplanations();
  CONTENT_EXPORT ~SecurityStyleExplanations();

  // True if the page ran insecure content such as scripts.
  bool ran_insecure_content;
  // True if the page displayed insecure content such as images.
  bool displayed_insecure_content;

  // The SecurityStyle assigned to a page that runs or displays insecure
  // content, respectively. These values are used to convey the effect
  // that mixed content has on the overall SecurityStyle of the page;
  // for example, a |displayed_insecure_content_style| value of
  // SECURITY_STYLE_UNAUTHENTICATED indicates that the page's overall
  // SecurityStyle will be downgraded to UNAUTHENTICATED as a result of
  // displaying insecure content.
  SecurityStyle ran_insecure_content_style;
  SecurityStyle displayed_insecure_content_style;

  bool scheme_is_cryptographic;

  // True if PKP was bypassed due to a local trust anchor.
  bool pkp_bypassed;

  std::vector<SecurityStyleExplanation> secure_explanations;
  std::vector<SecurityStyleExplanation> unauthenticated_explanations;
  std::vector<SecurityStyleExplanation> broken_explanations;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SECURITY_STYLE_EXPLANATION_H_
