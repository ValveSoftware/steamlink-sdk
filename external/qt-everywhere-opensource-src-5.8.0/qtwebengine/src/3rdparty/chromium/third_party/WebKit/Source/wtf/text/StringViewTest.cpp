// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wtf/text/StringView.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/StringImpl.h"
#include "wtf/text/WTFString.h"

namespace WTF {

const char* kChars = "12345";
const LChar* kChars8 = reinterpret_cast<const LChar*>(kChars);
const UChar* kChars16 = reinterpret_cast<const UChar*>(u"12345");

TEST(StringViewTest, ConstructionStringImpl8)
{
    RefPtr<StringImpl> impl8Bit = StringImpl::create(kChars8, 5);

    // StringView(StringImpl*);
    ASSERT_TRUE(StringView(impl8Bit.get()).is8Bit());
    EXPECT_FALSE(StringView(impl8Bit.get()).isNull());
    EXPECT_EQ(impl8Bit->characters8(), StringView(impl8Bit.get()).characters8());
    EXPECT_EQ(impl8Bit->length(), StringView(impl8Bit.get()).length());
    EXPECT_STREQ(kChars, StringView(impl8Bit.get()).toString().utf8().data());

    // StringView(StringImpl*, unsigned offset);
    ASSERT_TRUE(StringView(impl8Bit.get(), 2).is8Bit());
    EXPECT_FALSE(StringView(impl8Bit.get(), 2).isNull());
    EXPECT_EQ(impl8Bit->characters8() + 2, StringView(impl8Bit.get(), 2).characters8());
    EXPECT_EQ(3u, StringView(impl8Bit.get(), 2).length());
    EXPECT_EQ(StringView("345"), StringView(impl8Bit.get(), 2));
    EXPECT_STREQ("345", StringView(impl8Bit.get(), 2).toString().utf8().data());

    // StringView(StringImpl*, unsigned offset, unsigned length);
    ASSERT_TRUE(StringView(impl8Bit.get(), 2, 1).is8Bit());
    EXPECT_FALSE(StringView(impl8Bit.get(), 2, 1).isNull());
    EXPECT_EQ(impl8Bit->characters8() + 2, StringView(impl8Bit.get(), 2, 1).characters8());
    EXPECT_EQ(1u, StringView(impl8Bit.get(), 2, 1).length());
    EXPECT_EQ(StringView("3"), StringView(impl8Bit.get(), 2, 1));
    EXPECT_STREQ("3", StringView(impl8Bit.get(), 2, 1).toString().utf8().data());
}

TEST(StringViewTest, ConstructionStringImpl16)
{
    RefPtr<StringImpl> impl16Bit = StringImpl::create(kChars16, 5);

    // StringView(StringImpl*);
    ASSERT_FALSE(StringView(impl16Bit.get()).is8Bit());
    EXPECT_FALSE(StringView(impl16Bit.get()).isNull());
    EXPECT_EQ(impl16Bit->characters16(), StringView(impl16Bit.get()).characters16());
    EXPECT_EQ(impl16Bit->length(), StringView(impl16Bit.get()).length());
    EXPECT_STREQ(kChars, StringView(impl16Bit.get()).toString().utf8().data());

    // StringView(StringImpl*, unsigned offset);
    ASSERT_FALSE(StringView(impl16Bit.get(), 2).is8Bit());
    EXPECT_FALSE(StringView(impl16Bit.get(), 2).isNull());
    EXPECT_EQ(impl16Bit->characters16() + 2, StringView(impl16Bit.get(), 2).characters16());
    EXPECT_EQ(3u, StringView(impl16Bit.get(), 2).length());
    EXPECT_EQ(StringView("345"), StringView(impl16Bit.get(), 2));
    EXPECT_STREQ("345", StringView(impl16Bit.get(), 2).toString().utf8().data());

    // StringView(StringImpl*, unsigned offset, unsigned length);
    ASSERT_FALSE(StringView(impl16Bit.get(), 2, 1).is8Bit());
    EXPECT_FALSE(StringView(impl16Bit.get(), 2, 1).isNull());
    EXPECT_EQ(impl16Bit->characters16() + 2, StringView(impl16Bit.get(), 2, 1).characters16());
    EXPECT_EQ(1u, StringView(impl16Bit.get(), 2, 1).length());
    EXPECT_EQ(StringView("3"), StringView(impl16Bit.get(), 2, 1));
    EXPECT_STREQ("3", StringView(impl16Bit.get(), 2, 1).toString().utf8().data());
}

TEST(StringViewTest, ConstructionString8)
{
    String string8Bit = String(StringImpl::create(kChars8, 5));

    // StringView(const String&);
    ASSERT_TRUE(StringView(string8Bit).is8Bit());
    EXPECT_FALSE(StringView(string8Bit).isNull());
    EXPECT_EQ(string8Bit.characters8(), StringView(string8Bit).characters8());
    EXPECT_EQ(string8Bit.length(), StringView(string8Bit).length());
    EXPECT_STREQ(kChars, StringView(string8Bit).toString().utf8().data());

    // StringView(const String&, unsigned offset);
    ASSERT_TRUE(StringView(string8Bit, 2).is8Bit());
    EXPECT_FALSE(StringView(string8Bit, 2).isNull());
    EXPECT_EQ(string8Bit.characters8() + 2, StringView(string8Bit, 2).characters8());
    EXPECT_EQ(3u, StringView(string8Bit, 2).length());
    EXPECT_EQ(StringView("345"), StringView(string8Bit, 2));
    EXPECT_STREQ("345", StringView(string8Bit, 2).toString().utf8().data());

    // StringView(const String&, unsigned offset, unsigned length);
    ASSERT_TRUE(StringView(string8Bit, 2, 1).is8Bit());
    EXPECT_FALSE(StringView(string8Bit, 2, 1).isNull());
    EXPECT_EQ(string8Bit.characters8() + 2, StringView(string8Bit, 2, 1).characters8());
    EXPECT_EQ(1u, StringView(string8Bit, 2, 1).length());
    EXPECT_EQ(StringView("3"), StringView(string8Bit, 2, 1));
    EXPECT_STREQ("3", StringView(string8Bit, 2, 1).toString().utf8().data());
}

TEST(StringViewTest, ConstructionString16)
{
    String string16Bit = String(StringImpl::create(kChars16, 5));

    // StringView(const String&);
    ASSERT_FALSE(StringView(string16Bit).is8Bit());
    EXPECT_FALSE(StringView(string16Bit).isNull());
    EXPECT_EQ(string16Bit.characters16(), StringView(string16Bit).characters16());
    EXPECT_EQ(string16Bit.length(), StringView(string16Bit).length());
    EXPECT_STREQ(kChars, StringView(string16Bit).toString().utf8().data());

    // StringView(const String&, unsigned offset);
    ASSERT_FALSE(StringView(string16Bit, 2).is8Bit());
    EXPECT_FALSE(StringView(string16Bit, 2).isNull());
    EXPECT_EQ(string16Bit.characters16() + 2, StringView(string16Bit, 2).characters16());
    EXPECT_EQ(3u, StringView(string16Bit, 2).length());
    EXPECT_EQ(StringView("345"), StringView(string16Bit, 2));
    EXPECT_STREQ("345", StringView(string16Bit, 2).toString().utf8().data());

    // StringView(const String&, unsigned offset, unsigned length);
    ASSERT_FALSE(StringView(string16Bit, 2, 1).is8Bit());
    EXPECT_FALSE(StringView(string16Bit, 2, 1).isNull());
    EXPECT_EQ(string16Bit.characters16() + 2, StringView(string16Bit, 2, 1).characters16());
    EXPECT_EQ(1u, StringView(string16Bit, 2, 1).length());
    EXPECT_EQ(StringView("3"), StringView(string16Bit, 2, 1));
    EXPECT_STREQ("3", StringView(string16Bit, 2, 1).toString().utf8().data());
}

TEST(StringViewTest, ConstructionAtomicString8)
{
    AtomicString atom8Bit = AtomicString(StringImpl::create(kChars8, 5));

    // StringView(const AtomicString&);
    ASSERT_TRUE(StringView(atom8Bit).is8Bit());
    EXPECT_FALSE(StringView(atom8Bit).isNull());
    EXPECT_EQ(atom8Bit.characters8(), StringView(atom8Bit).characters8());
    EXPECT_EQ(atom8Bit.length(), StringView(atom8Bit).length());
    EXPECT_STREQ(kChars, StringView(atom8Bit).toString().utf8().data());

    // StringView(const AtomicString&, unsigned offset);
    ASSERT_TRUE(StringView(atom8Bit, 2).is8Bit());
    EXPECT_FALSE(StringView(atom8Bit, 2).isNull());
    EXPECT_EQ(atom8Bit.characters8() + 2, StringView(atom8Bit, 2).characters8());
    EXPECT_EQ(3u, StringView(atom8Bit, 2).length());
    EXPECT_EQ(StringView("345"), StringView(atom8Bit, 2));
    EXPECT_STREQ("345", StringView(atom8Bit, 2).toString().utf8().data());

    // StringView(const AtomicString&, unsigned offset, unsigned length);
    ASSERT_TRUE(StringView(atom8Bit, 2, 1).is8Bit());
    EXPECT_FALSE(StringView(atom8Bit, 2, 1).isNull());
    EXPECT_EQ(atom8Bit.characters8() + 2, StringView(atom8Bit, 2, 1).characters8());
    EXPECT_EQ(1u, StringView(atom8Bit, 2, 1).length());
    EXPECT_EQ(StringView("3"), StringView(atom8Bit, 2, 1));
    EXPECT_STREQ("3", StringView(atom8Bit, 2, 1).toString().utf8().data());
}

TEST(StringViewTest, ConstructionAtomicString16)
{
    AtomicString atom16Bit = AtomicString(StringImpl::create(kChars16, 5));

    // StringView(const AtomicString&);
    ASSERT_FALSE(StringView(atom16Bit).is8Bit());
    EXPECT_FALSE(StringView(atom16Bit).isNull());
    EXPECT_EQ(atom16Bit.characters16(), StringView(atom16Bit).characters16());
    EXPECT_EQ(atom16Bit.length(), StringView(atom16Bit).length());
    EXPECT_STREQ(kChars, StringView(atom16Bit).toString().utf8().data());

    // StringView(const AtomicString&, unsigned offset);
    ASSERT_FALSE(StringView(atom16Bit, 2).is8Bit());
    EXPECT_FALSE(StringView(atom16Bit, 2).isNull());
    EXPECT_EQ(atom16Bit.characters16() + 2, StringView(atom16Bit, 2).characters16());
    EXPECT_EQ(3u, StringView(atom16Bit, 2).length());
    EXPECT_EQ(StringView("345"), StringView(atom16Bit, 2));
    EXPECT_STREQ("345", StringView(atom16Bit, 2).toString().utf8().data());

    // StringView(const AtomicString&, unsigned offset, unsigned length);
    ASSERT_FALSE(StringView(atom16Bit, 2, 1).is8Bit());
    EXPECT_FALSE(StringView(atom16Bit, 2, 1).isNull());
    EXPECT_EQ(atom16Bit.characters16() + 2, StringView(atom16Bit, 2, 1).characters16());
    EXPECT_EQ(1u, StringView(atom16Bit, 2, 1).length());
    EXPECT_EQ(StringView("3"), StringView(atom16Bit, 2, 1));
    EXPECT_STREQ("3", StringView(atom16Bit, 2, 1).toString().utf8().data());
}

TEST(StringViewTest, ConstructionStringView8)
{
    StringView view8Bit = StringView(kChars8, 5);

    // StringView(StringView&);
    ASSERT_TRUE(StringView(view8Bit).is8Bit());
    EXPECT_FALSE(StringView(view8Bit).isNull());
    EXPECT_EQ(view8Bit.characters8(), StringView(view8Bit).characters8());
    EXPECT_EQ(view8Bit.length(), StringView(view8Bit).length());
    EXPECT_STREQ(kChars, StringView(view8Bit).toString().utf8().data());

    // StringView(const StringView&, unsigned offset);
    ASSERT_TRUE(StringView(view8Bit, 2).is8Bit());
    EXPECT_FALSE(StringView(view8Bit, 2).isNull());
    EXPECT_EQ(view8Bit.characters8() + 2, StringView(view8Bit, 2).characters8());
    EXPECT_EQ(3u, StringView(view8Bit, 2).length());
    EXPECT_EQ(StringView("345"), StringView(view8Bit, 2));
    EXPECT_STREQ("345", StringView(view8Bit, 2).toString().utf8().data());

    // StringView(const StringView&, unsigned offset, unsigned length);
    ASSERT_TRUE(StringView(view8Bit, 2, 1).is8Bit());
    EXPECT_FALSE(StringView(view8Bit, 2, 1).isNull());
    EXPECT_EQ(view8Bit.characters8() + 2, StringView(view8Bit, 2, 1).characters8());
    EXPECT_EQ(1u, StringView(view8Bit, 2, 1).length());
    EXPECT_EQ(StringView("3"), StringView(view8Bit, 2, 1));
    EXPECT_STREQ("3", StringView(view8Bit, 2, 1).toString().utf8().data());
}

TEST(StringViewTest, ConstructionStringView16)
{
    StringView view16Bit = StringView(kChars16, 5);

    // StringView(StringView&);
    ASSERT_FALSE(StringView(view16Bit).is8Bit());
    EXPECT_FALSE(StringView(view16Bit).isNull());
    EXPECT_EQ(view16Bit.characters16(), StringView(view16Bit).characters16());
    EXPECT_EQ(view16Bit.length(), StringView(view16Bit).length());
    EXPECT_EQ(kChars, StringView(view16Bit).toString());

    // StringView(const StringView&, unsigned offset);
    ASSERT_FALSE(StringView(view16Bit, 2).is8Bit());
    EXPECT_FALSE(StringView(view16Bit, 2).isNull());
    EXPECT_EQ(view16Bit.characters16() + 2, StringView(view16Bit, 2).characters16());
    EXPECT_EQ(3u, StringView(view16Bit, 2).length());
    EXPECT_EQ(StringView("345"), StringView(view16Bit, 2));
    EXPECT_STREQ("345", StringView(view16Bit, 2).toString().utf8().data());

    // StringView(const StringView&, unsigned offset, unsigned length);
    ASSERT_FALSE(StringView(view16Bit, 2, 1).is8Bit());
    EXPECT_FALSE(StringView(view16Bit, 2, 1).isNull());
    EXPECT_EQ(view16Bit.characters16() + 2, StringView(view16Bit, 2, 1).characters16());
    EXPECT_EQ(1u, StringView(view16Bit, 2, 1).length());
    EXPECT_EQ(StringView("3"), StringView(view16Bit, 2, 1));
    EXPECT_STREQ("3", StringView(view16Bit, 2, 1).toString().utf8().data());
}

TEST(StringViewTest, ConstructionLiteral8)
{
    // StringView(const LChar* chars);
    ASSERT_TRUE(StringView(kChars8).is8Bit());
    EXPECT_FALSE(StringView(kChars8).isNull());
    EXPECT_EQ(kChars8, StringView(kChars8).characters8());
    EXPECT_EQ(5u, StringView(kChars8).length());
    EXPECT_STREQ(kChars, StringView(kChars8).toString().utf8().data());

    // StringView(const char* chars);
    ASSERT_TRUE(StringView(kChars).is8Bit());
    EXPECT_FALSE(StringView(kChars).isNull());
    EXPECT_EQ(kChars8, StringView(kChars).characters8());
    EXPECT_EQ(5u, StringView(kChars).length());
    EXPECT_STREQ(kChars, StringView(kChars).toString().utf8().data());

    // StringView(const LChar* chars, unsigned length);
    ASSERT_TRUE(StringView(kChars8, 2).is8Bit());
    EXPECT_FALSE(StringView(kChars8, 2).isNull());
    EXPECT_EQ(2u, StringView(kChars8, 2).length());
    EXPECT_EQ(StringView("12"), StringView(kChars8, 2));
    EXPECT_STREQ("12", StringView(kChars8, 2).toString().utf8().data());

    // StringView(const char* chars, unsigned length);
    ASSERT_TRUE(StringView(kChars, 2).is8Bit());
    EXPECT_FALSE(StringView(kChars, 2).isNull());
    EXPECT_EQ(2u, StringView(kChars, 2).length());
    EXPECT_EQ(StringView("12"), StringView(kChars, 2));
    EXPECT_STREQ("12", StringView(kChars, 2).toString().utf8().data());
}

TEST(StringViewTest, ConstructionLiteral16)
{
    // StringView(const UChar* chars);
    ASSERT_FALSE(StringView(kChars16).is8Bit());
    EXPECT_FALSE(StringView(kChars16).isNull());
    EXPECT_EQ(kChars16, StringView(kChars16).characters16());
    EXPECT_EQ(5u, StringView(kChars16).length());
    EXPECT_EQ(String(kChars16), StringView(kChars16).toString().utf8().data());

    // StringView(const UChar* chars, unsigned length);
    ASSERT_FALSE(StringView(kChars16, 2).is8Bit());
    EXPECT_FALSE(StringView(kChars16, 2).isNull());
    EXPECT_EQ(kChars16, StringView(kChars16, 2).characters16());
    EXPECT_EQ(StringView("12"), StringView(kChars16, 2));
    EXPECT_EQ(StringView(reinterpret_cast<const UChar*>(u"12")), StringView(kChars16, 2));
    EXPECT_EQ(2u, StringView(kChars16, 2).length());
    EXPECT_EQ(String("12"), StringView(kChars16, 2).toString());
}

TEST(StringViewTest, ConstructionRawBytes)
{
    // StringView(const void* bytes, unsigned length, bool is8Bit);
    StringView view8(reinterpret_cast<const void*>(kChars), 2, true);
    ASSERT_TRUE(view8.is8Bit());
    EXPECT_EQ(2u, view8.length());
    EXPECT_EQ("12", view8);

    StringView view16(reinterpret_cast<const void*>(kChars16), 3, false);
    ASSERT_FALSE(view16.is8Bit());
    EXPECT_EQ(3u, view16.length());
    EXPECT_EQ("123", view16);
}

TEST(StringViewTest, IsEmpty)
{
    EXPECT_FALSE(StringView(kChars).isEmpty());
    EXPECT_TRUE(StringView(kChars, 0).isEmpty());
    EXPECT_FALSE(StringView(String(kChars)).isEmpty());
    EXPECT_TRUE(StringView(String(kChars), 5).isEmpty());
    EXPECT_TRUE(StringView(String(kChars), 4, 0).isEmpty());
    EXPECT_TRUE(StringView().isEmpty());
    EXPECT_TRUE(StringView("").isEmpty());
    EXPECT_TRUE(StringView(reinterpret_cast<const UChar*>(u"")).isEmpty());
    EXPECT_FALSE(StringView(kChars16).isEmpty());
}

TEST(StringViewTest, ToString)
{
    EXPECT_EQ(emptyString().impl(), StringView("").toString().impl());
    EXPECT_EQ(nullAtom.impl(), StringView().toString().impl());
    // NOTE: All the construction tests also check toString().
}

TEST(StringViewTest, ToAtomicString)
{
    EXPECT_EQ(nullAtom.impl(), StringView().toAtomicString());
    EXPECT_EQ(emptyAtom.impl(), StringView("").toAtomicString());
    EXPECT_EQ(AtomicString("12"), StringView(kChars8, 2).toAtomicString());
    // AtomicString will convert to 8bit if possible when creating the string.
    EXPECT_EQ(AtomicString("12").impl(), StringView(kChars16, 2).toAtomicString().impl());
}

TEST(StringViewTest, ToStringImplSharing)
{
    String string(kChars);
    EXPECT_EQ(string.impl(), StringView(string).sharedImpl());
    EXPECT_EQ(string.impl(), StringView(string).toString().impl());
    EXPECT_EQ(string.impl(), StringView(string).toAtomicString().impl());
}

TEST(StringViewTest, NullString)
{
    EXPECT_TRUE(StringView().isNull());
    EXPECT_TRUE(StringView(String()).isNull());
    EXPECT_TRUE(StringView(AtomicString()).isNull());
    EXPECT_TRUE(StringView(static_cast<const char*>(nullptr)).isNull());
    StringView view(kChars);
    EXPECT_FALSE(view.isNull());
    view.clear();
    EXPECT_TRUE(view.isNull());
    EXPECT_EQ(String(), StringView());
    EXPECT_TRUE(StringView().toString().isNull());
    EXPECT_FALSE(equalStringView(StringView(), ""));
    EXPECT_TRUE(equalStringView(StringView(), StringView()));
    EXPECT_FALSE(equalStringView(StringView(), "abc"));
    EXPECT_FALSE(equalStringView("abc", StringView()));
    EXPECT_FALSE(equalIgnoringASCIICase(StringView(), ""));
    EXPECT_TRUE(equalIgnoringASCIICase(StringView(), StringView()));
    EXPECT_FALSE(equalIgnoringASCIICase(StringView(), "abc"));
    EXPECT_FALSE(equalIgnoringASCIICase("abc", StringView()));
}

TEST(StringViewTest, IndexAccess)
{
    StringView view8(kChars8);
    EXPECT_EQ('1', view8[0]);
    EXPECT_EQ('3', view8[2]);
    StringView view16(kChars16);
    EXPECT_EQ('1', view16[0]);
    EXPECT_EQ('3', view16[2]);
}

TEST(StringViewTest, EqualIgnoringASCIICase)
{
    static const char* link8 = "link";
    static const char* linkCaps8 = "LINK";
    static const char* nonASCII8 = "a\xE1";
    static const char* nonASCIICaps8 = "A\xE1";
    static const char* nonASCIIInvalid8 = "a\xC1";

    static const UChar link16[5] = { 0x006c, 0x0069, 0x006e, 0x006b, 0 }; // link
    static const UChar linkCaps16[5] = { 0x004c, 0x0049, 0x004e, 0x004b, 0 }; // LINK
    static const UChar nonASCII16[3] = { 0x0061, 0x00e1, 0 }; // a\xE1
    static const UChar nonASCIICaps16[3] = { 0x0041, 0x00e1, 0 }; // A\xE1
    static const UChar nonASCIIInvalid16[3] = { 0x0061, 0x00c1, 0 }; // a\xC1

    EXPECT_TRUE(equalIgnoringASCIICase(StringView(link16), link8));
    EXPECT_TRUE(equalIgnoringASCIICase(StringView(link16), linkCaps16));
    EXPECT_TRUE(equalIgnoringASCIICase(StringView(link16), linkCaps8));
    EXPECT_TRUE(equalIgnoringASCIICase(StringView(link8), linkCaps8));
    EXPECT_TRUE(equalIgnoringASCIICase(StringView(link8), link16));

    EXPECT_TRUE(equalIgnoringASCIICase(StringView(nonASCII8), nonASCIICaps8));
    EXPECT_TRUE(equalIgnoringASCIICase(StringView(nonASCII8), nonASCIICaps16));
    EXPECT_TRUE(equalIgnoringASCIICase(StringView(nonASCII16), nonASCIICaps16));
    EXPECT_TRUE(equalIgnoringASCIICase(StringView(nonASCII16), nonASCIICaps8));
    EXPECT_FALSE(equalIgnoringASCIICase(StringView(nonASCII8), nonASCIIInvalid8));
    EXPECT_FALSE(equalIgnoringASCIICase(StringView(nonASCII8), nonASCIIInvalid16));

    EXPECT_TRUE(equalIgnoringASCIICase(StringView("link"), "lInK"));
    EXPECT_FALSE(equalIgnoringASCIICase(StringView("link"), "INKL"));
    EXPECT_FALSE(equalIgnoringASCIICase(StringView("link"), "link different length"));
    EXPECT_FALSE(equalIgnoringASCIICase(StringView("link different length"), "link"));

    EXPECT_TRUE(equalIgnoringASCIICase(StringView(""), ""));
}

} // namespace WTF
