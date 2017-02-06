// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/DocumentWriteEvaluator.h"

#include "bindings/core/v8/V8Binding.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class DocumentWriteEvaluatorTest : public ::testing::Test {
public:
    DocumentWriteEvaluatorTest()
        : m_evaluator(new DocumentWriteEvaluator("/path/", "www.example.com", "http:", "userAgent"))
    {
        m_evaluator->ensureEvaluationContext();
    }
    std::unique_ptr<DocumentWriteEvaluator> m_evaluator;
};

} // namespace

TEST_F(DocumentWriteEvaluatorTest, NoEvaluation)
{
    String written = m_evaluator->evaluateAndEmitWrittenSource(
        "var a = 2;");
    EXPECT_EQ("", written);
}

TEST_F(DocumentWriteEvaluatorTest, SimpleDocumentWrite)
{
    String written = m_evaluator->evaluateAndEmitWrittenSource(
        "document.write('Hello, World!');");
    EXPECT_EQ("Hello, World!", written);
}

TEST_F(DocumentWriteEvaluatorTest, WriteBeforeError)
{
    String written = m_evaluator->evaluateAndEmitWrittenSource(
        "document.write('Hello, World!');"
        "console.log('this causes an exception');");
    EXPECT_EQ("Hello, World!", written);
}

TEST_F(DocumentWriteEvaluatorTest, MultipleWrites)
{
    String written = m_evaluator->evaluateAndEmitWrittenSource(
        "document.write('Hello, World', '!');"
        "window.document.write('How' + ' are you?');"
        "document.writeln('Not bad.');");
    EXPECT_EQ("Hello, World!How are you?Not bad.", written);
}

TEST_F(DocumentWriteEvaluatorTest, HandleSimpleFunctions)
{
    String written = m_evaluator->evaluateAndEmitWrittenSource(
        "(function(src) {"
        "document.write(src);"
        "})('Hello, World!');");
    EXPECT_EQ("Hello, World!", written);
}

TEST_F(DocumentWriteEvaluatorTest, DynamicDocWrite)
{
    String written = m_evaluator->evaluateAndEmitWrittenSource(
        "var write = document.write;"
        "(function(f, w) {"
        "f(w);"
        "})(write, 'Hello, World!');");
    EXPECT_EQ("Hello, World!", written);
}

TEST_F(DocumentWriteEvaluatorTest, MultipleScripts)
{
    String written = m_evaluator->evaluateAndEmitWrittenSource(
        "var write = document.write;"
        "write('Hello');");
    EXPECT_EQ("Hello", written);

    String written2 = m_evaluator->evaluateAndEmitWrittenSource(
        "write('Hello');");
    EXPECT_EQ("Hello", written2);
}

TEST_F(DocumentWriteEvaluatorTest, UsePath)
{
    String written = m_evaluator->evaluateAndEmitWrittenSource(
        "document.write(location.pathname);"
        "document.write(' ', window.location.pathname);");
    EXPECT_EQ("/path/ /path/", written);
}

TEST_F(DocumentWriteEvaluatorTest, UseHost)
{
    String written = m_evaluator->evaluateAndEmitWrittenSource(
        "document.write(location.hostname);"
        "document.write(' ', window.location.hostname);");
    EXPECT_EQ("www.example.com www.example.com", written);
}

TEST_F(DocumentWriteEvaluatorTest, UseProtocol)
{
    String written = m_evaluator->evaluateAndEmitWrittenSource(
        "document.write(location.protocol);"
        "document.write(' ', window.location.protocol);");
    EXPECT_EQ("http: http:", written);
}

TEST_F(DocumentWriteEvaluatorTest, UseUserAgent)
{
    String written = m_evaluator->evaluateAndEmitWrittenSource(
        "document.write(navigator.userAgent);"
        "document.write(' ', window.navigator.userAgent);");
    EXPECT_EQ("userAgent userAgent", written);
}

} // namespace blink
