// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_INDEXED_RULESET_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_INDEXED_RULESET_H_

#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_piece.h"
#include "components/subresource_filter/core/common/closed_hash_map.h"
#include "components/subresource_filter/core/common/flat/rules_generated.h"
#include "components/subresource_filter/core/common/proto/rules.pb.h"
#include "components/subresource_filter/core/common/uint64_hasher.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace subresource_filter {

class FirstPartyOrigin;

// The integer type used to represent N-grams.
using NGram = uint64_t;
// The hasher used for hashing N-grams.
using NGramHasher = Uint64Hasher;
// The hash table probe sequence used both by the ruleset builder and matcher.
using NGramHashTableProber = DefaultProber<NGram, NGramHasher>;

constexpr size_t kNGramSize = 5;
static_assert(kNGramSize <= sizeof(NGram), "NGram type is too narrow.");

// The class used to construct flat data structures representing the set of URL
// filtering rules, as well as the index of those. Internally owns a
// FlatBufferBuilder storing the structures.
class RulesetIndexer {
 public:
  // The current binary format version of the indexed ruleset.
  static const int kIndexedFormatVersion;

  RulesetIndexer();
  ~RulesetIndexer();

  // Adds |rule| to the ruleset and the index unless the |rule| has unsupported
  // filter options, in which case the data structures remain unmodified.
  // Returns whether the |rule| has been serialized and added to the index.
  bool AddUrlRule(const proto::UrlRule& rule);

  // Finalizes construction of the data structures.
  void Finish();

  // Returns a pointer to the buffer containing the serialized flat data
  // structures. Should only be called after Finish().
  const uint8_t* data() const { return builder_.GetBufferPointer(); }

  // Returns the size of the buffer.
  size_t size() const { return base::strict_cast<size_t>(builder_.GetSize()); }

 private:
  using MutableUrlRuleList = std::vector<flatbuffers::Offset<flat::UrlRule>>;
  using MutableNGramIndex =
      ClosedHashMap<NGram, MutableUrlRuleList, NGramHashTableProber>;

  // Encapsulates a subset of the rules, and an index built on the URL patterns
  // in these rules. The ruleset is divided into parts according to metadata of
  // the rules. Currently there are two parts: blacklist and whitelist.
  struct MutableUrlPatternIndex {
    // This index contains all non-REGEXP rules that have at least one
    // acceptable N-gram. For a single rule the N-gram used as an index key is
    // picked greedily (see GetMostDistinctiveNGram).
    MutableNGramIndex ngram_index;

    // A fallback list that contains all the rules with no acceptable N-gram,
    // and all the REGEXP rules.
    MutableUrlRuleList fallback_rules;

    MutableUrlPatternIndex();
    ~MutableUrlPatternIndex();
  };

  // Returns an N-gram of the |pattern| encoded into the NGram integer type. The
  // N-gram is picked using a greedy heuristic, i.e. the one is chosen which
  // corresponds to the shortest list of rules within the |index|. If there are
  // no valid N-grams in the |pattern|, the return value is 0.
  static NGram GetMostDistinctiveNGram(const MutableNGramIndex& index,
                                       base::StringPiece pattern);

  // Serialized an |index| built over a part of the ruleset, and returns its
  // offset in the FlatBuffer.
  flatbuffers::Offset<flat::UrlPatternIndex> SerializeUrlPatternIndex(
      const MutableUrlPatternIndex& index);

  MutableUrlPatternIndex blacklist_;
  MutableUrlPatternIndex whitelist_;

  flatbuffers::FlatBufferBuilder builder_;

  DISALLOW_COPY_AND_ASSIGN(RulesetIndexer);
};

// Matches URLs against the FlatBuffer representation of an indexed ruleset.
class IndexedRulesetMatcher {
 public:
  // Returns whether the |buffer| of the given |size| contains a valid
  // flat::IndexedRuleset FlatBuffer.
  static bool Verify(const uint8_t* buffer, size_t size);

  // Creates an instance that matches URLs against the flat::IndexedRuleset
  // provided as the root object of serialized data in the |buffer| of the given
  // |size|.
  IndexedRulesetMatcher(const uint8_t* buffer, size_t size);

  // Returns whether the subset of subresource filtering rules specified by the
  // |activation_type| should be disabled for the |document| loaded from
  // |parent_document_origin|. Always returns false if |activation_type| ==
  // ACTIVATION_TYPE_UNSPECIFIED or the |document_url| is not valid. Unlike
  // page-level activation, such rules can be used to have fine-grained control
  // over the activation of filtering within (sub-)documents.
  bool ShouldDisableFilteringForDocument(
      const GURL& document_url,
      const url::Origin& parent_document_origin,
      proto::ActivationType activation_type) const;

  // Returns whether the network request to |url| of |element_type| initiated by
  // |document_origin| is not allowed to proceed. Always returns false if the
  // |url| is not valid or |element_type| == ELEMENT_TYPE_UNSPECIFIED.
  bool ShouldDisallowResourceLoad(const GURL& url,
                                  const FirstPartyOrigin& first_party,
                                  proto::ElementType element_type,
                                  bool disable_generic_rules) const;

 private:
  const flat::IndexedRuleset* root_;

  DISALLOW_COPY_AND_ASSIGN(IndexedRulesetMatcher);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_COMMON_INDEXED_RULESET_H_
