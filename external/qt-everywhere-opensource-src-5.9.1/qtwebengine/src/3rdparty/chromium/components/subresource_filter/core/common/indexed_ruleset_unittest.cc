// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/indexed_ruleset.h"

#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "components/subresource_filter/core/common/first_party_origin.h"
#include "components/subresource_filter/core/common/proto/rules.pb.h"
#include "components/subresource_filter/core/common/url_pattern.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

namespace {

constexpr proto::AnchorType kAnchorNone = proto::ANCHOR_TYPE_NONE;
constexpr proto::AnchorType kBoundary = proto::ANCHOR_TYPE_BOUNDARY;
constexpr proto::AnchorType kSubdomain = proto::ANCHOR_TYPE_SUBDOMAIN;
constexpr proto::UrlPatternType kSubstring = proto::URL_PATTERN_TYPE_SUBSTRING;

constexpr proto::SourceType kAnyParty = proto::SOURCE_TYPE_ANY;
constexpr proto::SourceType kFirstParty = proto::SOURCE_TYPE_FIRST_PARTY;
constexpr proto::SourceType kThirdParty = proto::SOURCE_TYPE_THIRD_PARTY;

// Note: Returns unique origin on origin_string == nullptr.
url::Origin GetOrigin(const char* origin_string) {
  return origin_string ? url::Origin(GURL(origin_string)) : url::Origin();
}

class UrlRuleBuilder {
 public:
  explicit UrlRuleBuilder(const UrlPattern& url_pattern,
                          bool is_whitelist = false)
      : UrlRuleBuilder(url_pattern, kAnyParty, is_whitelist) {}

  UrlRuleBuilder(const UrlPattern& url_pattern,
                 proto::SourceType source_type,
                 bool is_whitelist) {
    rule_.set_semantics(is_whitelist ? proto::RULE_SEMANTICS_WHITELIST
                                     : proto::RULE_SEMANTICS_BLACKLIST);

    rule_.set_source_type(source_type);
    rule_.set_element_types(proto::ELEMENT_TYPE_ALL);

    rule_.set_url_pattern_type(url_pattern.type);
    rule_.set_anchor_left(url_pattern.anchor_left);
    rule_.set_anchor_right(url_pattern.anchor_right);
    rule_.set_match_case(url_pattern.match_case);
    rule_.set_url_pattern(url_pattern.url_pattern.as_string());
  }

  UrlRuleBuilder& AddDomain(std::string domain_pattern) {
    DCHECK(!domain_pattern.empty());
    auto* domain = rule_.add_domains();
    if (domain_pattern[0] == '~') {
      domain_pattern.erase(0, 1);
      domain->set_exclude(true);
    }
    domain->set_domain(domain_pattern);
    return *this;
  }

  UrlRuleBuilder& AddDomains(const std::vector<std::string>& domains) {
    for (const std::string domain : domains)
      AddDomain(domain);
    return *this;
  }

  const proto::UrlRule& rule() const { return rule_; }
  proto::UrlRule& rule() { return rule_; }

 private:
  proto::UrlRule rule_;

  DISALLOW_COPY_AND_ASSIGN(UrlRuleBuilder);
};

}  // namespace

class IndexedRulesetTest : public testing::Test {
 public:
  IndexedRulesetTest() = default;

 protected:
  bool ShouldAllow(const char* url,
                   const char* initiator = nullptr,
                   proto::ElementType element_type = proto::ELEMENT_TYPE_OTHER,
                   bool disable_generic_rules = false) const {
    DCHECK_NE(matcher_.get(), nullptr);
    url::Origin origin = GetOrigin(initiator);
    FirstPartyOrigin first_party(origin);
    return !matcher_->ShouldDisallowResourceLoad(
        GURL(url), first_party, element_type, disable_generic_rules);
  }

  bool ShouldAllow(const char* url,
                   const char* initiator,
                   bool disable_generic_rules) const {
    return ShouldAllow(url, initiator, proto::ELEMENT_TYPE_OTHER,
                       disable_generic_rules);
  }

