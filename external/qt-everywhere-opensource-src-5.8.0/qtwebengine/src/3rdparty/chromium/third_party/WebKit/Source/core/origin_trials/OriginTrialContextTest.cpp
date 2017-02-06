// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/origin_trials/OriginTrialContext.h"

#include "core/HTMLNames.h"
#include "core/dom/DOMException.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLDocument.h"
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

const char kNonExistingFeatureName[] = "This feature does not exist";
const char kFrobulateFeatureName[] = "Frobulate";
const char kFrobulateEnabledOrigin[] = "https://www.example.com";
const char kFrobulateEnabledOriginUnsecure[] = "http://www.example.com";

// Names of UMA histograms
const char kEnabledHistogram[] = "OriginTrials.FeatureEnabled";
const char kMessageHistogram[] = "OriginTrials.FeatureEnabled.MessageGenerated";

// Values for message generated histogram
const int kMessageHistogramValueNotRequested = 0;
const int kMessageHistogramValueYes = 1;

// Trial token placeholder for mocked calls to validator
const char kTokenPlaceholder[] = "The token contents are not used";

class MockTokenValidator : public WebTrialTokenValidator {
public:
    MockTokenValidator()
        : m_response(WebOriginTrialTokenStatus::NotSupported)
        , m_callCount(0)
    {
    }
    ~MockTokenValidator() override {}

    // blink::WebTrialTokenValidator implementation
    WebOriginTrialTokenStatus validateToken(const blink::WebString& token, const blink::WebSecurityOrigin& origin, const blink::WebString& featureName) override
    {
        m_callCount++;
        return m_response;
    }

    // Useful methods for controlling the validator
    void setResponse(WebOriginTrialTokenStatus response)
    {
        m_response = response;
    }
    int callCount()
    {
        return m_callCount;
    }

private:
    WebOriginTrialTokenStatus m_response;
    int m_callCount;

    DISALLOW_COPY_AND_ASSIGN(MockTokenValidator);
};

} // namespace

class OriginTrialContextTest : public ::testing::Test {
protected:
    OriginTrialContextTest()
        : m_frameworkWasEnabled(RuntimeEnabledFeatures::originTrialsEnabled())
        , m_executionContext(new NullExecutionContext())
        , m_tokenValidator(wrapUnique(new MockTokenValidator()))
        , m_originTrialContext(new OriginTrialContext(m_executionContext.get(), m_tokenValidator.get()))
        , m_histogramTester(new HistogramTester())
    {
        RuntimeEnabledFeatures::setOriginTrialsEnabled(true);
    }

    ~OriginTrialContextTest()
    {
        RuntimeEnabledFeatures::setOriginTrialsEnabled(m_frameworkWasEnabled);
    }

    MockTokenValidator* tokenValidator() { return m_tokenValidator.get(); }

    void updateSecurityOrigin(const String& origin)
    {
        KURL pageURL(ParsedURLString, origin);
        RefPtr<SecurityOrigin> pageOrigin = SecurityOrigin::create(pageURL);
        m_executionContext->setSecurityOrigin(pageOrigin);
        m_executionContext->setIsSecureContext(SecurityOrigin::isSecure(pageURL));
    }

    bool isFeatureEnabled(const String& origin, const String& featureName, String* errorMessage)
    {
        updateSecurityOrigin(origin);
        // Need at least one token to ensure the token validator is called.
        m_originTrialContext->addToken(kTokenPlaceholder);
        return m_originTrialContext->isFeatureEnabled(featureName, errorMessage);
    }

    bool isFeatureEnabledWithoutErrorMessage(const String& origin, const String& featureName)
    {
        return isFeatureEnabled(origin, featureName, nullptr);
    }

    void expectEnabledUniqueMetric(WebOriginTrialTokenStatus status, int count)
    {
        m_histogramTester->expectUniqueSample(
            kEnabledHistogram, static_cast<int>(status), count);
    }

    void expectEnabledTotalMetric(int total)
    {
        m_histogramTester->expectTotalCount(kEnabledHistogram, total);
    }

    void expectMessageUniqueMetric(int messageGeneratedValue, int count)
    {
        m_histogramTester->expectUniqueSample(
            kMessageHistogram, messageGeneratedValue, count);
    }

