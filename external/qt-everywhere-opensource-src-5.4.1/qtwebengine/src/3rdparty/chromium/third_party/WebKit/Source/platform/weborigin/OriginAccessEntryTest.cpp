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
#include "platform/weborigin/OriginAccessEntry.h"

#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "public/platform/WebPublicSuffixList.h"
#include <gtest/gtest.h>

using WebCore::SecurityOrigin;
using WebCore::OriginAccessEntry;

namespace {

class OriginAccessEntryTestSuffixList : public blink::WebPublicSuffixList {
public:
    virtual size_t getPublicSuffixLength(const blink::WebString&)
    {
        return 3; // e.g. "com"
    }
};

class OriginAccessEntryTestPlatform : public blink::Platform {
public:
    virtual blink::WebPublicSuffixList* publicSuffixList()
    {
        return &m_suffixList;
    }

    // Stub for pure virtual method.
    virtual void cryptographicallyRandomValues(unsigned char*, size_t) { ASSERT_NOT_REACHED(); }

private:
    OriginAccessEntryTestSuffixList m_suffixList;
};

TEST(OriginAccessEntryTest, PublicSuffixListTest)
{
    OriginAccessEntryTestPlatform platform;
    blink::Platform::initialize(&platform);

    RefPtr<SecurityOrigin> origin = SecurityOrigin::createFromString("http://www.google.com");
    OriginAccessEntry entry1("http", "google.com", OriginAccessEntry::AllowSubdomains, OriginAccessEntry::TreatIPAddressAsIPAddress);
    OriginAccessEntry entry2("http", "hamster.com", OriginAccessEntry::AllowSubdomains, OriginAccessEntry::TreatIPAddressAsIPAddress);
    OriginAccessEntry entry3("http", "com", OriginAccessEntry::AllowSubdomains, OriginAccessEntry::TreatIPAddressAsIPAddress);
    EXPECT_EQ(OriginAccessEntry::MatchesOrigin, entry1.matchesOrigin(*origin));
    EXPECT_EQ(OriginAccessEntry::DoesNotMatchOrigin, entry2.matchesOrigin(*origin));
    EXPECT_EQ(OriginAccessEntry::MatchesOriginButIsPublicSuffix, entry3.matchesOrigin(*origin));

    blink::Platform::shutdown();
}

} // namespace