  bool ShouldDeactivate(const char* document_url,
                        const char* initiator = nullptr,
                        proto::ActivationType activation_type =
                            proto::ACTIVATION_TYPE_UNSPECIFIED) const {
    DCHECK(matcher_);
    url::Origin origin = GetOrigin(initiator);
    return matcher_->ShouldDisableFilteringForDocument(GURL(document_url),
                                                       origin, activation_type);
  }

  void AddUrlRule(const proto::UrlRule& rule) {
    ASSERT_TRUE(indexer_.AddUrlRule(rule)) << "URL pattern: "
                                           << rule.url_pattern();
  }

  void AddSimpleRule(const UrlPattern& url_pattern, bool is_whitelist) {
    AddUrlRule(UrlRuleBuilder(url_pattern, is_whitelist).rule());
  }

  void AddBlacklistRule(const UrlPattern& url_pattern,
                        proto::SourceType source_type = kAnyParty) {
    AddUrlRule(UrlRuleBuilder(url_pattern, source_type, false).rule());
  }

  void AddWhitelistRuleWithActivationTypes(const UrlPattern& url_pattern,
                                           int32_t activation_types) {
    UrlRuleBuilder builder(url_pattern, kAnyParty, true);
    builder.rule().set_element_types(proto::ELEMENT_TYPE_UNSPECIFIED);
    builder.rule().set_activation_types(activation_types);
    AddUrlRule(builder.rule());
  }

  void Finish() {
    indexer_.Finish();
    matcher_.reset(new IndexedRulesetMatcher(indexer_.data(), indexer_.size()));
  }

  void Reset() {
    matcher_.reset(nullptr);
    indexer_.~RulesetIndexer();
    new (&indexer_) RulesetIndexer();
  }

  RulesetIndexer indexer_;
  std::unique_ptr<IndexedRulesetMatcher> matcher_;

 private:
  DISALLOW_COPY_AND_ASSIGN(IndexedRulesetTest);
};