    void expectMessageTotalMetric(int total)
    {
        m_histogramTester->expectTotalCount(kMessageHistogram, total);
    }

private:
    const bool m_frameworkWasEnabled;
    Persistent<NullExecutionContext> m_executionContext;
    std::unique_ptr<MockTokenValidator> m_tokenValidator;
    Persistent<OriginTrialContext> m_originTrialContext;
    std::unique_ptr<HistogramTester> m_histogramTester;
};

TEST_F(OriginTrialContextTest, EnabledNonExistingFeature)
{
    String errorMessage;
    tokenValidator()->setResponse(WebOriginTrialTokenStatus::WrongFeature);
    bool isNonExistingFeatureEnabled = isFeatureEnabled(kFrobulateEnabledOrigin,
        kNonExistingFeatureName,
        &errorMessage);
    EXPECT_FALSE(isNonExistingFeatureEnabled);
    EXPECT_EQ(("The provided token(s) are not valid for the 'This feature does not exist' feature."), errorMessage);
}

TEST_F(OriginTrialContextTest, EnabledNonExistingFeatureWithoutErrorMessage)
{
    bool isNonExistingFeatureEnabled = isFeatureEnabledWithoutErrorMessage(
        kFrobulateEnabledOrigin,
        kNonExistingFeatureName);
    EXPECT_FALSE(isNonExistingFeatureEnabled);
}

// The feature should be enabled if a valid token for the origin is provided
TEST_F(OriginTrialContextTest, EnabledSecureRegisteredOrigin)
{
    String errorMessage;
    tokenValidator()->setResponse(WebOriginTrialTokenStatus::Success);
    bool isOriginEnabled = isFeatureEnabled(kFrobulateEnabledOrigin,
        kFrobulateFeatureName,
        &errorMessage);
    EXPECT_TRUE(isOriginEnabled);
    EXPECT_TRUE(errorMessage.isEmpty()) << "Message should be empty, was: " << errorMessage;
    EXPECT_EQ(1, tokenValidator()->callCount());

    // Enabled metric should be updated, but the message generated metric
    // should not be updated on success.
    expectEnabledUniqueMetric(WebOriginTrialTokenStatus::Success, 1);
    expectMessageTotalMetric(0);
}

// ... but if the browser says it's invalid for any reason, that's enough to
// reject.
TEST_F(OriginTrialContextTest, InvalidTokenResponseFromPlatform)
{
    String errorMessage;
    tokenValidator()->setResponse(WebOriginTrialTokenStatus::Malformed);
    bool isOriginEnabled = isFeatureEnabled(kFrobulateEnabledOrigin,
        kFrobulateFeatureName,
        &errorMessage);
    EXPECT_FALSE(isOriginEnabled);
    EXPECT_EQ(("The provided token(s) are not valid for the 'Frobulate' feature."), errorMessage);
    EXPECT_EQ(1, tokenValidator()->callCount());

    // Enabled and message generated metrics should be updated
    expectEnabledUniqueMetric(WebOriginTrialTokenStatus::Malformed, 1);
    expectMessageUniqueMetric(kMessageHistogramValueYes, 1);
}

TEST_F(OriginTrialContextTest, OnlyOneErrorMessageGenerated)
{
    String errorMessage1;
    String errorMessage2;
    tokenValidator()->setResponse(WebOriginTrialTokenStatus::NotSupported);
    isFeatureEnabled(kFrobulateEnabledOrigin, kFrobulateFeatureName, &errorMessage1);

    // After the first call, should be one sample recorded for both the enabled
    // and message generated metrics
    expectMessageUniqueMetric(kMessageHistogramValueYes, 1);
    expectEnabledUniqueMetric(WebOriginTrialTokenStatus::NotSupported, 1);

    isFeatureEnabled(kFrobulateEnabledOrigin, kFrobulateFeatureName, &errorMessage2);
    EXPECT_FALSE(errorMessage1.isEmpty());
    EXPECT_TRUE(errorMessage2.isEmpty());

    // Should only be one sample recorded for the enabled metric, but two
    // samples for message generation.
    expectEnabledTotalMetric(1);
    expectMessageTotalMetric(2);
}

TEST_F(OriginTrialContextTest, ErrorMessageClearedIfStringReused)
{
    String errorMessage;
    tokenValidator()->setResponse(WebOriginTrialTokenStatus::NotSupported);
    isFeatureEnabled(kFrobulateEnabledOrigin, kFrobulateFeatureName, &errorMessage);
    EXPECT_FALSE(errorMessage.isEmpty());
    isFeatureEnabled(kFrobulateEnabledOrigin, kFrobulateFeatureName, &errorMessage);
    EXPECT_TRUE(errorMessage.isEmpty());
}

