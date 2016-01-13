/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "platform/weborigin/SecurityOrigin.h"

#include "platform/weborigin/KURL.h"
#include <gtest/gtest.h>

using WebCore::SecurityOrigin;

namespace {

const int MaxAllowedPort = 65535;

TEST(SecurityOriginTest, InvalidPortsCreateUniqueOrigins)
{
    int ports[] = { -100, -1, MaxAllowedPort + 1, 1000000 };

    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(ports); ++i) {
        RefPtr<SecurityOrigin> origin = SecurityOrigin::create("http", "example.com", ports[i]);
        EXPECT_TRUE(origin->isUnique()) << "Port " << ports[i] << " should have generated a unique origin.";
    }
}

TEST(SecurityOriginTest, ValidPortsCreateNonUniqueOrigins)
{
    int ports[] = { 0, 80, 443, 5000, MaxAllowedPort };

    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(ports); ++i) {
        RefPtr<SecurityOrigin> origin = SecurityOrigin::create("http", "example.com", ports[i]);
        EXPECT_FALSE(origin->isUnique()) << "Port " << ports[i] << " should not have generated a unique origin.";
    }
}

TEST(SecurityOriginTest, CanAccessFeatureRequringSecureOrigin)
{
    struct TestCase {
        bool accessGranted;
        const char* url;
    };

    TestCase inputs[] = {
        // Access is granted to webservers running on localhost.
        { true, "http://localhost" },
        { true, "http://LOCALHOST" },
        { true, "http://localhost:100" },
        { true, "http://127.0.0.1" },
        { true, "http://127.0.0.2" },
        { true, "http://127.1.0.2" },
        { true, "http://0177.00.00.01" },
        { true, "http://[::1]" },
        { true, "http://[0:0::1]" },
        { true, "http://[0:0:0:0:0:0:0:1]" },
        { true, "http://[::1]:21" },
        { true, "http://127.0.0.1:8080" },
        { true, "ftp://127.0.0.1" },
        { true, "ftp://127.0.0.1:443" },
        { true, "ws://127.0.0.1" },

        // Access is denied to non-localhost over HTTP
        { false, "http://[1::]" },
        { false, "http://[::2]" },
        { false, "http://[1::1]" },
        { false, "http://[1:2::3]" },
        { false, "http://[::127.0.0.1]" },
        { false, "http://a.127.0.0.1" },
        { false, "http://127.0.0.1.b" },
        { false, "http://localhost.a" },
        { false, "http://a.localhost" },

        // Access is granted to all secure transports.
        { true, "https://foobar.com" },
        { true, "wss://foobar.com" },

        // Access is denied to insecure transports.
        { false, "ftp://foobar.com" },
        { false, "http://foobar.com" },
        { false, "http://foobar.com:443" },
        { false, "ws://foobar.com" },

        // Access is granted to local files
        { true, "file:///home/foobar/index.html" },

        // blob: URLs must look to the inner URL's origin, and apply the same
        // rules as above. Spot check some of them
        { true, "blob:http://localhost:1000/578223a1-8c13-17b3-84d5-eca045ae384a" },
        { true, "blob:https://foopy:99/578223a1-8c13-17b3-84d5-eca045ae384a" },
        { false, "blob:http://baz:99/578223a1-8c13-17b3-84d5-eca045ae384a" },
        { false, "blob:ftp://evil:99/578223a1-8c13-17b3-84d5-eca045ae384a" },

        // filesystem: URLs work the same as blob: URLs, and look to the inner
        // URL for security origin.
        { true, "filesystem:http://localhost:1000/foo" },
        { true, "filesystem:https://foopy:99/foo" },
        { false, "filesystem:http://baz:99/foo" },
        { false, "filesystem:ftp://evil:99/foo" },
    };

    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(inputs); ++i) {
        SCOPED_TRACE(i);
        RefPtr<SecurityOrigin> origin = SecurityOrigin::createFromString(inputs[i].url);
        EXPECT_EQ(inputs[i].accessGranted, origin->canAccessFeatureRequiringSecureOrigin());
    }

    // Unique origins are not considered secure.
    RefPtr<SecurityOrigin> uniqueOrigin = SecurityOrigin::createUnique();
    EXPECT_FALSE(uniqueOrigin->canAccessFeatureRequiringSecureOrigin());
}

} // namespace

