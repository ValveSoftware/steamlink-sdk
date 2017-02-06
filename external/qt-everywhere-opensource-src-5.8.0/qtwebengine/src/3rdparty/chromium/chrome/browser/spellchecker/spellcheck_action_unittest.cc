// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_action.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(SpellcheckActionTest, FinalActionsTest) {
  static const SpellcheckAction::SpellcheckActionType kFinalActions[] = {
    SpellcheckAction::TYPE_ADD_TO_DICT,
    SpellcheckAction::TYPE_IGNORE,
    SpellcheckAction::TYPE_IN_DICTIONARY,
    SpellcheckAction::TYPE_MANUALLY_CORRECTED,
    SpellcheckAction::TYPE_NO_ACTION,
    SpellcheckAction::TYPE_SELECT,
  };
  SpellcheckAction action;
  for (size_t i = 0; i < arraysize(kFinalActions); ++i) {
    action.set_type(kFinalActions[i]);
    ASSERT_TRUE(action.IsFinal());
  }
}

TEST(SpellcheckActionTest, PendingActionsTest) {
  static const SpellcheckAction::SpellcheckActionType kPendingActions[] = {
    SpellcheckAction::TYPE_PENDING,
    SpellcheckAction::TYPE_PENDING_IGNORE,
  };
  SpellcheckAction action;
  for (size_t i = 0; i < arraysize(kPendingActions); ++i) {
    action.set_type(kPendingActions[i]);
    ASSERT_FALSE(action.IsFinal());
  }
}

TEST(SpellcheckActionTest, FinalizeTest) {
  SpellcheckAction action;
  action.set_type(SpellcheckAction::TYPE_PENDING);
  action.Finalize();
  ASSERT_EQ(SpellcheckAction::TYPE_NO_ACTION, action.type());

  action.set_type(SpellcheckAction::TYPE_PENDING_IGNORE);
  action.Finalize();
  ASSERT_EQ(SpellcheckAction::TYPE_IGNORE, action.type());
}

TEST(SpellcheckActionTest, SerializeTest) {
  static const size_t kNumTestCases = 7;
  static const struct {
    SpellcheckAction action;
    std::string expected;
  } kTestCases[] = {
    { SpellcheckAction(
          SpellcheckAction::TYPE_ADD_TO_DICT, -1,
          base::ASCIIToUTF16("nothing")),
      "{\"actionType\": \"ADD_TO_DICT\"}" },
    { SpellcheckAction(
          SpellcheckAction::TYPE_IGNORE, -1, base::ASCIIToUTF16("nothing")),
      "{\"actionType\": \"IGNORE\"}" },
    { SpellcheckAction(
          SpellcheckAction::TYPE_IN_DICTIONARY, -1,
          base::ASCIIToUTF16("nothing")),
      "{\"actionType\": \"IN_DICTIONARY\"}" },
    { SpellcheckAction(
          SpellcheckAction::TYPE_MANUALLY_CORRECTED, -1,
          base::ASCIIToUTF16("hello")),
      "{\"actionTargetValue\": \"hello\","
      "\"actionType\": \"MANUALLY_CORRECTED\"}" },
    { SpellcheckAction(
          SpellcheckAction::TYPE_NO_ACTION, -1, base::ASCIIToUTF16("nothing")),
      "{\"actionType\": \"NO_ACTION\"}" },
    { SpellcheckAction(
          SpellcheckAction::TYPE_PENDING, -1, base::ASCIIToUTF16("nothing")),
      "{\"actionType\": \"PENDING\"}" },
    { SpellcheckAction(
          SpellcheckAction::TYPE_PENDING_IGNORE, -1,
          base::ASCIIToUTF16("nothing")),
      "{\"actionType\": \"PENDING\"}" },
    { SpellcheckAction(
          SpellcheckAction::TYPE_SELECT, 42, base::ASCIIToUTF16("nothing")),
      "{\"actionTargetIndex\": 42, \"actionType\": \"SELECT\"}" },
  };
  for (size_t i = 0; i < kNumTestCases; ++i) {
    std::unique_ptr<base::DictionaryValue> serialized(
        kTestCases[i].action.Serialize());
    std::unique_ptr<base::Value> expected =
        base::JSONReader::Read(kTestCases[i].expected);
    EXPECT_TRUE(serialized->Equals(expected.get()));
  }
}
