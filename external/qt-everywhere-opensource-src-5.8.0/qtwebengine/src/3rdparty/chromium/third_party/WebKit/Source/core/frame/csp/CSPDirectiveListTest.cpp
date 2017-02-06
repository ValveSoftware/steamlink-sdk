// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/csp/CSPDirectiveList.h"

#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/frame/csp/SourceListDirective.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/network/ResourceRequest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/StringOperators.h"
#include "wtf/text/WTFString.h"

namespace blink {

class CSPDirectiveListTest : public ::testing::Test {
public:
    CSPDirectiveListTest()
        : csp(ContentSecurityPolicy::create())
    {
    }

    CSPDirectiveList* createList(const String& list, ContentSecurityPolicyHeaderType type)
    {
        Vector<UChar> characters;
        list.appendTo(characters);
        const UChar* begin = characters.data();
        const UChar* end = begin + characters.size();

        return CSPDirectiveList::create(csp, begin, end, type, ContentSecurityPolicyHeaderSourceHTTP);
    }

protected:
    Persistent<ContentSecurityPolicy> csp;
};

TEST_F(CSPDirectiveListTest, IsMatchingNoncePresent)
{
    struct TestCase {
        const char* list;
        const char* nonce;
        bool expected;
    } cases[] = {
        { "script-src 'self'", "yay", false },
        { "script-src 'self'", "boo", false },
        { "script-src 'nonce-yay'", "yay", true },
        { "script-src 'nonce-yay'", "boo", false },
        { "script-src 'nonce-yay' 'nonce-boo'", "yay", true },
        { "script-src 'nonce-yay' 'nonce-boo'", "boo", true },

        // Falls back to 'default-src'
        { "default-src 'nonce-yay'", "yay", true },
        { "default-src 'nonce-yay'", "boo", false },
        { "default-src 'nonce-boo'; script-src 'nonce-yay'", "yay", true },
        { "default-src 'nonce-boo'; script-src 'nonce-yay'", "boo", false },

        // Unrelated directives do not affect result
        { "style-src 'nonce-yay'", "yay", false },
        { "style-src 'nonce-yay'", "boo", false },
    };

    for (const auto& test : cases) {
        // Report-only
        Member<CSPDirectiveList> directiveList = createList(test.list, ContentSecurityPolicyHeaderTypeReport);
        Member<SourceListDirective> scriptSrc = directiveList->operativeDirective(directiveList->m_scriptSrc.get());
        EXPECT_EQ(test.expected, directiveList->isMatchingNoncePresent(scriptSrc, test.nonce));
        // Empty/null strings are always not present, regardless of the policy.
        EXPECT_FALSE(directiveList->isMatchingNoncePresent(scriptSrc, ""));
        EXPECT_FALSE(directiveList->isMatchingNoncePresent(scriptSrc, String()));

        // Enforce
        directiveList = createList(test.list, ContentSecurityPolicyHeaderTypeEnforce);
        scriptSrc = directiveList->operativeDirective(directiveList->m_scriptSrc.get());
        EXPECT_EQ(test.expected, directiveList->isMatchingNoncePresent(scriptSrc, test.nonce));
        // Empty/null strings are always not present, regardless of the policy.
        EXPECT_FALSE(directiveList->isMatchingNoncePresent(scriptSrc, ""));
        EXPECT_FALSE(directiveList->isMatchingNoncePresent(scriptSrc, String()));
    }
}

TEST_F(CSPDirectiveListTest, AllowScriptFromSourceNoNonce)
{
    struct TestCase {
        const char* list;
        const char* url;
        bool expected;
    } cases[] = {
        { "script-src https://example.com", "https://example.com/script.js", true },
        { "script-src https://example.com/", "https://example.com/script.js", true },
        { "script-src https://example.com/", "https://example.com/script/script.js", true },
        { "script-src https://example.com/script", "https://example.com/script.js", false },
        { "script-src https://example.com/script", "https://example.com/script/script.js", false },
        { "script-src https://example.com/script/", "https://example.com/script.js", false },
        { "script-src https://example.com/script/", "https://example.com/script/script.js", true },
        { "script-src https://example.com", "https://not.example.com/script.js", false },
        { "script-src https://*.example.com", "https://not.example.com/script.js", true },
        { "script-src https://*.example.com", "https://example.com/script.js", false },

        // Falls back to default-src:
        { "default-src https://example.com", "https://example.com/script.js", true },
        { "default-src https://example.com/", "https://example.com/script.js", true },
        { "default-src https://example.com/", "https://example.com/script/script.js", true },
        { "default-src https://example.com/script", "https://example.com/script.js", false },
        { "default-src https://example.com/script", "https://example.com/script/script.js", false },
        { "default-src https://example.com/script/", "https://example.com/script.js", false },
        { "default-src https://example.com/script/", "https://example.com/script/script.js", true },
        { "default-src https://example.com", "https://not.example.com/script.js", false },
        { "default-src https://*.example.com", "https://not.example.com/script.js", true },
        { "default-src https://*.example.com", "https://example.com/script.js", false },
    };

    for (const auto& test : cases) {
        SCOPED_TRACE(testing::Message() << "List: `" << test.list << "`, URL: `" << test.url << "`");
        KURL scriptSrc = KURL(KURL(), test.url);

        // Report-only
        Member<CSPDirectiveList> directiveList = createList(test.list, ContentSecurityPolicyHeaderTypeReport);
        EXPECT_EQ(test.expected, directiveList->allowScriptFromSource(scriptSrc, String(), ResourceRequest::RedirectStatus::NoRedirect, ContentSecurityPolicy::SuppressReport));

        // Enforce
        directiveList = createList(test.list, ContentSecurityPolicyHeaderTypeEnforce);
        EXPECT_EQ(test.expected, directiveList->allowScriptFromSource(scriptSrc, String(), ResourceRequest::RedirectStatus::NoRedirect, ContentSecurityPolicy::SuppressReport));
    }
}

TEST_F(CSPDirectiveListTest, AllowFromSourceWithNonce)
{
    struct TestCase {
        const char* list;
        const char* url;
        const char* nonce;
        bool expected;
    } cases[] = {
        // Doesn't affect lists without nonces:
        { "https://example.com", "https://example.com/file", "yay", true },
        { "https://example.com", "https://example.com/file", "boo", true },
        { "https://example.com", "https://example.com/file", "", true },
        { "https://example.com", "https://not.example.com/file", "yay", false },
        { "https://example.com", "https://not.example.com/file", "boo", false },
        { "https://example.com", "https://not.example.com/file", "", false },

        // Doesn't affect URLs that match the whitelist.
        { "https://example.com 'nonce-yay'", "https://example.com/file", "yay", true },
        { "https://example.com 'nonce-yay'", "https://example.com/file", "boo", true },
        { "https://example.com 'nonce-yay'", "https://example.com/file", "", true },

        // Does affect URLs that don't.
        { "https://example.com 'nonce-yay'", "https://not.example.com/file", "yay", true },
        { "https://example.com 'nonce-yay'", "https://not.example.com/file", "boo", false },
        { "https://example.com 'nonce-yay'", "https://not.example.com/file", "", false },
    };

    for (const auto& test : cases) {
        SCOPED_TRACE(testing::Message() << "List: `" << test.list << "`, URL: `" << test.url << "`");
        KURL resource = KURL(KURL(), test.url);

        // Report-only 'script-src'
        Member<CSPDirectiveList> directiveList = createList(String("script-src ") + test.list, ContentSecurityPolicyHeaderTypeReport);
        EXPECT_EQ(test.expected, directiveList->allowScriptFromSource(resource, String(test.nonce), ResourceRequest::RedirectStatus::NoRedirect, ContentSecurityPolicy::SuppressReport));

        // Enforce 'script-src'
        directiveList = createList(String("script-src ") + test.list, ContentSecurityPolicyHeaderTypeEnforce);
        EXPECT_EQ(test.expected, directiveList->allowScriptFromSource(resource, String(test.nonce), ResourceRequest::RedirectStatus::NoRedirect, ContentSecurityPolicy::SuppressReport));

        // Report-only 'style-src'
        directiveList = createList(String("style-src ") + test.list, ContentSecurityPolicyHeaderTypeReport);
        EXPECT_EQ(test.expected, directiveList->allowStyleFromSource(resource, String(test.nonce), ResourceRequest::RedirectStatus::NoRedirect, ContentSecurityPolicy::SuppressReport));

        // Enforce 'style-src'
        directiveList = createList(String("style-src ") + test.list, ContentSecurityPolicyHeaderTypeEnforce);
        EXPECT_EQ(test.expected, directiveList->allowStyleFromSource(resource, String(test.nonce), ResourceRequest::RedirectStatus::NoRedirect, ContentSecurityPolicy::SuppressReport));

        // Report-only 'style-src'
        directiveList = createList(String("default-src ") + test.list, ContentSecurityPolicyHeaderTypeReport);
        EXPECT_EQ(test.expected, directiveList->allowScriptFromSource(resource, String(test.nonce), ResourceRequest::RedirectStatus::NoRedirect, ContentSecurityPolicy::SuppressReport));
        EXPECT_EQ(test.expected, directiveList->allowStyleFromSource(resource, String(test.nonce), ResourceRequest::RedirectStatus::NoRedirect, ContentSecurityPolicy::SuppressReport));

        // Enforce 'style-src'
        directiveList = createList(String("default-src ") + test.list, ContentSecurityPolicyHeaderTypeEnforce);
        EXPECT_EQ(test.expected, directiveList->allowScriptFromSource(resource, String(test.nonce), ResourceRequest::RedirectStatus::NoRedirect, ContentSecurityPolicy::SuppressReport));
        EXPECT_EQ(test.expected, directiveList->allowStyleFromSource(resource, String(test.nonce), ResourceRequest::RedirectStatus::NoRedirect, ContentSecurityPolicy::SuppressReport));
    }
}

} // namespace blink