TEST_F(IndexedRulesetTest, OneRuleWithoutMetaInfo) {
  const struct {
    UrlPattern url_pattern;
    const char* url;
    bool expect_allowed;
  } kTestCases[] = {
      // SUBSTRING
      {{"abcd", kSubstring}, "http://example.com/abcd", false},
      {{"abcd", kSubstring}, "http://example.com/dcab", true},
      {{"42", kSubstring}, "http://example.com/adcd/picture42.png", false},
      {{"&test", kSubstring},
       "http://example.com/params?para1=false&test=true",
       false},
      {{"-test-42.", kSubstring}, "http://example.com/unit-test-42.1", false},
      {{"/abcdtest160x600.", kSubstring},
       "http://example.com/abcdtest160x600.png",
       false},

      // WILDCARDED
      {{"http://example.com/abcd/picture*.png"},
       "http://example.com/abcd/picture42.png",
       false},
      {{"example.com", kSubdomain, kAnchorNone}, "http://example.com", false},
      {{"example.com", kSubdomain, kAnchorNone},
       "http://test.example.com",
       false},
      {{"example.com", kSubdomain, kAnchorNone},
       "https://test.example.com.com",
       false},
      {{"example.com", kSubdomain, kAnchorNone},
       "https://test.rest.example.com",
       false},
      {{"example.com", kSubdomain, kAnchorNone},
       "https://test_example.com",
       true},

      {{"http://example.com", kBoundary, kAnchorNone},
       "http://example.com/",
       false},
      {{"http://example.com", kBoundary, kAnchorNone},
       "http://example.com/42",
       false},
      {{"http://example.com", kBoundary, kAnchorNone},
       "http://example.com/42/http://example.com/",
       false},
      {{"http://example.com", kBoundary, kAnchorNone},
       "http://example.com/42/http://example.info/",
       false},
      {{"http://example.com/", kBoundary, kBoundary},
       "http://example.com",
       false},
      {{"http://example.com/", kBoundary, kBoundary},
       "http://example.com/42",
       true},
      {{"http://example.com/", kBoundary, kBoundary},
       "http://example.info/42/http://example.com/",
       true},
      {{"http://example.com/", kBoundary, kBoundary},
       "http://example.info/42/http://example.com/",
       true},
      {{"http://example.com/", kBoundary, kBoundary},
       "http://example.com/",
       false},
      {{"http://example.com/", kBoundary, kBoundary},
       "http://example.com/42.swf",
       true},
      {{"http://example.com/", kBoundary, kBoundary},
       "http://example.info/redirect/http://example.com/",
       true},
      {{"pdf", kAnchorNone, kBoundary}, "http://example.com/abcd.pdf", false},
      {{"pdf", kAnchorNone, kBoundary}, "http://example.com/pdfium", true},
      {{"http://example.com^"}, "http://example.com/", false},
      {{"http://example.com^"}, "http://example.com:8000/", false},
      {{"http://example.com^"}, "http://example.com.ru", true},
      {{"^example.com^"},
       "http://example.com:8000/42.loss?a=12&b=%D1%82%D0%B5%D1%81%D1%82",
       false},
      {{"^42.loss^"},
       "http://example.com:8000/42.loss?a=12&b=%D1%82%D0%B5%D1%81%D1%82",
       false},

      // FIXME(pkalinnikov): The '^' at the end should match end-of-string.
      // {"^%D1%82%D0%B5%D1%81%D1%82^",
      //  "http://example.com:8000/42.loss?a=12&b=%D1%82%D0%B5%D1%81%D1%82",
      //  false},
      // {"/abcd/*/picture^", "http://example.com/abcd/42/picture", false},

      {{"/abcd/*/picture^"},
       "http://example.com/abcd/42/loss/picture?param",
       false},
      {{"/abcd/*/picture^"}, "http://example.com/abcd//picture/42", false},
      {{"/abcd/*/picture^"}, "http://example.com/abcd/picture", true},
      {{"/abcd/*/picture^"}, "http://example.com/abcd/42/pictureraph", true},
      {{"/abcd/*/picture^"}, "http://example.com/abcd/42/picture.swf", true},
      {{"test.example.com^", kSubdomain, kAnchorNone},
       "http://test.example.com/42.swf",
       false},
      {{"test.example.com^", kSubdomain, kAnchorNone},
       "http://server1.test.example.com/42.swf",
       false},
      {{"test.example.com^", kSubdomain, kAnchorNone},
       "https://test.example.com:8000/",
       false},
      {{"test.example.com^", kSubdomain, kAnchorNone},
       "http://test.example.com.ua/42.swf",
       true},
      {{"test.example.com^", kSubdomain, kAnchorNone},
       "http://example.com/redirect/http://test.example.com/",
       true},

      {{"/abcd/*"}, "https://example.com/abcd/", false},
      {{"/abcd/*"}, "http://example.com/abcd/picture.jpeg", false},
      {{"/abcd/*"}, "https://example.com/abcd", true},
      {{"/abcd/*"}, "http://abcd.example.com", true},
      {{"*/abcd/"}, "https://example.com/abcd/", false},
      {{"*/abcd/"}, "http://example.com/abcd/picture.jpeg", false},
      {{"*/abcd/"}, "https://example.com/test-abcd/", true},
      {{"*/abcd/"}, "http://abcd.example.com", true},

      // FIXME(pkalinnikov): Implement REGEXP matching.
      // REGEXP
      // {"/test|rest\\d+/", "http://example.com/test42", false},
      // {"/test|rest\\d+/", "http://example.com/test", false},
      // {"/test|rest\\d+/", "http://example.com/rest42", false},
      // {"/test|rest\\d+/", "http://example.com/rest", true},
      // {"/example\\.com/.*\\/[a-zA-Z0-9]{3}/", "http://example.com/abcd/42y",
      //  false},
      // {"/example\\.com/.*\\/[a-zA-Z0-9]{3}/", "http://example.com/abcd/%42y",
      //  true},
      // {"||example.com^*/test.htm", "http://example.com/unit/test.html",
      // false},
      // {"||example.com^*/test.htm", "http://examole.com/test.htm", true},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message()
                 << "Rule: " << test_case.url_pattern.url_pattern
                 << "; URL: " << test_case.url);

    AddBlacklistRule(test_case.url_pattern);
    Finish();

    EXPECT_EQ(test_case.expect_allowed, ShouldAllow(test_case.url));
    Reset();
  }
}