TEST_F(OriginTrialContextTest, ErrorMessageGeneratedPerFeature)
{
    String errorMessage1;
    String errorMessage2;
    tokenValidator()->setResponse(WebOriginTrialTokenStatus::NotSupported);
    isFeatureEnabled(kFrobulateEnabledOrigin, kFrobulateFeatureName, &errorMessage1);
    isFeatureEnabled(kFrobulateEnabledOrigin, kNonExistingFeatureName, &errorMessage2);
    EXPECT_FALSE(errorMessage1.isEmpty());
    EXPECT_FALSE(errorMessage2.isEmpty());

    // Enabled and message generated metrics should have same number of samples
    // as different features used for each call.
    expectEnabledUniqueMetric(WebOriginTrialTokenStatus::NotSupported, 2);
    expectMessageUniqueMetric(kMessageHistogramValueYes, 2);
}

TEST_F(OriginTrialContextTest, EnabledSecureRegisteredOriginWithoutErrorMessage)
{
    tokenValidator()->setResponse(WebOriginTrialTokenStatus::Success);
    bool isOriginEnabled = isFeatureEnabledWithoutErrorMessage(
        kFrobulateEnabledOrigin,
        kFrobulateFeatureName);
    EXPECT_TRUE(isOriginEnabled);
    EXPECT_EQ(1, tokenValidator()->callCount());
}

// The feature should not be enabled if the origin is unsecure, even if a valid
// token for the origin is provided
TEST_F(OriginTrialContextTest, EnabledNonSecureRegisteredOrigin)
{
    String errorMessage;
    bool isOriginEnabled = isFeatureEnabled(kFrobulateEnabledOriginUnsecure,
        kFrobulateFeatureName,
        &errorMessage);
    EXPECT_FALSE(isOriginEnabled);
    EXPECT_EQ(0, tokenValidator()->callCount());
    EXPECT_FALSE(errorMessage.isEmpty());
    expectEnabledUniqueMetric(WebOriginTrialTokenStatus::Insecure, 1);
}

TEST_F(OriginTrialContextTest, EnabledNonSecureRegisteredOriginWithoutErrorMessage)
{
    bool isOriginEnabled = isFeatureEnabledWithoutErrorMessage(
        kFrobulateEnabledOriginUnsecure,
        kFrobulateFeatureName);
    EXPECT_FALSE(isOriginEnabled);
    EXPECT_EQ(0, tokenValidator()->callCount());
    expectMessageUniqueMetric(kMessageHistogramValueNotRequested, 1);
}

TEST_F(OriginTrialContextTest, ParseHeaderValue)
{
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

    ASSERT_TRUE(tokens = OriginTrialContext::parseHeaderValue(",foo, ,bar,,'  ', ''"));
    ASSERT_EQ(3u, tokens->size());
    EXPECT_EQ("foo", (*tokens)[0]);
    EXPECT_EQ("bar", (*tokens)[1]);
    EXPECT_EQ("  ", (*tokens)[2]);

    ASSERT_TRUE(tokens = OriginTrialContext::parseHeaderValue("  \"abc\"  , 'def',g"));
    ASSERT_EQ(3u, tokens->size());
    EXPECT_EQ("abc", (*tokens)[0]);
    EXPECT_EQ("def", (*tokens)[1]);
    EXPECT_EQ("g", (*tokens)[2]);

    ASSERT_TRUE(tokens = OriginTrialContext::parseHeaderValue(" \"a\\b\\\"c'd\", 'e\\f\\'g' "));
    ASSERT_EQ(2u, tokens->size());
    EXPECT_EQ("ab\"c'd", (*tokens)[0]);
    EXPECT_EQ("ef'g", (*tokens)[1]);

    ASSERT_TRUE(tokens = OriginTrialContext::parseHeaderValue("\"ab,c\" , 'd,e'"));
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

TEST_F(OriginTrialContextTest, ParseHeaderValue_NotCommaSeparated)
{
    EXPECT_FALSE(OriginTrialContext::parseHeaderValue("foo bar"));
    EXPECT_FALSE(OriginTrialContext::parseHeaderValue("\"foo\" 'bar'"));
    EXPECT_FALSE(OriginTrialContext::parseHeaderValue("foo 'bar'"));
    EXPECT_FALSE(OriginTrialContext::parseHeaderValue("\"foo\" bar"));
}

} // namespace blink
