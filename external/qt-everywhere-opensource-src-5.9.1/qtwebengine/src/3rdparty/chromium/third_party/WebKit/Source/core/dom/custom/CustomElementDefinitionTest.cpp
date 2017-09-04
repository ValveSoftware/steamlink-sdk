// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/CustomElementDefinition.h"

#include "core/dom/Node.h"  // CustomElementState
#include "core/dom/custom/CEReactionsScope.h"
#include "core/dom/custom/CustomElementDescriptor.h"
#include "core/dom/custom/CustomElementReactionTestHelpers.h"
#include "core/dom/custom/CustomElementTestHelpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class ConstructorFails : public TestCustomElementDefinition {
  WTF_MAKE_NONCOPYABLE(ConstructorFails);

 public:
  ConstructorFails(const CustomElementDescriptor& descriptor)
      : TestCustomElementDefinition(descriptor) {}
  ~ConstructorFails() override = default;
  bool runConstructor(Element*) override { return false; }
};

}  // namespace

TEST(CustomElementDefinitionTest, upgrade_clearsReactionQueueOnFailure) {
  Element* element = CreateElement("a-a");
  EXPECT_EQ(CustomElementState::Undefined, element->getCustomElementState())
      << "sanity check: this element should be ready to upgrade";
  {
    CEReactionsScope reactions;
    reactions.enqueueToCurrentQueue(
        element, new TestReaction({new Unreached(
                     "upgrade failure should clear the reaction queue")}));
    ConstructorFails definition(CustomElementDescriptor("a-a", "a-a"));
    definition.upgrade(element);
  }
  EXPECT_EQ(CustomElementState::Failed, element->getCustomElementState())
      << "failing to construct should have set the 'failed' element state";
}

TEST(CustomElementDefinitionTest,
     upgrade_clearsReactionQueueOnFailure_backupStack) {
  Element* element = CreateElement("a-a");
  EXPECT_EQ(CustomElementState::Undefined, element->getCustomElementState())
      << "sanity check: this element should be ready to upgrade";
  ResetCustomElementReactionStackForTest resetReactionStack;
  resetReactionStack.stack().enqueueToBackupQueue(
      element, new TestReaction({new Unreached(
                   "upgrade failure should clear the reaction queue")}));
  ConstructorFails definition(CustomElementDescriptor("a-a", "a-a"));
  definition.upgrade(element);
  EXPECT_EQ(CustomElementState::Failed, element->getCustomElementState())
      << "failing to construct should have set the 'failed' element state";
}

}  // namespace blink
