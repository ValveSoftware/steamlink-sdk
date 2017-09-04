// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_URL_PATTERN_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_URL_PATTERN_H_

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "components/subresource_filter/core/common/proto/rules.pb.h"

namespace subresource_filter {

namespace flat {
struct UrlRule;  // The FlatBuffers version of UrlRule.
}

// The structure used to mirror a URL pattern regardless of the representation
// of the UrlRule that owns it.
struct UrlPattern {
  UrlPattern();

  // Creates a |url_pattern| of a certain |type|.
  UrlPattern(base::StringPiece url_pattern,
             proto::UrlPatternType type = proto::URL_PATTERN_TYPE_WILDCARDED);

  // Creates a WILDCARDED |url_pattern| with the specified anchors.
  UrlPattern(base::StringPiece url_pattern,
             proto::AnchorType anchor_left,
             proto::AnchorType anchor_right);

  // The following constructors create UrlPattern from one of the UrlRule
  // representations. The passed in |rule| must outlive the created instance.
  explicit UrlPattern(const flat::UrlRule& rule);
  explicit UrlPattern(const proto::UrlRule& rule);

  ~UrlPattern();

  proto::UrlPatternType type = proto::URL_PATTERN_TYPE_UNSPECIFIED;
  base::StringPiece url_pattern;

  proto::AnchorType anchor_left = proto::ANCHOR_TYPE_NONE;
  proto::AnchorType anchor_right = proto::ANCHOR_TYPE_NONE;

  bool match_case = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(UrlPattern);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_URL_PATTERN_H_
