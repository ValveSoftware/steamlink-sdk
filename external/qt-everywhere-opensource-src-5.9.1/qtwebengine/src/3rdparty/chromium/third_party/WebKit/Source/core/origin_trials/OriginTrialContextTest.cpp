// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/origin_trials/OriginTrialContext.h"

#include "core/HTMLNames.h"
#include "core/dom/DOMException.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLHeadElement.h"
#include "core/html/HTMLMetaElement.h"
#include "core/testing/DummyPageHolder.h"
#include "core/testing/NullExecutionContext.h"
#include "platform/testing/HistogramTester.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebOriginTrialTokenStatus.h"
#include "public/platform/WebTrialTokenValidator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"
#include "wtf/Vector.h"
#include <memory>

namespace blink {
namespace {

const char kNonExistingTrialName[] = "This trial does not exist";
const char kFrobulateTrialName[] = "Frobulate";
const char kFrobulateEnabledOrigin[] = "https://www.example.com";
const char kFrobulateEnabledOriginUnsecure[] = "http://www.example.com";

// Names of UMA histograms
const char kResultHistogram[] = "OriginTrials.ValidationResult";

// Trial token placeholder for mocked calls to validator
const char kTokenPlaceholder[] = "The token contents are not used";

class MockTokenValidator : public WebTrialTokenValidator {
 public:
  MockTokenValidator()
      : m_response(WebOriginTrialTokenStatus::NotSupported), m_callCount(0) {}
  ~MockTokenValidator() override {}

  // blink::WebTrialTokenValidator implementation
  WebOriginTrialTokenStatus validateToken(const WebString& token,
                                          const WebSecurityOrigin& origin,
                                          WebString* featureName) override {
    m_callCount++;
    *featureName = m_feature;
    return m_response;
  }

  // Useful methods for controlling the validator
  void setResponse(WebOriginTrialTokenStatus response,
                   const WebString& feature) {
    m_response = response;
    m_feature = feature;
  }
  int callCount() { return m_callCount; }

 private:
  WebOriginTrialTokenStatus m_response;
  WebString m_feature;
  int m_callCount;

  DISALLOW_COPY_AND_ASSIGN(MockTokenValidator);
};

}  // namespace

class OriginTrialContextTest : public ::testing::Test {
 protected:
  OriginTrialContextTest()
      : m_frameworkWasEnabled(RuntimeEnabledFeatures::originTrialsEnabled()),
        m_executionContext(new NullExecutionContext()),
        m_tokenValidator(makeUnique<MockTokenValidator>()),
        m_originTrialContext(new OriginTrialContext(m_executionContext.get(),
                                                    m_tokenValidator.get())),
        m_histogramTester(new HistogramTester()) {
    RuntimeEnabledFeatures::setOriginTrialsEnabled(true);
  }

  ~OriginTrialContextTest() {
    RuntimeEnabledFeatures::setOriginTrialsEnabled(m_frameworkWasEnabled);
  }

  MockTokenValidator* tokenValidator() { return m_tokenValidator.get(); }

  void updateSecurityOrigin(const String& origin) {
    KURL pageURL(ParsedURLString, origin);
    RefPtr<SecurityOrigin> pageOrigin = SecurityOrigin::create(pageURL);
    m_executionContext->setSecurityOrigin(pageOrigin);
    m_executionContext->setIsSecureContext(SecurityOrigin::isSecure(pageURL));
  }

  bool isTrialEnabled(const String& origin, const String& featureName) {
    updateSecurityOrigin(origin);
    // Need at least one token to ensure the token validator is called.
    m_originTrialContext->addToken(kTokenPlaceholder);
    return m_originTrialContext->isTrialEnabled(featureName);
  }

  void expectStatusUniqueMetric(WebOriginTrialTokenStatus status, int count) {
    m_histogramTester->expectUniqueSample(kResultHistogram,
                                          static_cast<int>(status), count);
  }

  void expecStatusTotalMetric(int total) {
    m_histogramTester->expectTotalCount(kResultHistogram, total);
  }