TEST_F(IndexedRulesetTest, OneRuleWithThirdParty) {
  const struct {
    const char* url_pattern;
    proto::SourceType source_type;

    const char* url;
    const char* initiator;
    bool expect_allowed;
  } kTestCases[] = {
      {"example.com", kThirdParty, "http://example.com", "http://exmpl.org",
       false},
      {"example.com", kThirdParty, "http://example.com", "http://example.com",
       true},
      {"example.com", kThirdParty, "http://example.com/path?k=v",
       "http://exmpl.org", false},
      {"example.com", kThirdParty, "http://example.com/path?k=v",
       "http://example.com", true},
      {"example.com", kFirstParty, "http://example.com/path?k=v",
       "http://example.com", false},
      {"example.com", kFirstParty, "http://example.com/path?k=v",
       "http://exmpl.com", true},
      {"example.com", kAnyParty, "http://example.com/path?k=v",
       "http://example.com", false},
      {"example.com", kAnyParty, "http://example.com/path?k=v",
       "http://exmpl.com", false},
      {"example.com", kThirdParty, "http://subdomain.example.com",
       "http://example.com", true},
      {"example.com", kThirdParty, "http://example.com", nullptr, false},

      // Public Suffix List tests.
      {"example.com", kThirdParty, "http://two.example.com",
       "http://one.example.com", true},
      {"example.com", kThirdParty, "http://example.com",
       "http://one.example.com", true},
      {"example.com", kThirdParty, "http://two.example.com",
       "http://example.com", true},
      {"example.com", kThirdParty, "http://example.com", "http://example.org",
       false},
      {"appspot.com", kThirdParty, "http://two.appspot.org",
       "http://one.appspot.com", true},
  };

  for (auto test_case : kTestCases) {
    SCOPED_TRACE(testing::Message()
                 << "Rule: " << test_case.url_pattern << "; source: "
                 << (int)test_case.source_type << "; URL: " << test_case.url
                 << "; Initiator: " << test_case.initiator);

    AddBlacklistRule(UrlPattern(test_case.url_pattern, kSubstring),
                     test_case.source_type);
    Finish();

    EXPECT_EQ(test_case.expect_allowed,
              ShouldAllow(test_case.url, test_case.initiator));
    Reset();
  }
}

TEST_F(IndexedRulesetTest, OneRuleWithDomainList) {
  const struct {
    const char* url_pattern;
    std::vector<std::string> domains;

    const char* url;
    const char* initiator;
    bool expect_allowed;
  } kTestCases[] = {
      {"example.com",
       {"domain1.com", "domain2.com"},
       "http://example.com",
       "http://domain1.com",
       false},

      {"example.com",
       {"domain1.com", "domain2.com"},
       "http://example.com",
       "http://not_domain1.com",
       true},

      {"example.com",
       {"domain1.com", "domain2.com"},
       "http://example.com",
       "http://domain2.com",
       false},

      {"example.com",
       {"domain1.com", "domain2.com"},
       "http://example.com",
       "http://subdomain.domain2.com",
       false},

      {"example.com",
       {"domain1.com", "domain2.com"},
       "http://example.com",
       "http://domain3.com",
       true},

      {"example.com",
       {"~domain1.com", "~domain2.com"},
       "http://example.com",
       "http://domain2.com",
       true},

      {"example.com",
       {"~domain1.com", "~domain2.com"},
       "http://example.com",
       "http://domain3.com",
       false},

      {"example.com",
       {"domain1.com", "~subdomain1.domain1.com"},
       "http://example.com",
       "http://subdomain2.domain1.com",
       false},

      {"example.com",
       {"domain1.com", "~subdomain1.domain1.com"},
       "http://example.com",
       "http://subdomain1.domain1.com",
       true},

      {"example.com",
       {"domain1.com", "domain2.com"},
       "http://example.com",
       nullptr,
       true},

      // The following test addresses a former bug in domain list matcher. When
      // "domain.com" was matched, the positive filters lookup stopped, and the
      // next domain was considered as a negative. The initial character was
      // skipped (supposing it's a '~') and the remainder was considered a
      // domain. So "ddomain.com" would be matched and thus the whole rule would
      // be classified as non-matching, which is not correct.
      {"ex.com",
       {"domain.com", "ddomain.com", "~sub.domain.com"},
       "http://ex.com",
       "http://domain.com",
       false},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message() << "Rule: " << test_case.url_pattern
                                    << "; URL: " << test_case.url
                                    << "; Initiator: " << test_case.initiator);

    UrlRuleBuilder builder(UrlPattern(test_case.url_pattern, kSubstring));
    builder.AddDomains(test_case.domains);
    AddUrlRule(builder.rule());
    Finish();

    EXPECT_EQ(test_case.expect_allowed,
              ShouldAllow(test_case.url, test_case.initiator));
    Reset();
  }
}

