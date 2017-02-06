// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/resolver/MatchResult.h"

#include "core/css/StylePropertySet.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class MatchResultTest : public ::testing::Test {
protected:
    void SetUp() override;

    const StylePropertySet* propertySet(unsigned index) const
    {
        return propertySets[index].get();
    }

private:
    PersistentHeapVector<Member<MutableStylePropertySet>, 6> propertySets;
};

void MatchResultTest::SetUp()
{
    for (unsigned i = 0; i < 6; i++)
        propertySets.append(MutableStylePropertySet::create(HTMLQuirksMode));
}

void testMatchedPropertiesRange(const MatchedPropertiesRange& range, int expectedLength, const StylePropertySet** expectedSets)
{
    EXPECT_EQ(expectedLength, range.end() - range.begin());
    for (const auto& matchedProperties : range)
        EXPECT_EQ(*expectedSets++, matchedProperties.properties);
}

TEST_F(MatchResultTest, UARules)
{
    const StylePropertySet* uaSets[] = { propertySet(0), propertySet(1) };

    MatchResult result;
    result.addMatchedProperties(uaSets[0]);
    result.addMatchedProperties(uaSets[1]);
    result.finishAddingUARules();

    result.finishAddingAuthorRulesForTreeScope();

    testMatchedPropertiesRange(result.allRules(), 2, uaSets);
    testMatchedPropertiesRange(result.uaRules(), 2, uaSets);
    testMatchedPropertiesRange(result.authorRules(), 0, nullptr);

    ImportantAuthorRanges important(result);
    EXPECT_EQ(important.end(), important.begin());
}

TEST_F(MatchResultTest, AuthorRules)
{
    const StylePropertySet* authorSets[] = { propertySet(0), propertySet(1) };

    MatchResult result;

    result.finishAddingUARules();
    result.addMatchedProperties(authorSets[0]);
    result.addMatchedProperties(authorSets[1]);
    result.finishAddingAuthorRulesForTreeScope();

    testMatchedPropertiesRange(result.allRules(), 2, authorSets);
    testMatchedPropertiesRange(result.uaRules(), 0, nullptr);
    testMatchedPropertiesRange(result.authorRules(), 2, authorSets);

    ImportantAuthorRanges important(result);
    EXPECT_EQ(important.end(), ++important.begin());
}

TEST_F(MatchResultTest, UAAndAuthorRules)
{
    const StylePropertySet* allSets[] = { propertySet(0), propertySet(1), propertySet(2), propertySet(3) };
    const StylePropertySet** uaSets = &allSets[0];
    const StylePropertySet** authorSets = &allSets[2];

    MatchResult result;

    result.addMatchedProperties(uaSets[0]);
    result.addMatchedProperties(uaSets[1]);
    result.finishAddingUARules();

    result.addMatchedProperties(authorSets[0]);
    result.addMatchedProperties(authorSets[1]);
    result.finishAddingAuthorRulesForTreeScope();

    testMatchedPropertiesRange(result.allRules(), 4, allSets);
    testMatchedPropertiesRange(result.uaRules(), 2, uaSets);
    testMatchedPropertiesRange(result.authorRules(), 2, authorSets);

    ImportantAuthorRanges important(result);
    EXPECT_EQ(important.end(), ++important.begin());
}

TEST_F(MatchResultTest, AuthorRulesMultipleScopes)
{
    const StylePropertySet* authorSets[] = { propertySet(0), propertySet(1), propertySet(2), propertySet(3) };

    MatchResult result;

    result.finishAddingUARules();

    result.addMatchedProperties(authorSets[0]);
    result.addMatchedProperties(authorSets[1]);
    result.finishAddingAuthorRulesForTreeScope();

    result.addMatchedProperties(authorSets[2]);
    result.addMatchedProperties(authorSets[3]);
    result.finishAddingAuthorRulesForTreeScope();

    testMatchedPropertiesRange(result.allRules(), 4, authorSets);
    testMatchedPropertiesRange(result.uaRules(), 0, nullptr);
    testMatchedPropertiesRange(result.authorRules(), 4, authorSets);

    ImportantAuthorRanges important(result);

    auto iter = important.begin();
    EXPECT_NE(important.end(), iter);
    testMatchedPropertiesRange(*iter, 2, &authorSets[2]);

    ++iter;
    EXPECT_NE(important.end(), iter);
    testMatchedPropertiesRange(*iter, 2, authorSets);

    ++iter;
    EXPECT_EQ(important.end(), iter);
}

TEST_F(MatchResultTest, UARulesAndAuthorRulesMultipleScopes)
{
    const StylePropertySet* allSets[] = { propertySet(0), propertySet(1), propertySet(2), propertySet(3), propertySet(4), propertySet(5) };
    const StylePropertySet** uaSets = &allSets[0];
    const StylePropertySet** authorSets = &allSets[2];

    MatchResult result;

    result.addMatchedProperties(uaSets[0]);
    result.addMatchedProperties(uaSets[1]);
    result.finishAddingUARules();

    result.addMatchedProperties(authorSets[0]);
    result.addMatchedProperties(authorSets[1]);
    result.finishAddingAuthorRulesForTreeScope();

    result.addMatchedProperties(authorSets[2]);
    result.addMatchedProperties(authorSets[3]);
    result.finishAddingAuthorRulesForTreeScope();

    testMatchedPropertiesRange(result.allRules(), 6, allSets);
    testMatchedPropertiesRange(result.uaRules(), 2, uaSets);
    testMatchedPropertiesRange(result.authorRules(), 4, authorSets);

    ImportantAuthorRanges important(result);

    ImportantAuthorRangeIterator iter = important.begin();
    EXPECT_NE(important.end(), iter);
    testMatchedPropertiesRange(*iter, 2, &authorSets[2]);

    ++iter;
    EXPECT_NE(important.end(), iter);
    testMatchedPropertiesRange(*iter, 2, authorSets);

    ++iter;
    EXPECT_EQ(important.end(), iter);
}

} // namespace blink
