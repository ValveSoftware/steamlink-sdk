// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_index.h"

#include <stdint.h>

#include <algorithm>
#include <functional>
#include <iterator>
#include <list>

#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/utf_offset_string_conversions.h"
#include "build/build_config.h"
#include "components/bookmarks/browser/bookmark_client.h"
#include "components/bookmarks/browser/bookmark_match.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/query_parser/snippet.h"
#include "third_party/icu/source/common/unicode/normalizer2.h"
#include "third_party/icu/source/common/unicode/utypes.h"

namespace bookmarks {

typedef BookmarkClient::NodeTypedCountPair NodeTypedCountPair;
typedef BookmarkClient::NodeTypedCountPairs NodeTypedCountPairs;

namespace {

// Returns a normalized version of the UTF16 string |text|.  If it fails to
// normalize the string, returns |text| itself as a best-effort.
base::string16 Normalize(const base::string16& text) {
  UErrorCode status = U_ZERO_ERROR;
  const icu::Normalizer2* normalizer2 =
      icu::Normalizer2::getInstance(nullptr, "nfkc", UNORM2_COMPOSE, status);
  if (U_FAILURE(status)) {
    // Log and crash right away to capture the error code in the crash report.
    LOG(FATAL) << "failed to create a normalizer: " << u_errorName(status);
  }
  icu::UnicodeString unicode_text(
      text.data(), static_cast<int32_t>(text.length()));
  icu::UnicodeString unicode_normalized_text;
  normalizer2->normalize(unicode_text, unicode_normalized_text, status);
  if (U_FAILURE(status)) {
    // This should not happen. Log the error and fall back.
    LOG(ERROR) << "normalization failed: " << u_errorName(status);
    return text;
  }
  return base::string16(unicode_normalized_text.getBuffer(),
                        unicode_normalized_text.length());
}

// Sort functor for NodeTypedCountPairs. We sort in decreasing order of typed
// count so that the best matches will always be added to the results.
struct NodeTypedCountPairSortFunctor {
  bool operator()(const NodeTypedCountPair& a,
                  const NodeTypedCountPair& b) const {
    return a.second > b.second;
  }
};

// Extract the const Node* stored in a BookmarkClient::NodeTypedCountPair.
struct NodeTypedCountPairExtractNodeFunctor {
  const BookmarkNode* operator()(const NodeTypedCountPair& pair) const {
    return pair.first;
  }
};

}  // namespace

BookmarkIndex::BookmarkIndex(BookmarkClient* client)
    : client_(client) {
  DCHECK(client_);
}

BookmarkIndex::~BookmarkIndex() {
}

void BookmarkIndex::Add(const BookmarkNode* node) {
  if (!node->is_url())
    return;
  std::vector<base::string16> terms =
      ExtractQueryWords(Normalize(node->GetTitle()));
  for (size_t i = 0; i < terms.size(); ++i)
    RegisterNode(terms[i], node);
  terms =
      ExtractQueryWords(CleanUpUrlForMatching(node->url(), nullptr));
  for (size_t i = 0; i < terms.size(); ++i)
    RegisterNode(terms[i], node);
}

void BookmarkIndex::Remove(const BookmarkNode* node) {
  if (!node->is_url())
    return;

  std::vector<base::string16> terms =
      ExtractQueryWords(Normalize(node->GetTitle()));
  for (size_t i = 0; i < terms.size(); ++i)
    UnregisterNode(terms[i], node);
  terms =
      ExtractQueryWords(CleanUpUrlForMatching(node->url(), nullptr));
  for (size_t i = 0; i < terms.size(); ++i)
    UnregisterNode(terms[i], node);
}

void BookmarkIndex::GetBookmarksMatching(
    const base::string16& input_query,
    size_t max_count,
    query_parser::MatchingAlgorithm matching_algorithm,
    std::vector<BookmarkMatch>* results) {
  const base::string16 query = Normalize(input_query);
  std::vector<base::string16> terms = ExtractQueryWords(query);
  if (terms.empty())
    return;

  NodeSet matches;
  for (size_t i = 0; i < terms.size(); ++i) {
    if (!GetBookmarksMatchingTerm(
            terms[i], i == 0, matching_algorithm, &matches)) {
      return;
    }
  }

  Nodes sorted_nodes;
  SortMatches(matches, &sorted_nodes);

  // We use a QueryParser to fill in match positions for us. It's not the most
  // efficient way to go about this, but by the time we get here we know what
  // matches and so this shouldn't be performance critical.
  query_parser::QueryParser parser;
  ScopedVector<query_parser::QueryNode> query_nodes;
  parser.ParseQueryNodes(query, matching_algorithm, &query_nodes.get());

  // The highest typed counts should be at the beginning of the results vector
  // so that the best matches will always be included in the results. The loop
  // that calculates result relevance in HistoryContentsProvider::ConvertResults
  // will run backwards to assure higher relevance will be attributed to the
  // best matches.
  for (Nodes::const_iterator i = sorted_nodes.begin();
       i != sorted_nodes.end() && results->size() < max_count;
       ++i)
    AddMatchToResults(*i, &parser, query_nodes.get(), results);
}

void BookmarkIndex::SortMatches(const NodeSet& matches,
                                Nodes* sorted_nodes) const {
  sorted_nodes->reserve(matches.size());
  if (client_->SupportsTypedCountForNodes()) {
    NodeTypedCountPairs node_typed_counts;
    client_->GetTypedCountForNodes(matches, &node_typed_counts);
    std::sort(node_typed_counts.begin(),
              node_typed_counts.end(),
              NodeTypedCountPairSortFunctor());
    std::transform(node_typed_counts.begin(),
                   node_typed_counts.end(),
                   std::back_inserter(*sorted_nodes),
                   NodeTypedCountPairExtractNodeFunctor());
  } else {
    sorted_nodes->insert(sorted_nodes->end(), matches.begin(), matches.end());
  }
}

void BookmarkIndex::AddMatchToResults(
    const BookmarkNode* node,
    query_parser::QueryParser* parser,
    const query_parser::QueryNodeStarVector& query_nodes,
    std::vector<BookmarkMatch>* results) {
  // Check that the result matches the query.  The previous search
  // was a simple per-word search, while the more complex matching
  // of QueryParser may filter it out.  For example, the query
  // ["thi"] will match the bookmark titled [Thinking], but since
  // ["thi"] is quoted we don't want to do a prefix match.
  query_parser::QueryWordVector title_words, url_words;
  const base::string16 lower_title =
      base::i18n::ToLower(Normalize(node->GetTitle()));
  parser->ExtractQueryWords(lower_title, &title_words);
  base::OffsetAdjuster::Adjustments adjustments;
  parser->ExtractQueryWords(
      CleanUpUrlForMatching(node->url(), &adjustments),
      &url_words);
  query_parser::Snippet::MatchPositions title_matches, url_matches;
  for (size_t i = 0; i < query_nodes.size(); ++i) {
    const bool has_title_matches =
        query_nodes[i]->HasMatchIn(title_words, &title_matches);
    const bool has_url_matches =
        query_nodes[i]->HasMatchIn(url_words, &url_matches);
    if (!has_title_matches && !has_url_matches)
      return;
    query_parser::QueryParser::SortAndCoalesceMatchPositions(&title_matches);
    query_parser::QueryParser::SortAndCoalesceMatchPositions(&url_matches);
  }
  BookmarkMatch match;
  if (lower_title.length() == node->GetTitle().length()) {
    // Only use title matches if the lowercase string is the same length
    // as the original string, otherwise the matches are meaningless.
    // TODO(mpearson): revise match positions appropriately.
    match.title_match_positions.swap(title_matches);
  }
  // Now that we're done processing this entry, correct the offsets of the
  // matches in |url_matches| so they point to offsets in the original URL
  // spec, not the cleaned-up URL string that we used for matching.
  std::vector<size_t> offsets =
      BookmarkMatch::OffsetsFromMatchPositions(url_matches);
  base::OffsetAdjuster::UnadjustOffsets(adjustments, &offsets);
  url_matches =
      BookmarkMatch::ReplaceOffsetsInMatchPositions(url_matches, offsets);
  match.url_match_positions.swap(url_matches);
  match.node = node;
  results->push_back(match);
}

bool BookmarkIndex::GetBookmarksMatchingTerm(
    const base::string16& term,
    bool first_term,
    query_parser::MatchingAlgorithm matching_algorithm,
    NodeSet* matches) {
  Index::const_iterator i = index_.lower_bound(term);
  if (i == index_.end())
    return false;

  if (!query_parser::QueryParser::IsWordLongEnoughForPrefixSearch(
      term, matching_algorithm)) {
    // Term is too short for prefix match, compare using exact match.
    if (i->first != term)
      return false;  // No bookmarks with this term.

    if (first_term) {
      (*matches) = i->second;
      return true;
    }
    *matches = base::STLSetIntersection<NodeSet>(i->second, *matches);
  } else {
    // Loop through index adding all entries that start with term to
    // |prefix_matches|.
    NodeSet tmp_prefix_matches;
    // If this is the first term, then store the result directly in |matches|
    // to avoid calling stl intersection (which requires a copy).
    NodeSet* prefix_matches = first_term ? matches : &tmp_prefix_matches;
    while (i != index_.end() &&
           i->first.size() >= term.size() &&
           term.compare(0, term.size(), i->first, 0, term.size()) == 0) {
#if !defined(OS_ANDROID)
      prefix_matches->insert(i->second.begin(), i->second.end());
#else
      // Work around a bug in the implementation of std::set::insert in the STL
      // used on android (http://crbug.com/367050).
      for (NodeSet::const_iterator n = i->second.begin(); n != i->second.end();
           ++n)
        prefix_matches->insert(prefix_matches->end(), *n);
#endif
      ++i;
    }
    if (!first_term)
      *matches = base::STLSetIntersection<NodeSet>(*prefix_matches, *matches);
  }
  return !matches->empty();
}

std::vector<base::string16> BookmarkIndex::ExtractQueryWords(
    const base::string16& query) {
  std::vector<base::string16> terms;
  if (query.empty())
    return std::vector<base::string16>();
  query_parser::QueryParser parser;
  parser.ParseQueryWords(base::i18n::ToLower(query),
                         query_parser::MatchingAlgorithm::DEFAULT,
                         &terms);
  return terms;
}

void BookmarkIndex::RegisterNode(const base::string16& term,
                                 const BookmarkNode* node) {
  index_[term].insert(node);
}

void BookmarkIndex::UnregisterNode(const base::string16& term,
                                   const BookmarkNode* node) {
  Index::iterator i = index_.find(term);
  if (i == index_.end()) {
    // We can get here if the node has the same term more than once. For
    // example, a bookmark with the title 'foo foo' would end up here.
    return;
  }
  i->second.erase(node);
  if (i->second.empty())
    index_.erase(i);
}

}  // namespace bookmarks