TEST_F(IndexedRulesetTest, OneRuleWithElementTypes) {
  constexpr proto::ElementType kAll = proto::ELEMENT_TYPE_ALL;
  constexpr proto::ElementType kImage = proto::ELEMENT_TYPE_IMAGE;
  constexpr proto::ElementType kFont = proto::ELEMENT_TYPE_FONT;
  constexpr proto::ElementType kScript = proto::ELEMENT_TYPE_SCRIPT;
  constexpr proto::ElementType kSubdoc = proto::ELEMENT_TYPE_SUBDOCUMENT;
  constexpr proto::ElementType kPopup = proto::ELEMENT_TYPE_POPUP;

  const struct {
    const char* url_pattern;
    int32_t element_types;

    const char* url;
    proto::ElementType element_type;
    bool expect_allowed;
  } kTestCases[] = {
      {"ex.com", kAll, "http://ex.com/img.jpg", kImage, false},
      {"ex.com", kAll & ~kPopup, "http://ex.com/img", kPopup, true},

      {"ex.com", kImage, "http://ex.com/img.jpg", kImage, false},
      {"ex.com", kAll & ~kImage, "http://ex.com/img.jpg", kImage, true},
      {"ex.com", kScript, "http://ex.com/img.jpg", kImage, true},
      {"ex.com", kAll & ~kScript, "http://ex.com/img.jpg", kImage, false},

      {"ex.com", kImage | kFont, "http://ex.com/font", kFont, false},
      {"ex.com", kImage | kFont, "http://ex.com/image", kImage, false},
      {"ex.com", kImage | kFont, "http://ex.com/video",
       proto::ELEMENT_TYPE_MEDIA, true},
      {"ex.com", kAll & ~kFont & ~kScript, "http://ex.com/font", kFont, true},
      {"ex.com", kAll & ~kFont & ~kScript, "http://ex.com/scr", kScript, true},
      {"ex.com", kAll & ~kFont & ~kScript, "http://ex.com/img", kImage, false},
      {"ex.com$subdocument,~subdocument", kSubdoc & ~kSubdoc,
       "http://ex.com/sub", kSubdoc, true},

      {"ex.com", kAll, "http://ex.com", proto::ELEMENT_TYPE_OTHER, false},
      {"ex.com", kAll, "http://ex.com", proto::ELEMENT_TYPE_UNSPECIFIED, true},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message()
                 << "Rule: " << test_case.url_pattern << "; ElementTypes: "
                 << (int)test_case.element_types << "; URL: " << test_case.url
                 << "; ElementType: " << (int)test_case.element_type);

    UrlRuleBuilder builder(UrlPattern(test_case.url_pattern, kSubstring));
    builder.rule().set_element_types(test_case.element_types);
    AddUrlRule(builder.rule());
    Finish();

    EXPECT_EQ(test_case.expect_allowed,
              ShouldAllow(test_case.url, nullptr /* initiator */,
                          test_case.element_type));
    Reset();
  }
}

