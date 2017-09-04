// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/indexed_ruleset.h"

#include <algorithm>
#include <limits>
#include <string>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "components/subresource_filter/core/common/first_party_origin.h"
#include "components/subresource_filter/core/common/ngram_extractor.h"
#include "components/subresource_filter/core/common/url_pattern.h"
#include "components/subresource_filter/core/common/url_pattern_matching.h"
#include "third_party/flatbuffers/src/include/flatbuffers/flatbuffers.h"

namespace subresource_filter {

namespace {

using FlatStringOffset = flatbuffers::Offset<flatbuffers::String>;

// Checks whether a URL |rule| can be converted to its FlatBuffers equivalent,
// and performs the actual conversion.
class UrlRuleFlatBufferConverter {
 public:
  // Creates the converter, and initializes |is_convertible| bit. If
  // |is_convertible| == true, then all the fields, needed for serializing the
  // |rule| to FlatBuffer, are initialized (|options|, |anchor_right|, etc.).
  UrlRuleFlatBufferConverter(const proto::UrlRule& rule) : rule_(rule) {
    is_convertible_ = InitializeOptions() && InitializeElementTypes() &&
                      InitializeActivationTypes() && InitializeUrlPattern();
  }

  // Returns whether the |rule| can be converted to its FlatBuffers equivalent.
  // The conversion is not possible if the rule has attributes not supported by
  // this client version.
  bool is_convertible() const { return is_convertible_; }

  // Writes the URL |rule| to the FlatBuffer using the |builder|, and returns
  // the offset to the serialized rule.
  flatbuffers::Offset<flat::UrlRule> SerializeConvertedRule(
      flatbuffers::FlatBufferBuilder* builder) const {
    DCHECK(is_convertible());

    flatbuffers::Offset<flatbuffers::Vector<FlatStringOffset>> domains_offset;
    if (rule_.domains_size()) {
      std::vector<FlatStringOffset> domains;
      domains.reserve(rule_.domains_size());

      std::string domain;
      for (const auto& domain_list_item : rule_.domains()) {
        domain.clear();
        domain.reserve(domain_list_item.domain().size() + 1);
        if (domain_list_item.exclude())
          domain += '~';
        domain += domain_list_item.domain();
        domains.push_back(builder->CreateString(domain));
      }
      domains_offset = builder->CreateVector(domains);
    }

    auto url_pattern_offset = builder->CreateString(rule_.url_pattern());

    std::vector<uint8_t> failure_function;
    BuildFailureFunction(UrlPattern(rule_), &failure_function);
    auto failure_function_offset =
        builder->CreateVector(failure_function.data(), failure_function.size());

    return flat::CreateUrlRule(*builder, options_, element_types_,
                               activation_types_, url_pattern_type_,
                               anchor_left_, anchor_right_, domains_offset,
                               url_pattern_offset, failure_function_offset);
  }

 private:
  static bool ConvertAnchorType(proto::AnchorType anchor_type,
                                flat::AnchorType* result) {
    switch (anchor_type) {
      case proto::ANCHOR_TYPE_NONE:
        *result = flat::AnchorType_NONE;
        break;
      case proto::ANCHOR_TYPE_BOUNDARY:
        *result = flat::AnchorType_BOUNDARY;
        break;
      case proto::ANCHOR_TYPE_SUBDOMAIN:
        *result = flat::AnchorType_SUBDOMAIN;
        break;
      default:
        return false;  // Unsupported anchor type.
    }
    return true;
  }

