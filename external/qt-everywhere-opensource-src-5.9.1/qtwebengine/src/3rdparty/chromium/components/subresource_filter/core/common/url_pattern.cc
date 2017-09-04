// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/url_pattern.h"

#include "components/subresource_filter/core/common/flat/rules_generated.h"

namespace subresource_filter {

namespace {

proto::UrlPatternType ConvertUrlPatternType(flat::UrlPatternType type) {
  switch (type) {
    case flat::UrlPatternType_SUBSTRING:
      return proto::URL_PATTERN_TYPE_SUBSTRING;
    case flat::UrlPatternType_WILDCARDED:
      return proto::URL_PATTERN_TYPE_WILDCARDED;
    case flat::UrlPatternType_REGEXP:
      return proto::URL_PATTERN_TYPE_REGEXP;
    default:
      return proto::URL_PATTERN_TYPE_UNSPECIFIED;
  }
}

proto::AnchorType ConvertAnchorType(flat::AnchorType type) {
  switch (type) {
    case flat::AnchorType_NONE:
      return proto::ANCHOR_TYPE_NONE;
    case flat::AnchorType_BOUNDARY:
      return proto::ANCHOR_TYPE_BOUNDARY;
    case flat::AnchorType_SUBDOMAIN:
      return proto::ANCHOR_TYPE_SUBDOMAIN;
    default:
      return proto::ANCHOR_TYPE_UNSPECIFIED;
  }
}

base::StringPiece ConvertString(const flatbuffers::String* string) {
  return string ? base::StringPiece(string->data(), string->size())
                : base::StringPiece();
}

}  // namespace

UrlPattern::UrlPattern() = default;

UrlPattern::UrlPattern(base::StringPiece url_pattern,
                       proto::UrlPatternType type)
    : type(type), url_pattern(url_pattern) {}

UrlPattern::UrlPattern(base::StringPiece url_pattern,
                       proto::AnchorType anchor_left,
                       proto::AnchorType anchor_right)
    : type(proto::URL_PATTERN_TYPE_WILDCARDED),
      url_pattern(url_pattern),
      anchor_left(anchor_left),
      anchor_right(anchor_right) {}

UrlPattern::UrlPattern(const flat::UrlRule& rule)
    : type(ConvertUrlPatternType(rule.url_pattern_type())),
      url_pattern(ConvertString(rule.url_pattern())),
      anchor_left(ConvertAnchorType(rule.anchor_left())),
      anchor_right(ConvertAnchorType(rule.anchor_right())),
      match_case(!!(rule.options() & flat::OptionFlag_IS_MATCH_CASE)) {}

UrlPattern::UrlPattern(const proto::UrlRule& rule)
    : type(rule.url_pattern_type()),
      url_pattern(rule.url_pattern()),
      anchor_left(rule.anchor_left()),
      anchor_right(rule.anchor_right()),
      match_case(rule.match_case()) {}

UrlPattern::~UrlPattern() = default;

}  // namespace subresource_filter