TEST_F(IndexedRulesetTest, OneRuleWithActivationTypes) {
  constexpr proto::ActivationType kNone = proto::ACTIVATION_TYPE_UNSPECIFIED;
  constexpr proto::ActivationType kDocument = proto::ACTIVATION_TYPE_DOCUMENT;

  const struct {
    const char* url_pattern;
    int32_t activation_types;

    const char* document_url;
    proto::ActivationType activation_type;
    bool expect_disabled;
  } kTestCases[] = {
      {"example.com", kDocument, "http://example.com", kDocument, true},
      {"xample.com", kDocument, "http://example.com", kDocument, true},
      {"exampl.com", kDocument, "http://example.com", kDocument, false},

      {"example.com", kNone, "http://example.com", kDocument, false},
      {"example.com", kDocument, "http://example.com", kNone, false},
      {"example.com", kNone, "http://example.com", kNone, false},

      // Invalid GURL.
      {"example.com", kDocument, "http;//example.com", kDocument, false},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message()
                 << "Rule: " << test_case.url_pattern
                 << "; ActivationTypes: " << (int)test_case.activation_types
                 << "; DocURL: " << test_case.document_url
                 << "; ActivationType: " << (int)test_case.activation_type);

    AddWhitelistRuleWithActivationTypes(
        UrlPattern(test_case.url_pattern, kSubstring),
        test_case.activation_types);
    Finish();

    EXPECT_EQ(test_case.expect_disabled,
              ShouldDeactivate(test_case.document_url, nullptr /* initiator */,
                               test_case.activation_type));
    EXPECT_EQ(test_case.expect_disabled,
              ShouldDeactivate(test_case.document_url, "http://example.com/",
                               test_case.activation_type));
    EXPECT_EQ(test_case.expect_disabled,
              ShouldDeactivate(test_case.document_url, "http://xmpl.com/",
                               test_case.activation_type));
    Reset();
  }
}

TEST_F(IndexedRulesetTest, MatchWithDisableGenericRules) {
  // Generic rules.
  ASSERT_NO_FATAL_FAILURE(
      AddUrlRule(UrlRuleBuilder(UrlPattern("some_text", kSubstring)).rule()));
  ASSERT_NO_FATAL_FAILURE(
      AddUrlRule(UrlRuleBuilder(UrlPattern("another_text", kSubstring))
                     .AddDomain("~example.com")
                     .rule()));

  // Domain specific rules.
  ASSERT_NO_FATAL_FAILURE(
      AddUrlRule(UrlRuleBuilder(UrlPattern("some_text", kSubstring))
                     .AddDomain("example1.com")
                     .rule()));
  ASSERT_NO_FATAL_FAILURE(
      AddUrlRule(UrlRuleBuilder(UrlPattern("more_text", kSubstring))
                     .AddDomain("example.com")
                     .AddDomain("~exclude.example.com")
                     .rule()));
  ASSERT_NO_FATAL_FAILURE(
      AddUrlRule(UrlRuleBuilder(UrlPattern("last_text", kSubstring))
                     .AddDomain("example1.com")
                     .AddDomain("sub.example2.com")
                     .rule()));

  Finish();

  const struct {
    const char* url_pattern;
    const char* initiator;
    bool should_allow_with_disable_generic_rules;
    bool should_allow_with_enable_all_rules;
  } kTestCases[] = {
      {"http://ex.com/some_text", "http://example.com", true, false},
      {"http://ex.com/some_text", "http://example1.com", false, false},

      {"http://ex.com/another_text", "http://example.com", true, true},
      {"http://ex.com/another_text", "http://example1.com", true, false},

      {"http://ex.com/more_text", "http://example.com", false, false},
      {"http://ex.com/more_text", "http://exclude.example.com", true, true},
      {"http://ex.com/more_text", "http://example1.com", true, true},

      {"http://ex.com/last_text", "http://example.com", true, true},
      {"http://ex.com/last_text", "http://example1.com", false, false},
      {"http://ex.com/last_text", "http://example2.com", true, true},
      {"http://ex.com/last_text", "http://sub.example2.com", false, false},
  };

  constexpr bool kDisableGenericRules = true;
  constexpr bool kEnableAllRules = false;
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message() << "Url: " << test_case.url_pattern
                                    << "; Initiator: " << test_case.initiator);

    EXPECT_EQ(test_case.should_allow_with_disable_generic_rules,
              ShouldAllow(test_case.url_pattern, test_case.initiator,
                          kDisableGenericRules));
    EXPECT_EQ(test_case.should_allow_with_enable_all_rules,
              ShouldAllow(test_case.url_pattern, test_case.initiator,
                          kEnableAllRules));
  }
}