  bool InitializeOptions() {
    if (rule_.semantics() == proto::RULE_SEMANTICS_WHITELIST) {
      options_ |= flat::OptionFlag_IS_WHITELIST;
    } else if (rule_.semantics() != proto::RULE_SEMANTICS_BLACKLIST) {
      return false;  // Unsupported semantics.
    }

    switch (rule_.source_type()) {
      case proto::SOURCE_TYPE_ANY:
        options_ |= flat::OptionFlag_APPLIES_TO_THIRD_PARTY;
      // Note: fall through here intentionally.
      case proto::SOURCE_TYPE_FIRST_PARTY:
        options_ |= flat::OptionFlag_APPLIES_TO_FIRST_PARTY;
        break;
      case proto::SOURCE_TYPE_THIRD_PARTY:
        options_ |= flat::OptionFlag_APPLIES_TO_THIRD_PARTY;
        break;

      default:
        return false;  // Unsupported source type.
    }

    if (rule_.match_case())
      options_ |= flat::OptionFlag_IS_MATCH_CASE;

    return true;
  }

  bool InitializeElementTypes() {
    static_assert(
        proto::ELEMENT_TYPE_ALL <= std::numeric_limits<uint16_t>::max(),
        "Element types can not be stored in uint16_t.");
    if ((rule_.element_types() & proto::ELEMENT_TYPE_ALL) !=
        rule_.element_types()) {
      return false;  // Unsupported element types.
    }
    element_types_ = static_cast<uint16_t>(rule_.element_types());
    // Note: Normally we can not distinguish between the main plugin resource
    // and any other loads it makes. We treat them both as OBJECT requests.
    if (element_types_ & proto::ELEMENT_TYPE_OBJECT_SUBREQUEST)
      element_types_ |= proto::ELEMENT_TYPE_OBJECT;
    return true;
  }

  bool InitializeActivationTypes() {
    static_assert(
        proto::ACTIVATION_TYPE_ALL <= std::numeric_limits<uint8_t>::max(),
        "Activation types can not be stored in uint8_t.");
    if ((rule_.activation_types() & proto::ACTIVATION_TYPE_ALL) !=
        rule_.activation_types()) {
      return false;  // Unsupported activation types.
    }
    activation_types_ = static_cast<uint8_t>(rule_.activation_types());
    return true;
  }

  bool InitializeUrlPattern() {
    if (rule_.url_pattern().size() >
        static_cast<size_t>(std::numeric_limits<uint8_t>::max())) {
      // Failure function can not always be stored as an array of uint8_t in
      // case the pattern's length exceeds 255.
      return false;
    }

    switch (rule_.url_pattern_type()) {
      case proto::URL_PATTERN_TYPE_SUBSTRING:
        url_pattern_type_ = flat::UrlPatternType_SUBSTRING;
        break;
      case proto::URL_PATTERN_TYPE_WILDCARDED:
        url_pattern_type_ = flat::UrlPatternType_WILDCARDED;
        break;
      case proto::URL_PATTERN_TYPE_REGEXP:
        url_pattern_type_ = flat::UrlPatternType_REGEXP;
        break;

      default:
        return false;  // Unsupported URL pattern type.
    }

    if (!ConvertAnchorType(rule_.anchor_left(), &anchor_left_) ||
        !ConvertAnchorType(rule_.anchor_right(), &anchor_right_)) {
      return false;
    }
    if (anchor_right_ == flat::AnchorType_SUBDOMAIN)
      return false;  // Unsupported right anchor.

    return true;
  }

  const proto::UrlRule& rule_;

  uint8_t options_ = 0;
  uint16_t element_types_ = 0;
  uint8_t activation_types_ = 0;
  flat::UrlPatternType url_pattern_type_ = flat::UrlPatternType_WILDCARDED;
  flat::AnchorType anchor_left_ = flat::AnchorType_NONE;
  flat::AnchorType anchor_right_ = flat::AnchorType_NONE;