 private:
  const bool m_frameworkWasEnabled;
  Persistent<NullExecutionContext> m_executionContext;
  std::unique_ptr<MockTokenValidator> m_tokenValidator;
  Persistent<OriginTrialContext> m_originTrialContext;
  std::unique_ptr<HistogramTester> m_histogramTester;
};

TEST_F(OriginTrialContextTest, EnabledNonExistingTrial) {
  tokenValidator()->setResponse(WebOriginTrialTokenStatus::Success,
                                kFrobulateTrialName);
  bool isNonExistingTrialEnabled =
      isTrialEnabled(kFrobulateEnabledOrigin, kNonExistingTrialName);
  EXPECT_FALSE(isNonExistingTrialEnabled);

  // Status metric should be updated.
  expectStatusUniqueMetric(WebOriginTrialTokenStatus::Success, 1);
}

// The feature should be enabled if a valid token for the origin is provided
TEST_F(OriginTrialContextTest, EnabledSecureRegisteredOrigin) {
  tokenValidator()->setResponse(WebOriginTrialTokenStatus::Success,
                                kFrobulateTrialName);
  bool isOriginEnabled =
      isTrialEnabled(kFrobulateEnabledOrigin, kFrobulateTrialName);
  EXPECT_TRUE(isOriginEnabled);
  EXPECT_EQ(1, tokenValidator()->callCount());

  // Status metric should be updated.
  expectStatusUniqueMetric(WebOriginTrialTokenStatus::Success, 1);
}

// ... but if the browser says it's invalid for any reason, that's enough to
// reject.
TEST_F(OriginTrialContextTest, InvalidTokenResponseFromPlatform) {
  tokenValidator()->setResponse(WebOriginTrialTokenStatus::Malformed,
                                kFrobulateTrialName);
  bool isOriginEnabled =
      isTrialEnabled(kFrobulateEnabledOrigin, kFrobulateTrialName);
  EXPECT_FALSE(isOriginEnabled);
  EXPECT_EQ(1, tokenValidator()->callCount());

  // Status metric should be updated.
  expectStatusUniqueMetric(WebOriginTrialTokenStatus::Malformed, 1);
}

// The feature should not be enabled if the origin is insecure, even if a valid
// token for the origin is provided
TEST_F(OriginTrialContextTest, EnabledNonSecureRegisteredOrigin) {
  tokenValidator()->setResponse(WebOriginTrialTokenStatus::Success,
                                kFrobulateTrialName);
  bool isOriginEnabled =
      isTrialEnabled(kFrobulateEnabledOriginUnsecure, kFrobulateTrialName);
  EXPECT_FALSE(isOriginEnabled);
  EXPECT_EQ(0, tokenValidator()->callCount());
  expectStatusUniqueMetric(WebOriginTrialTokenStatus::Insecure, 1);
}

TEST_F(OriginTrialContextTest, ParseHeaderValue) {
  std::unique_ptr<Vector<String>> tokens;
  ASSERT_TRUE(tokens = OriginTrialContext::parseHeaderValue(" foo\t "));
  ASSERT_EQ(1u, tokens->size());
  EXPECT_EQ("foo", (*tokens)[0]);

  ASSERT_TRUE(tokens = OriginTrialContext::parseHeaderValue(" \" bar \" "));
  ASSERT_EQ(1u, tokens->size());
  EXPECT_EQ(" bar ", (*tokens)[0]);

  ASSERT_TRUE(tokens = OriginTrialContext::parseHeaderValue(" foo, bar"));
  ASSERT_EQ(2u, tokens->size());
  EXPECT_EQ("foo", (*tokens)[0]);
  EXPECT_EQ("bar", (*tokens)[1]);

  ASSERT_TRUE(tokens =
                  OriginTrialContext::parseHeaderValue(",foo, ,bar,,'  ', ''"));
  ASSERT_EQ(3u, tokens->size());
  EXPECT_EQ("foo", (*tokens)[0]);
  EXPECT_EQ("bar", (*tokens)[1]);
  EXPECT_EQ("  ", (*tokens)[2]);

  ASSERT_TRUE(tokens =
                  OriginTrialContext::parseHeaderValue("  \"abc\"  , 'def',g"));
  ASSERT_EQ(3u, tokens->size());
  EXPECT_EQ("abc", (*tokens)[0]);
  EXPECT_EQ("def", (*tokens)[1]);
  EXPECT_EQ("g", (*tokens)[2]);

  ASSERT_TRUE(tokens = OriginTrialContext::parseHeaderValue(
                  " \"a\\b\\\"c'd\", 'e\\f\\'g' "));
  ASSERT_EQ(2u, tokens->size());
  EXPECT_EQ("ab\"c'd", (*tokens)[0]);
  EXPECT_EQ("ef'g", (*tokens)[1]);

  ASSERT_TRUE(tokens =
                  OriginTrialContext::parseHeaderValue("\"ab,c\" , 'd,e'"));
  ASSERT_EQ(2u, tokens->size());
  EXPECT_EQ("ab,c", (*tokens)[0]);
  EXPECT_EQ("d,e", (*tokens)[1]);

  ASSERT_TRUE(tokens = OriginTrialContext::parseHeaderValue("  "));
  EXPECT_EQ(0u, tokens->size());

  ASSERT_TRUE(tokens = OriginTrialContext::parseHeaderValue(""));
  EXPECT_EQ(0u, tokens->size());

  ASSERT_TRUE(tokens = OriginTrialContext::parseHeaderValue(" ,, \"\" "));
  EXPECT_EQ(0u, tokens->size());
}

TEST_F(OriginTrialContextTest, ParseHeaderValue_NotCommaSeparated) {
  EXPECT_FALSE(OriginTrialContext::parseHeaderValue("foo bar"));
  EXPECT_FALSE(OriginTrialContext::parseHeaderValue("\"foo\" 'bar'"));
  EXPECT_FALSE(OriginTrialContext::parseHeaderValue("foo 'bar'"));
  EXPECT_FALSE(OriginTrialContext::parseHeaderValue("\"foo\" bar"));
}

}  // namespace blink