TEST_F(IndexedRulesetTest, EmptyRuleset) {
  Finish();
  EXPECT_TRUE(ShouldAllow("http://example.com"));
  EXPECT_TRUE(ShouldAllow("http://another.example.com?param=val"));
  EXPECT_TRUE(ShouldAllow(nullptr));
}

TEST_F(IndexedRulesetTest, NoRuleApplies) {
  AddSimpleRule(UrlPattern("?filtered_content=", kSubstring), false);
  AddSimpleRule(UrlPattern("&filtered_content=", kSubstring), false);
  Finish();

  EXPECT_TRUE(ShouldAllow("http://example.com"));
  EXPECT_TRUE(ShouldAllow("http://example.com?filtered_not"));
}

TEST_F(IndexedRulesetTest, SimpleBlacklist) {
  AddSimpleRule(UrlPattern("?param=", kSubstring), false);
  Finish();

  EXPECT_TRUE(ShouldAllow("https://example.com"));
  EXPECT_FALSE(ShouldAllow("http://example.org?param=image1"));
}

TEST_F(IndexedRulesetTest, SimpleWhitelist) {
  AddSimpleRule(UrlPattern("example.com/?filtered_content=", kSubstring), true);
  Finish();

  EXPECT_TRUE(ShouldAllow("https://example.com?filtered_content=image1"));
}

TEST_F(IndexedRulesetTest, BlacklistWhitelist) {
  AddSimpleRule(UrlPattern("?filter=", kSubstring), false);
  AddSimpleRule(UrlPattern("whitelisted.com/?filter=", kSubstring), true);
  Finish();

  EXPECT_TRUE(ShouldAllow("https://whitelisted.com?filter=off"));
  EXPECT_TRUE(ShouldAllow("https://notblacklisted.com"));
  EXPECT_FALSE(ShouldAllow("http://blacklisted.com?filter=on"));
}

TEST_F(IndexedRulesetTest, BlacklistAndActivationType) {
  const auto kDocument = proto::ACTIVATION_TYPE_DOCUMENT;

  AddSimpleRule(UrlPattern("example.com", kSubstring), false);
  AddWhitelistRuleWithActivationTypes(UrlPattern("example.com", kSubstring),
                                      kDocument);
  Finish();

  EXPECT_TRUE(ShouldDeactivate("https://example.com", nullptr, kDocument));
  EXPECT_FALSE(ShouldDeactivate("https://xample.com", nullptr, kDocument));
  EXPECT_FALSE(ShouldAllow("https://example.com"));
  EXPECT_TRUE(ShouldAllow("https://xample.com"));
}

TEST_F(IndexedRulesetTest, RuleWithUnsupportedOptions) {
  UrlRuleBuilder builder(UrlPattern("exmpl"), proto::SOURCE_TYPE_ANY, false);
  builder.rule().set_activation_types(builder.rule().activation_types() |
                                      (proto::ACTIVATION_TYPE_MAX << 1));
  builder.rule().set_element_types(builder.rule().element_types() |
                                   (proto::ELEMENT_TYPE_MAX << 1));
  EXPECT_FALSE(indexer_.AddUrlRule(builder.rule()));

  AddSimpleRule(UrlPattern("example.com", kSubstring), false);
  Finish();

  EXPECT_TRUE(ShouldAllow("https://exmpl.com"));
  EXPECT_FALSE(ShouldAllow("https://example.com"));
}

}  // namespace subresource_filter
