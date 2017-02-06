// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/V8ScriptRunner.h"

#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/fetch/CachedMetadataHandler.h"
#include "core/fetch/ScriptResource.h"
#include "platform/heap/Handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <v8.h>

namespace blink {

namespace {

class V8ScriptRunnerTest : public ::testing::Test {
public:
    V8ScriptRunnerTest() { }
    ~V8ScriptRunnerTest() override { }

    void SetUp() override
    {
        // To trick various layers of caching, increment a counter for each
        // test and use it in code(), fielname() and url().
        counter++;
    }

    void TearDown() override
    {
        m_resourceRequest = ResourceRequest();
        m_resource.clear();
    }

    WTF::String code() const
    {
        // Simple function for testing. Note:
        // - Add counter to trick V8 code cache.
        // - Pad counter to 1000 digits, to trick minimal cacheability threshold.
        return WTF::String::format("a = function() { 1 + 1; } // %01000d\n", counter);
    }
    WTF::String filename() const
    {
        return WTF::String::format("whatever%d.js", counter);
    }
    WTF::String url() const
    {
        return WTF::String::format("http://bla.com/bla%d", counter);
    }
    unsigned tagForParserCache(CachedMetadataHandler* cacheHandler) const
    {
        return V8ScriptRunner::tagForParserCache(cacheHandler);
    }
    unsigned tagForCodeCache(CachedMetadataHandler* cacheHandler) const
    {
        return V8ScriptRunner::tagForCodeCache(cacheHandler);
    }
    void setCacheTimeStamp(CachedMetadataHandler* cacheHandler)
    {
        V8ScriptRunner::setCacheTimeStamp(cacheHandler);
    }

    bool compileScript(v8::Isolate* isolate, V8CacheOptions cacheOptions)
    {
        return !V8ScriptRunner::compileScript(
            v8String(isolate, code()), filename(), String(), WTF::TextPosition(),
            isolate, m_resource.get(), nullptr, m_resource.get() ? m_resource->cacheHandler(): nullptr, NotSharableCrossOrigin, cacheOptions)
            .IsEmpty();
    }

    void setEmptyResource()
    {
        m_resourceRequest = ResourceRequest();
        m_resource = ScriptResource::create(m_resourceRequest, "UTF-8");
    }

    void setResource()
    {
        m_resourceRequest = ResourceRequest(url());
        m_resource = ScriptResource::create(m_resourceRequest, "UTF-8");
    }

    CachedMetadataHandler* cacheHandler()
    {
        return m_resource->cacheHandler();
    }

protected:
    ResourceRequest m_resourceRequest;
    Persistent<ScriptResource> m_resource;

    static int counter;
};

int V8ScriptRunnerTest::counter = 0;

TEST_F(V8ScriptRunnerTest, resourcelessShouldPass)
{
    V8TestingScope scope;
    EXPECT_TRUE(compileScript(scope.isolate(), V8CacheOptionsNone));
    EXPECT_TRUE(compileScript(scope.isolate(), V8CacheOptionsParse));
    EXPECT_TRUE(compileScript(scope.isolate(), V8CacheOptionsCode));
}

TEST_F(V8ScriptRunnerTest, emptyResourceDoesNotHaveCacheHandler)
{
    setEmptyResource();
    EXPECT_FALSE(cacheHandler());
}

TEST_F(V8ScriptRunnerTest, parseOption)
{
    V8TestingScope scope;
    setResource();
    EXPECT_TRUE(compileScript(scope.isolate(), V8CacheOptionsParse));
    EXPECT_TRUE(cacheHandler()->cachedMetadata(tagForParserCache(cacheHandler())));
    EXPECT_FALSE(cacheHandler()->cachedMetadata(tagForCodeCache(cacheHandler())));
    // The cached data is associated with the encoding.
    ResourceRequest request(url());
    ScriptResource* anotherResource = ScriptResource::create(request, "UTF-16");
    EXPECT_FALSE(cacheHandler()->cachedMetadata(tagForParserCache(anotherResource->cacheHandler())));
}

TEST_F(V8ScriptRunnerTest, codeOption)
{
    V8TestingScope scope;
    setResource();
    setCacheTimeStamp(cacheHandler());

    EXPECT_TRUE(compileScript(scope.isolate(), V8CacheOptionsCode));

    EXPECT_FALSE(cacheHandler()->cachedMetadata(tagForParserCache(cacheHandler())));
    EXPECT_TRUE(cacheHandler()->cachedMetadata(tagForCodeCache(cacheHandler())));
    // The cached data is associated with the encoding.
    ResourceRequest request(url());
    ScriptResource* anotherResource = ScriptResource::create(request, "UTF-16");
    EXPECT_FALSE(cacheHandler()->cachedMetadata(tagForCodeCache(anotherResource->cacheHandler())));
}

} // namespace

} // namespace blink
