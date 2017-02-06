// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_INDEX_H_
#define COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_INDEX_H_

#include <stddef.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/query_parser/query_parser.h"

namespace bookmarks {

class BookmarkClient;
class BookmarkNode;
struct BookmarkMatch;

// BookmarkIndex maintains an index of the titles and URLs of bookmarks for
// quick look up. BookmarkIndex is owned and maintained by BookmarkModel, you
// shouldn't need to interact directly with BookmarkIndex.
//
// BookmarkIndex maintains the index (index_) as a map of sets. The map (type
// Index) maps from a lower case string to the set (type NodeSet) of
// BookmarkNodes that contain that string in their title or URL.
class BookmarkIndex {
 public:
  BookmarkIndex(BookmarkClient* client);
  ~BookmarkIndex();

  // Invoked when a bookmark has been added to the model.
  void Add(const BookmarkNode* node);

  // Invoked when a bookmark has been removed from the model.
  void Remove(const BookmarkNode* node);

  // Returns up to |max_count| of bookmarks containing each term from the text
  // |query| in either the title or the URL.
  void GetBookmarksMatching(const base::string16& query,
                            size_t max_count,
                            query_parser::MatchingAlgorithm matching_algorithm,
                            std::vector<BookmarkMatch>* results);

 private:
  typedef std::vector<const BookmarkNode*> Nodes;
  typedef std::set<const BookmarkNode*> NodeSet;
  typedef std::map<base::string16, NodeSet> Index;

  // Constructs |sorted_nodes| by taking the matches in |matches| and sorting
  // them in decreasing order of typed count (if supported by the client) and
  // deduping them.
  void SortMatches(const NodeSet& matches, Nodes* sorted_nodes) const;

  // Add |node| to |results| if the node matches the query.
  void AddMatchToResults(
      const BookmarkNode* node,
      query_parser::QueryParser* parser,
      const query_parser::QueryNodeStarVector& query_nodes,
      std::vector<BookmarkMatch>* results);

  // Populates |matches| for the specified term. If |first_term| is true, this
  // is the first term in the query. Returns true if there is at least one node
  // matching the term.
  bool GetBookmarksMatchingTerm(
      const base::string16& term,
      bool first_term,
      query_parser::MatchingAlgorithm matching_algorithm,
      NodeSet* matches);

  // Returns the set of query words from |query|.
  std::vector<base::string16> ExtractQueryWords(const base::string16& query);

  // Adds |node| to |index_|.
  void RegisterNode(const base::string16& term, const BookmarkNode* node);

  // Removes |node| from |index_|.
  void UnregisterNode(const base::string16& term, const BookmarkNode* node);

  Index index_;

  BookmarkClient* const client_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkIndex);
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_INDEX_H_
