// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/Attr.h"

#include "core/dom/Document.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class AttrTest : public ::testing::Test {
protected:
    void SetUp() override;

    Attr* createAttribute();
    const AtomicString& value() const { return m_value; }

private:
    Persistent<Document> m_document;
    AtomicString m_value;
};

void AttrTest::SetUp()
{
    m_document = Document::create();
    m_value = "value";
}

Attr* AttrTest::createAttribute()
{
    return m_document->createAttribute("name", ASSERT_NO_EXCEPTION);
}

TEST_F(AttrTest, InitialValueState)
{
    Attr* attr = createAttribute();
    EXPECT_EQ(emptyAtom, attr->value());
    EXPECT_EQ(emptyString(), attr->toNode()->nodeValue());
    EXPECT_EQ(String(), attr->textContent());
}

TEST_F(AttrTest, SetValue)
{
    Attr* attr = createAttribute();
    attr->setValue(value());
    EXPECT_EQ(value(), attr->value());
    EXPECT_EQ(value(), attr->toNode()->nodeValue());
    // Node::textContent() always returns String() for Attr.
    EXPECT_EQ(String(), attr->textContent());
}

TEST_F(AttrTest, SetNodeValue)
{
    Attr* attr = createAttribute();
    attr->toNode()->setNodeValue(value());
    EXPECT_EQ(value(), attr->value());
    EXPECT_EQ(value(), attr->toNode()->nodeValue());
    // Node::textContent() always returns String() for Attr.
    EXPECT_EQ(String(), attr->textContent());
}

TEST_F(AttrTest, SetTextContent)
{
    Attr* attr = createAttribute();
    // Node::setTextContent() does nothing for Attr.
    attr->setTextContent(value());
    EXPECT_EQ(emptyAtom, attr->value());
    EXPECT_EQ(emptyString(), attr->toNode()->nodeValue());
    EXPECT_EQ(String(), attr->textContent());
}

TEST_F(AttrTest, LengthOfContents)
{
    Attr* attr = createAttribute();
    EXPECT_EQ(0u, attr->lengthOfContents());
    attr->setValue(value());
    EXPECT_EQ(0u, attr->lengthOfContents());
}

} // namespace blink