  bool is_convertible_ = true;
};

}  // namespace

// RulesetIndexer --------------------------------------------------------------

// static
const int RulesetIndexer::kIndexedFormatVersion = 11;

RulesetIndexer::MutableUrlPatternIndex::MutableUrlPatternIndex() = default;
RulesetIndexer::MutableUrlPatternIndex::~MutableUrlPatternIndex() = default;

RulesetIndexer::RulesetIndexer() = default;
RulesetIndexer::~RulesetIndexer() = default;

bool RulesetIndexer::AddUrlRule(const proto::UrlRule& rule) {
  UrlRuleFlatBufferConverter converter(rule);
  if (!converter.is_convertible())
    return false;
  auto rule_offset = converter.SerializeConvertedRule(&builder_);

  MutableUrlPatternIndex* index_part =
      (rule.semantics() == proto::RULE_SEMANTICS_BLACKLIST ? &blacklist_
                                                           : &whitelist_);

  NGram ngram = 0;
  if (rule.url_pattern_type() != proto::URL_PATTERN_TYPE_REGEXP) {
    ngram =
        GetMostDistinctiveNGram(index_part->ngram_index, rule.url_pattern());
  }

  if (ngram) {
    index_part->ngram_index[ngram].push_back(rule_offset);
  } else {
    // TODO(pkalinnikov): Index fallback rules as well.
    index_part->fallback_rules.push_back(rule_offset);
  }

  return true;
}

void RulesetIndexer::Finish() {
  auto blacklist_offset = SerializeUrlPatternIndex(blacklist_);
  auto whitelist_offset = SerializeUrlPatternIndex(whitelist_);

  auto url_rules_index_offset =
      flat::CreateIndexedRuleset(builder_, blacklist_offset, whitelist_offset);
  builder_.Finish(url_rules_index_offset);
}

NGram RulesetIndexer::GetMostDistinctiveNGram(
    const MutableNGramIndex& ngram_index,
    base::StringPiece pattern) {
  size_t min_list_size = std::numeric_limits<size_t>::max();
  NGram best_ngram = 0;

  auto ngrams = CreateNGramExtractor<kNGramSize, NGram>(
      pattern, [](char c) { return c == '*' || c == '^'; });

  for (uint64_t ngram : ngrams) {
    const MutableUrlRuleList* rules = ngram_index.Get(ngram);
    const size_t list_size = rules ? rules->size() : 0;
    if (list_size < min_list_size) {
      // TODO(pkalinnikov): Pick random of the same-sized lists.
      min_list_size = list_size;
      best_ngram = ngram;
      if (list_size == 0)
        break;
    }
  }

  return best_ngram;
}

flatbuffers::Offset<flat::UrlPatternIndex>
RulesetIndexer::SerializeUrlPatternIndex(const MutableUrlPatternIndex& index) {
  const MutableNGramIndex& ngram_index = index.ngram_index;

  std::vector<flatbuffers::Offset<flat::NGramToRules>> flat_hash_table(
      ngram_index.table_size());

  flatbuffers::Offset<flat::NGramToRules> empty_slot_offset =
      flat::CreateNGramToRules(builder_);
  for (size_t i = 0, size = ngram_index.table_size(); i != size; ++i) {
    const uint32_t entry_index = ngram_index.hash_table()[i];
    if (entry_index >= ngram_index.size()) {
      flat_hash_table[i] = empty_slot_offset;
      continue;
    }
    const MutableNGramIndex::EntryType& entry =
        ngram_index.entries()[entry_index];
    auto rules_offset = builder_.CreateVector(entry.second);
    flat_hash_table[i] =
        flat::CreateNGramToRules(builder_, entry.first, rules_offset);
  }
  auto ngram_index_offset = builder_.CreateVector(flat_hash_table);

  auto fallback_rules_offset = builder_.CreateVector(index.fallback_rules);

  return flat::CreateUrlPatternIndex(builder_, kNGramSize, ngram_index_offset,
                                     empty_slot_offset, fallback_rules_offset);
}

// IndexedRulesetMatcher -------------------------------------------------------

namespace {

using FlatUrlRuleList = flatbuffers::Vector<flatbuffers::Offset<flat::UrlRule>>;
using FlatNGramIndex =
    flatbuffers::Vector<flatbuffers::Offset<flat::NGramToRules>>;

// Returns whether the |origin| matches the list of |domains|. A match means
// that the longest domain in |domains| that |origin| is a sub-domain of is not
// an exception OR all the |domains| are exceptions and neither matches the
// |origin|. Thus, domain filters with more domain components trump filters with
// fewer domain components, i.e. the more specific a filter is, the higher the
// priority.
//
// A rule whose domain list is empty or contains only negative domains is still
// considered a "generic" rule. Therefore, if |disable_generic_rules| is set,
// this function will always return false for such |domains| lists.
//
// TODO(pkalinnikov): Make it fast.
bool DoesInitiatorMatchDomainList(
    const url::Origin& initiator,
    const flatbuffers::Vector<FlatStringOffset>& domains,
    bool disable_generic_rules) {
  if (!domains.size())
    return !disable_generic_rules;
  // Unique |initiator| matches lists of exception domains only.
  if (initiator.unique()) {
    for (const flatbuffers::String* domain_filter : domains) {
      DCHECK_GT(domain_filter->size(), 0u);
      if (domain_filter->Get(0) != '~')
        return false;
    }
    return !disable_generic_rules;
  }

  size_t max_domain_length = 0;
  bool is_positive = true;
  bool negatives_only = true;

  for (flatbuffers::uoffset_t i = 0, size = domains.size(); i != size; ++i) {
    const flatbuffers::String* domain_filter = domains.Get(i);
    if (domain_filter->Length() <= max_domain_length)
      continue;

    base::StringPiece filter_piece(domain_filter->c_str(),
                                   domain_filter->Length());
    const bool is_negative = (domain_filter->Get(0) == '~');
    if (is_negative) {
      filter_piece.remove_prefix(1);
      if (filter_piece.length() == max_domain_length)
        continue;
    } else {
      negatives_only = false;
    }

    if (!initiator.DomainIs(filter_piece))
      continue;
    max_domain_length = filter_piece.length();
    is_positive = !is_negative;
  }

  return max_domain_length ? is_positive
                           : negatives_only && !disable_generic_rules;
}

// Returns true iff the request to |url| of type |element_type| requested by
// |initiator| matches the |rule|'s metadata: resource type, first/third party,
// domain list.
bool DoesRuleMetadataMatch(const flat::UrlRule& rule,
                           const url::Origin& initiator,
                           proto::ElementType element_type,
                           proto::ActivationType activation_type,
                           bool is_third_party,
                           bool disable_generic_rules) {
  if (element_type != proto::ELEMENT_TYPE_UNSPECIFIED &&
      !(rule.element_types() & element_type)) {
    return false;
  }
  if (activation_type != proto::ACTIVATION_TYPE_UNSPECIFIED &&
      !(rule.activation_types() & activation_type)) {
    return false;
  }

  if (is_third_party &&
      !(rule.options() & flat::OptionFlag_APPLIES_TO_THIRD_PARTY)) {
    return false;
  }
  if (!is_third_party &&
      !(rule.options() & flat::OptionFlag_APPLIES_TO_FIRST_PARTY)) {
    return false;
  }

  return rule.domains() ? DoesInitiatorMatchDomainList(
                              initiator, *rule.domains(), disable_generic_rules)
                        : !disable_generic_rules;
}

bool MatchesAny(const FlatUrlRuleList* rules,
                const GURL& url,
                const url::Origin& initiator,
                proto::ElementType element_type,
                proto::ActivationType activation_type,
                bool is_third_party,
                bool disable_generic_rules) {
  if (!rules)
    return false;
  for (const flat::UrlRule* rule : *rules) {
    DCHECK_NE(rule, nullptr);

    if (rule->url_pattern_type() != flat::UrlPatternType_REGEXP) {
      const uint8_t* begin = rule->failure_function()->data();
      const uint8_t* end = begin + rule->failure_function()->size();
      if (!IsUrlPatternMatch(url, UrlPattern(*rule), begin, end))
        continue;
    } else {
      // TODO(pkalinnikov): Implement REGEXP rules matching.
      continue;
    }

    if (DoesRuleMetadataMatch(*rule, initiator, element_type, activation_type,
                              is_third_party, disable_generic_rules)) {
      return true;
    }
  }

  return false;
}

// Returns whether the network request matches a particular part of the index.
// |is_third_party| should reflect the relation between |url| and |initiator|.
bool IsMatch(const flat::UrlPatternIndex* index,
             const GURL& url,
             const url::Origin& initiator,
             proto::ElementType element_type,
             proto::ActivationType activation_type,
             bool is_third_party,
             bool disable_generic_rules) {
  if (!index)
    return false;
  const FlatNGramIndex* hash_table = index->ngram_index();
  const flat::NGramToRules* empty_slot = index->ngram_index_empty_slot();
  DCHECK_NE(hash_table, nullptr);

  NGramHashTableProber prober;

  auto ngrams = CreateNGramExtractor<kNGramSize, uint64_t>(
      url.spec(), [](char) { return false; });
  for (uint64_t ngram : ngrams) {
    const size_t slot_index = prober.FindSlot(
        ngram, base::strict_cast<size_t>(hash_table->size()),
        [hash_table, empty_slot](NGram ngram, size_t slot_index) {
          const flat::NGramToRules* entry = hash_table->Get(slot_index);
          DCHECK_NE(entry, nullptr);
          return entry == empty_slot || entry->ngram() == ngram;
        });
    DCHECK_LT(slot_index, hash_table->size());

    const flat::NGramToRules* entry = hash_table->Get(slot_index);
    if (entry == empty_slot)
      continue;
    if (MatchesAny(entry->rule_list(), url, initiator, element_type,
                   activation_type, is_third_party, disable_generic_rules)) {
      return true;
    }
  }

  const FlatUrlRuleList* rules = index->fallback_rules();
  return MatchesAny(rules, url, initiator, element_type, activation_type,
                    is_third_party, disable_generic_rules);
}

}  // namespace

bool IndexedRulesetMatcher::Verify(const uint8_t* buffer, size_t size) {
  const auto* indexed_ruleset = flat::GetIndexedRuleset(buffer);
  flatbuffers::Verifier verifier(buffer, size);
  return indexed_ruleset->Verify(verifier);
}

IndexedRulesetMatcher::IndexedRulesetMatcher(const uint8_t* buffer, size_t size)
    : root_(flat::GetIndexedRuleset(buffer)) {
  const flat::UrlPatternIndex* index = root_->blacklist_index();
  DCHECK(!index || index->n() == kNGramSize);
  index = root_->whitelist_index();
  DCHECK(!index || index->n() == kNGramSize);
}

bool IndexedRulesetMatcher::ShouldDisableFilteringForDocument(
    const GURL& document_url,
    const url::Origin& parent_document_origin,
    proto::ActivationType activation_type) const {
  if (!document_url.is_valid() ||
      activation_type == proto::ACTIVATION_TYPE_UNSPECIFIED) {
    return false;
  }
  return IsMatch(
      root_->whitelist_index(), document_url, parent_document_origin,
      proto::ELEMENT_TYPE_UNSPECIFIED, activation_type,
      FirstPartyOrigin::IsThirdParty(document_url, parent_document_origin),
      false);
}

bool IndexedRulesetMatcher::ShouldDisallowResourceLoad(
    const GURL& url,
    const FirstPartyOrigin& first_party,
    proto::ElementType element_type,
    bool disable_generic_rules) const {
  if (!url.is_valid() || element_type == proto::ELEMENT_TYPE_UNSPECIFIED)
    return false;
  const bool is_third_party = first_party.IsThirdParty(url);
  return IsMatch(root_->blacklist_index(), url, first_party.origin(),
                 element_type, proto::ACTIVATION_TYPE_UNSPECIFIED,
                 is_third_party, disable_generic_rules) &&
         !IsMatch(root_->whitelist_index(), url, first_party.origin(),
                  element_type, proto::ACTIVATION_TYPE_UNSPECIFIED,
                  is_third_party, disable_generic_rules);
}

}  // namespace subresource_filter
