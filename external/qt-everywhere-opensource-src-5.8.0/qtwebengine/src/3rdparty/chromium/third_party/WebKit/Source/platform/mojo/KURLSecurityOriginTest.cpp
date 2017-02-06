// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/mojo/url_test.mojom-blink.h"
#include "url/url_constants.h"

namespace blink {
namespace {

class UrlTestImpl : public url::mojom::blink::UrlTest {
public:
    explicit UrlTestImpl(url::mojom::blink::UrlTestRequest request)
        : m_binding(this, std::move(request))
    {
    }

    // UrlTest:
    void BounceUrl(const KURL& in, const BounceUrlCallback& callback) override
    {
        callback.Run(in);
    }

    void BounceOrigin(const RefPtr<SecurityOrigin>& in,
        const BounceOriginCallback& callback) override
    {
        callback.Run(in);
    }

private:
    mojo::Binding<UrlTest> m_binding;
};

} // namespace

// Mojo version of chrome IPC test in url/ipc/url_param_traits_unittest.cc.
TEST(KURLSecurityOriginStructTraitsTest, Basic)
{
    base::MessageLoop messageLoop;

    url::mojom::blink::UrlTestPtr proxy;
    UrlTestImpl impl(GetProxy(&proxy));

    const char* serializeCases[] = {
        "http://www.google.com/",
        "http://user:pass@host.com:888/foo;bar?baz#nop",
    };

    for (const char* testCase : serializeCases) {
        KURL input(KURL(), testCase);
        KURL output;
        EXPECT_TRUE(proxy->BounceUrl(input, &output));

        // We want to test each component individually to make sure its range was
        // correctly serialized and deserialized, not just the spec.
        EXPECT_EQ(input.getString(), output.getString());
        EXPECT_EQ(input.isValid(), output.isValid());
        EXPECT_EQ(input.protocol(), output.protocol());
        EXPECT_EQ(input.user(), output.user());
        EXPECT_EQ(input.pass(), output.pass());
        EXPECT_EQ(input.host(), output.host());
        EXPECT_EQ(input.port(), output.port());
        EXPECT_EQ(input.path(), output.path());
        EXPECT_EQ(input.query(), output.query());
        EXPECT_EQ(input.fragmentIdentifier(), output.fragmentIdentifier());
    }

    // Test an excessively long GURL.
    {
        const std::string url = std::string("http://example.org/").append(url::kMaxURLChars + 1, 'a');
        KURL input(KURL(), url.c_str());
        KURL output;
        EXPECT_TRUE(proxy->BounceUrl(input, &output));
        EXPECT_TRUE(output.isEmpty());
    }

    // Test basic Origin serialization.
    RefPtr<SecurityOrigin> nonUnique = SecurityOrigin::create("http", "www.google.com", 80);
    RefPtr<SecurityOrigin> output;
    EXPECT_TRUE(proxy->BounceOrigin(nonUnique, &output));
    EXPECT_TRUE(nonUnique->isSameSchemeHostPort(output.get()));
    EXPECT_FALSE(nonUnique->isUnique());

    RefPtr<SecurityOrigin> unique = SecurityOrigin::createUnique();
    EXPECT_TRUE(proxy->BounceOrigin(unique, &output));
    EXPECT_TRUE(output->isUnique());
}

} // namespace url
