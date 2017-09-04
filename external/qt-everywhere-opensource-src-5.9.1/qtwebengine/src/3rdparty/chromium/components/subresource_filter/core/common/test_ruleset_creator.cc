// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/test_ruleset_creator.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "components/subresource_filter/core/common/indexed_ruleset.h"
#include "components/subresource_filter/core/common/proto/rules.pb.h"
#include "components/subresource_filter/core/common/unindexed_ruleset.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace subresource_filter {

namespace {

// The methods below assume that char and uint8_t are interchangeable.
static_assert(CHAR_BIT == 8, "Assumed char was 8 bits.");

proto::UrlRule CreateSuffixRule(base::StringPiece suffix) {
  proto::UrlRule rule;
  rule.set_semantics(proto::RULE_SEMANTICS_BLACKLIST);
  rule.set_source_type(proto::SOURCE_TYPE_ANY);
  rule.set_element_types(proto::ELEMENT_TYPE_ALL);
  rule.set_url_pattern_type(proto::URL_PATTERN_TYPE_SUBSTRING);
  rule.set_anchor_left(proto::ANCHOR_TYPE_NONE);
  rule.set_anchor_right(proto::ANCHOR_TYPE_BOUNDARY);
  rule.set_url_pattern(suffix.as_string());
  return rule;
}

std::vector<uint8_t> SerializeUnindexedRulesetWithSingleRule(
    const proto::UrlRule& rule) {
  std::string ruleset_contents;
  google::protobuf::io::StringOutputStream output(&ruleset_contents);
  UnindexedRulesetWriter ruleset_writer(&output);
  ruleset_writer.AddUrlRule(rule);
  ruleset_writer.Finish();

  auto data = reinterpret_cast<const uint8_t*>(ruleset_contents.data());
  return std::vector<uint8_t>(data, data + ruleset_contents.size());
}

std::vector<uint8_t> SerializeIndexedRulesetWithSingleRule(
    const proto::UrlRule& rule) {
  RulesetIndexer indexer;
  EXPECT_TRUE(indexer.AddUrlRule(rule));
  indexer.Finish();
  return std::vector<uint8_t>(indexer.data(), indexer.data() + indexer.size());
}

}  // namespace

namespace testing {

// TestRuleset -----------------------------------------------------------------

TestRuleset::TestRuleset() = default;
TestRuleset::~TestRuleset() = default;

// static
base::File TestRuleset::Open(const TestRuleset& ruleset) {
  base::File file;
  file.Initialize(ruleset.path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                    base::File::FLAG_SHARE_DELETE);
  return file;
}

// TestRulesetPair -------------------------------------------------------------

TestRulesetPair::TestRulesetPair() = default;
TestRulesetPair::~TestRulesetPair() = default;

// TestRulesetCreator ----------------------------------------------------------

TestRulesetCreator::TestRulesetCreator() = default;
TestRulesetCreator::~TestRulesetCreator() = default;

void TestRulesetCreator::CreateRulesetToDisallowURLsWithPathSuffix(
    base::StringPiece suffix,
    TestRulesetPair* test_ruleset_pair) {
  DCHECK(test_ruleset_pair);
  proto::UrlRule suffix_rule = CreateSuffixRule(suffix);
  ASSERT_NO_FATAL_FAILURE(CreateTestRulesetFromContents(
      SerializeUnindexedRulesetWithSingleRule(suffix_rule),
      &test_ruleset_pair->unindexed));
  ASSERT_NO_FATAL_FAILURE(CreateTestRulesetFromContents(
      SerializeIndexedRulesetWithSingleRule(suffix_rule),
      &test_ruleset_pair->indexed));
}

void TestRulesetCreator::CreateUnindexedRulesetToDisallowURLsWithPathSuffix(
    base::StringPiece suffix,
    TestRuleset* test_unindexed_ruleset) {
  DCHECK(test_unindexed_ruleset);
  proto::UrlRule suffix_rule = CreateSuffixRule(suffix);
  ASSERT_NO_FATAL_FAILURE(CreateTestRulesetFromContents(
      SerializeUnindexedRulesetWithSingleRule(suffix_rule),
      test_unindexed_ruleset));
}

void TestRulesetCreator::GetUniqueTemporaryPath(base::FilePath* path) {
  DCHECK(path);
  ASSERT_TRUE(scoped_temp_dir_.IsValid() ||
              scoped_temp_dir_.CreateUniqueTempDir());
  *path = scoped_temp_dir_.GetPath().AppendASCII(
      base::IntToString(next_unique_file_suffix++));
}

void TestRulesetCreator::CreateTestRulesetFromContents(
    std::vector<uint8_t> ruleset_contents,
    TestRuleset* ruleset) {
  DCHECK(ruleset);

  ruleset->contents = std::move(ruleset_contents);
  ASSERT_NO_FATAL_FAILURE(GetUniqueTemporaryPath(&ruleset->path));
  int ruleset_size_as_int = base::checked_cast<int>(ruleset->contents.size());
  int num_bytes_written = base::WriteFile(
      ruleset->path, reinterpret_cast<const char*>(ruleset->contents.data()),
      ruleset_size_as_int);
  ASSERT_EQ(ruleset_size_as_int, num_bytes_written);
}

}  // namespace testing
}  // namespace subresource_filter
