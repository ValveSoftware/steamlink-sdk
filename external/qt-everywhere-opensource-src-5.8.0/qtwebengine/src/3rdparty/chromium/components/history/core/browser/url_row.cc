// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/url_row.h"

#include <algorithm>

namespace history {

URLRow::URLRow() {
  Initialize();
}

URLRow::URLRow(const GURL& url) : url_(url) {
  // Initialize will not set the URL, so our initialization above will stay.
  Initialize();
}

URLRow::URLRow(const GURL& url, URLID id) : url_(url) {
  // Initialize will not set the URL, so our initialization above will stay.
  Initialize();
  // Initialize will zero the id_, so set it here.
  id_ = id;
}

URLRow::URLRow(const URLRow& other) = default;

URLRow::~URLRow() {
}

URLRow& URLRow::operator=(const URLRow& other) {
  if (this == &other)
    return *this;
  id_ = other.id_;
  url_ = other.url_;
  title_ = other.title_;
  visit_count_ = other.visit_count_;
  typed_count_ = other.typed_count_;
  last_visit_ = other.last_visit_;
  hidden_ = other.hidden_;
  return *this;
}

void URLRow::Swap(URLRow* other) {
  std::swap(id_, other->id_);
  url_.Swap(&other->url_);
  title_.swap(other->title_);
  std::swap(visit_count_, other->visit_count_);
  std::swap(typed_count_, other->typed_count_);
  std::swap(last_visit_, other->last_visit_);
  std::swap(hidden_, other->hidden_);
}

void URLRow::Initialize() {
  id_ = 0;
  visit_count_ = 0;
  typed_count_ = 0;
  last_visit_ = base::Time();
  hidden_ = false;
}


URLResult::URLResult()
    : blocked_visit_(false) {
}

URLResult::URLResult(const GURL& url, base::Time visit_time)
    : URLRow(url),
      visit_time_(visit_time),
      blocked_visit_(false) {
}

URLResult::URLResult(const GURL& url,
                     const query_parser::Snippet::MatchPositions& title_matches)
    : URLRow(url) {
  title_match_positions_ = title_matches;
}
URLResult::URLResult(const URLRow& url_row)
    : URLRow(url_row),
      blocked_visit_(false) {
}

URLResult::URLResult(const URLResult& other) = default;

URLResult::~URLResult() {
}

void URLResult::SwapResult(URLResult* other) {
  URLRow::Swap(other);
  std::swap(visit_time_, other->visit_time_);
  snippet_.Swap(&other->snippet_);
  title_match_positions_.swap(other->title_match_positions_);
  std::swap(blocked_visit_, other->blocked_visit_);
}

// static
bool URLResult::CompareVisitTime(const URLResult& lhs, const URLResult& rhs) {
  return lhs.visit_time() > rhs.visit_time();
}

}  // namespace history
