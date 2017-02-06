/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "wtf/text/StringImpl.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/WTFString.h"

namespace WTF {

TEST(StringImplTest, Create8Bit)
{
    RefPtr<StringImpl> testStringImpl = StringImpl::create("1224");
    EXPECT_TRUE(testStringImpl->is8Bit());
}

TEST(StringImplTest, Latin1CaseFoldTable)
{
    LChar symbol = 0xff;
    while (symbol--) {
        EXPECT_EQ(Unicode::foldCase(symbol), StringImpl::latin1CaseFoldTable[symbol]);
    }
}

TEST(StringImplTest, LowerASCII)
{
    RefPtr<StringImpl> testStringImpl = StringImpl::create("link");
    EXPECT_TRUE(testStringImpl->is8Bit());
    EXPECT_TRUE(StringImpl::create("a\xE1")->is8Bit());

    EXPECT_TRUE(equal(testStringImpl.get(), StringImpl::create("link")->lowerASCII().get()));
    EXPECT_TRUE(equal(testStringImpl.get(), StringImpl::create("LINK")->lowerASCII().get()));
    EXPECT_TRUE(equal(testStringImpl.get(), StringImpl::create("lInk")->lowerASCII().get()));

    EXPECT_TRUE(equal(StringImpl::create("LINK")->lower().get(), StringImpl::create("LINK")->lowerASCII().get()));
    EXPECT_TRUE(equal(StringImpl::create("lInk")->lower().get(), StringImpl::create("lInk")->lowerASCII().get()));

    EXPECT_TRUE(equal(StringImpl::create("a\xE1").get(), StringImpl::create("A\xE1")->lowerASCII().get()));
    EXPECT_TRUE(equal(StringImpl::create("a\xC1").get(), StringImpl::create("A\xC1")->lowerASCII().get()));

    EXPECT_FALSE(equal(StringImpl::create("a\xE1").get(), StringImpl::create("a\xC1")->lowerASCII().get()));
    EXPECT_FALSE(equal(StringImpl::create("A\xE1").get(), StringImpl::create("A\xC1")->lowerASCII().get()));

    static const UChar test[5] = { 0x006c, 0x0069, 0x006e, 0x006b, 0 }; // link
    static const UChar testCapitalized[5] = { 0x004c, 0x0049, 0x004e, 0x004b, 0 }; // LINK

    RefPtr<StringImpl> testStringImpl16 = StringImpl::create(test, 4);
    EXPECT_FALSE(testStringImpl16->is8Bit());

    EXPECT_TRUE(equal(testStringImpl16.get(), StringImpl::create(test, 4)->lowerASCII().get()));
    EXPECT_TRUE(equal(testStringImpl16.get(), StringImpl::create(testCapitalized, 4)->lowerASCII().get()));

    static const UChar testWithNonASCII[3] = { 0x0061, 0x00e1, 0 }; // a\xE1
    static const UChar testWithNonASCIIComparison[3] = { 0x0061, 0x00c1, 0 }; // a\xC1
    static const UChar testWithNonASCIICapitalized[3] = { 0x0041, 0x00e1, 0 }; // A\xE1

    EXPECT_TRUE(equal(StringImpl::create(testWithNonASCII, 2).get(), StringImpl::create(testWithNonASCIICapitalized, 2)->lowerASCII().get()));
    EXPECT_FALSE(equal(StringImpl::create(testWithNonASCII, 2).get(), StringImpl::create(testWithNonASCIIComparison, 2)->lowerASCII().get()));
}

} // namespace WTF
