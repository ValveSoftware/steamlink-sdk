// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for |Feedback| object.

#include "chrome/browser/spellchecker/feedback.h"

#include <stddef.h>
#include <stdint.h>

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace spellcheck {

namespace {

// Maximum number of bytes of feedback that would be sent to the server at once.
const size_t kMaxFeedbackSize = 1024;

// Identifier for a renderer process.
const int kRendererProcessId = 7;

// Hash identifier for a misspelling.
const uint32_t kMisspellingHash = 42;

}  // namespace

// A test fixture to help keep the tests simple.
class FeedbackTest : public testing::Test {
 public:
  FeedbackTest() : feedback_(kMaxFeedbackSize) {}
  ~FeedbackTest() override {}

 protected:
  void AddMisspelling(int renderer_process_id, uint32_t hash) {
    feedback_.AddMisspelling(renderer_process_id,
                             Misspelling(base::string16(), 0, 0,
                                         std::vector<base::string16>(), hash));
  }

  spellcheck::Feedback feedback_;
};

TEST_F(FeedbackTest, LimitFeedbackSize) {
  // Adding too much feedback data should prevent adding more feedback.
  feedback_.AddMisspelling(
      kRendererProcessId,
      Misspelling(
          base::ASCIIToUTF16("0123456789"), 0, 1,
          std::vector<base::string16>(50, base::ASCIIToUTF16("9876543210")),
          0));
  feedback_.AddMisspelling(
      kRendererProcessId,
      Misspelling(
          base::ASCIIToUTF16("0123456789"), 0, 1,
          std::vector<base::string16>(50, base::ASCIIToUTF16("9876543210")),
          kMisspellingHash));
  EXPECT_EQ(nullptr, feedback_.GetMisspelling(kMisspellingHash));

  // Clearing the existing feedback data should allow adding feedback again.
  feedback_.Clear();
  feedback_.AddMisspelling(
      kRendererProcessId,
      Misspelling(
          base::ASCIIToUTF16("0123456789"), 0, 1,
          std::vector<base::string16>(50, base::ASCIIToUTF16("9876543210")),
          kMisspellingHash));
  EXPECT_NE(nullptr, feedback_.GetMisspelling(kMisspellingHash));
  feedback_.Clear();

  // Erasing finalized misspellings should allow adding feedback again.
  feedback_.AddMisspelling(
      kRendererProcessId,
      Misspelling(
          base::ASCIIToUTF16("0123456789"), 0, 1,
          std::vector<base::string16>(50, base::ASCIIToUTF16("9876543210")),
          0));
  feedback_.FinalizeRemovedMisspellings(kRendererProcessId,
                                        std::vector<uint32_t>());
  feedback_.EraseFinalizedMisspellings(kRendererProcessId);
  feedback_.AddMisspelling(
      kRendererProcessId,
      Misspelling(
          base::ASCIIToUTF16("0123456789"), 0, 1,
          std::vector<base::string16>(50, base::ASCIIToUTF16("9876543210")),
          kMisspellingHash));
  EXPECT_NE(nullptr, feedback_.GetMisspelling(kMisspellingHash));
}

// Should be able to retrieve misspelling after it's added.
TEST_F(FeedbackTest, RetreiveMisspelling) {
  EXPECT_EQ(nullptr, feedback_.GetMisspelling(kMisspellingHash));
  AddMisspelling(kRendererProcessId, kMisspellingHash);
  Misspelling* result = feedback_.GetMisspelling(kMisspellingHash);
  EXPECT_NE(nullptr, result);
  EXPECT_EQ(kMisspellingHash, result->hash);
}

// Removed misspellings should be finalized.
TEST_F(FeedbackTest, FinalizeRemovedMisspellings) {
  static const int kRemovedMisspellingHash = 1;
  static const int kRemainingMisspellingHash = 2;
  AddMisspelling(kRendererProcessId, kRemovedMisspellingHash);
  AddMisspelling(kRendererProcessId, kRemainingMisspellingHash);
  std::vector<uint32_t> remaining_markers(1, kRemainingMisspellingHash);
  feedback_.FinalizeRemovedMisspellings(kRendererProcessId, remaining_markers);
  Misspelling* removed_misspelling =
      feedback_.GetMisspelling(kRemovedMisspellingHash);
  EXPECT_NE(nullptr, removed_misspelling);
  EXPECT_TRUE(removed_misspelling->action.IsFinal());
  Misspelling* remaining_misspelling =
      feedback_.GetMisspelling(kRemainingMisspellingHash);
  EXPECT_NE(nullptr, remaining_misspelling);
  EXPECT_FALSE(remaining_misspelling->action.IsFinal());
}

// Duplicate misspellings should not be finalized.
TEST_F(FeedbackTest, DuplicateMisspellingFinalization) {
  AddMisspelling(kRendererProcessId, kMisspellingHash);
  AddMisspelling(kRendererProcessId, kMisspellingHash);
  std::vector<uint32_t> remaining_markers(1, kMisspellingHash);
  feedback_.FinalizeRemovedMisspellings(kRendererProcessId, remaining_markers);
  std::vector<Misspelling> misspellings = feedback_.GetAllMisspellings();
  EXPECT_EQ(static_cast<size_t>(1), misspellings.size());
  EXPECT_FALSE(misspellings[0].action.IsFinal());
}

// Misspellings should be associated with a renderer.
TEST_F(FeedbackTest, RendererHasMisspellings) {
  EXPECT_FALSE(feedback_.RendererHasMisspellings(kRendererProcessId));
  AddMisspelling(kRendererProcessId, kMisspellingHash);
  EXPECT_TRUE(feedback_.RendererHasMisspellings(kRendererProcessId));
}

// Should be able to retrieve misspellings in renderer.
TEST_F(FeedbackTest, GetMisspellingsInRenderer) {
  AddMisspelling(kRendererProcessId, kMisspellingHash);
  const std::vector<Misspelling>& renderer_with_misspellings =
      feedback_.GetMisspellingsInRenderer(kRendererProcessId);
  EXPECT_EQ(static_cast<size_t>(1), renderer_with_misspellings.size());
  EXPECT_EQ(kMisspellingHash, renderer_with_misspellings[0].hash);
  const std::vector<Misspelling>& renderer_without_misspellings =
      feedback_.GetMisspellingsInRenderer(kRendererProcessId + 1);
  EXPECT_EQ(static_cast<size_t>(0), renderer_without_misspellings.size());
}

// Finalized misspellings should be erased.
TEST_F(FeedbackTest, EraseFinalizedMisspellings) {
  AddMisspelling(kRendererProcessId, kMisspellingHash);
  feedback_.FinalizeRemovedMisspellings(kRendererProcessId,
                                        std::vector<uint32_t>());
  EXPECT_TRUE(feedback_.RendererHasMisspellings(kRendererProcessId));
  feedback_.EraseFinalizedMisspellings(kRendererProcessId);
  EXPECT_FALSE(feedback_.RendererHasMisspellings(kRendererProcessId));
  EXPECT_TRUE(feedback_.GetMisspellingsInRenderer(kRendererProcessId).empty());
}

// Should be able to check for misspelling existence.
TEST_F(FeedbackTest, HasMisspelling) {
  EXPECT_FALSE(feedback_.HasMisspelling(kMisspellingHash));
  AddMisspelling(kRendererProcessId, kMisspellingHash);
  EXPECT_TRUE(feedback_.HasMisspelling(kMisspellingHash));
}

// Should be able to check for feedback data presence.
TEST_F(FeedbackTest, EmptyFeedback) {
  EXPECT_TRUE(feedback_.Empty());
  AddMisspelling(kRendererProcessId, kMisspellingHash);
  EXPECT_FALSE(feedback_.Empty());
}

// Should be able to retrieve a list of all renderers with misspellings.
TEST_F(FeedbackTest, GetRendersWithMisspellings) {
  EXPECT_TRUE(feedback_.GetRendersWithMisspellings().empty());
  AddMisspelling(kRendererProcessId, kMisspellingHash);
  AddMisspelling(kRendererProcessId + 1, kMisspellingHash + 1);
  std::vector<int> result = feedback_.GetRendersWithMisspellings();
  EXPECT_EQ(static_cast<size_t>(2), result.size());
  EXPECT_NE(result[0], result[1]);
  EXPECT_TRUE(result[0] == kRendererProcessId ||
              result[0] == kRendererProcessId + 1);
  EXPECT_TRUE(result[1] == kRendererProcessId ||
              result[1] == kRendererProcessId + 1);
}

// Should be able to finalize all misspellings.
TEST_F(FeedbackTest, FinalizeAllMisspellings) {
  AddMisspelling(kRendererProcessId, kMisspellingHash);
  AddMisspelling(kRendererProcessId + 1, kMisspellingHash + 1);
  {
    std::vector<Misspelling> pending = feedback_.GetAllMisspellings();
    for (std::vector<Misspelling>::const_iterator it = pending.begin();
         it != pending.end(); ++it) {
      EXPECT_FALSE(it->action.IsFinal());
    }
  }
  feedback_.FinalizeAllMisspellings();
  {
    std::vector<Misspelling> final = feedback_.GetAllMisspellings();
    for (std::vector<Misspelling>::const_iterator it = final.begin();
         it != final.end(); ++it) {
      EXPECT_TRUE(it->action.IsFinal());
    }
  }
}

// Should be able to retrieve a copy of all misspellings.
TEST_F(FeedbackTest, GetAllMisspellings) {
  EXPECT_TRUE(feedback_.GetAllMisspellings().empty());
  AddMisspelling(kRendererProcessId, kMisspellingHash);
  AddMisspelling(kRendererProcessId + 1, kMisspellingHash + 1);
  const std::vector<Misspelling>& result = feedback_.GetAllMisspellings();
  EXPECT_EQ(static_cast<size_t>(2), result.size());
  EXPECT_NE(result[0].hash, result[1].hash);
  EXPECT_TRUE(result[0].hash == kMisspellingHash ||
              result[0].hash == kMisspellingHash + 1);
  EXPECT_TRUE(result[1].hash == kMisspellingHash ||
              result[1].hash == kMisspellingHash + 1);
}

// Should be able to clear all misspellings.
TEST_F(FeedbackTest, ClearFeedback) {
  AddMisspelling(kRendererProcessId, kMisspellingHash);
  AddMisspelling(kRendererProcessId + 1, kMisspellingHash + 1);
  EXPECT_FALSE(feedback_.Empty());
  feedback_.Clear();
  EXPECT_TRUE(feedback_.Empty());
}

// Should be able to find misspellings by misspelled word.
TEST_F(FeedbackTest, FindMisspellingsByText) {
  static const base::string16 kMisspelledText =
      ASCIIToUTF16("Helllo world. Helllo world");
  static const base::string16 kSuggestion = ASCIIToUTF16("Hello");
  static const int kMisspellingStart = 0;
  static const int kMisspellingLength = 6;
  static const int kSentenceLength = 14;
  static const int kNumberOfSentences = 2;
  static const int kNumberOfRenderers = 2;
  uint32_t hash = kMisspellingHash;
  for (int renderer_process_id = kRendererProcessId;
       renderer_process_id < kRendererProcessId + kNumberOfRenderers;
       ++renderer_process_id) {
    for (int j = 0; j < kNumberOfSentences; ++j) {
      feedback_.AddMisspelling(
          renderer_process_id,
          Misspelling(kMisspelledText, kMisspellingStart + j * kSentenceLength,
                      kMisspellingLength,
                      std::vector<base::string16>(1, kSuggestion), ++hash));
    }
  }

  static const base::string16 kOtherMisspelledText =
      ASCIIToUTF16("Somethign else");
  static const base::string16 kOtherSuggestion = ASCIIToUTF16("Something");
  static const int kOtherMisspellingStart = 0;
  static const int kOtherMisspellingLength = 9;
  feedback_.AddMisspelling(
      kRendererProcessId,
      Misspelling(kOtherMisspelledText, kOtherMisspellingStart,
                  kOtherMisspellingLength,
                  std::vector<base::string16>(1, kOtherSuggestion), hash + 1));

  static const base::string16 kMisspelledWord = ASCIIToUTF16("Helllo");
  const std::set<uint32_t>& misspellings =
      feedback_.FindMisspellings(kMisspelledWord);
  EXPECT_EQ(static_cast<size_t>(kNumberOfSentences * kNumberOfRenderers),
            misspellings.size());

  for (std::set<uint32_t>::const_iterator it = misspellings.begin();
       it != misspellings.end(); ++it) {
    Misspelling* misspelling = feedback_.GetMisspelling(*it);
    EXPECT_NE(nullptr, misspelling);
    EXPECT_TRUE(misspelling->hash >= kMisspellingHash &&
                misspelling->hash <= hash);
    EXPECT_EQ(kMisspelledWord, GetMisspelledString(*misspelling));
  }
}

// Should not be able to find misspellings by misspelled word after they have
// been removed.
TEST_F(FeedbackTest, CannotFindMisspellingsByTextAfterErased) {
  static const base::string16 kMisspelledText = ASCIIToUTF16("Helllo world");
  static const base::string16 kMisspelledWord = ASCIIToUTF16("Helllo");
  static const base::string16 kSuggestion = ASCIIToUTF16("Hello");
  static const int kMisspellingStart = 0;
  static const int kMisspellingLength = 6;
  feedback_.AddMisspelling(
      kRendererProcessId,
      Misspelling(kMisspelledText, kMisspellingStart, kMisspellingLength,
                  std::vector<base::string16>(1, kSuggestion),
                  kMisspellingHash));
  feedback_.GetMisspelling(kMisspellingHash)->action.Finalize();
  feedback_.EraseFinalizedMisspellings(kRendererProcessId);
  EXPECT_TRUE(feedback_.FindMisspellings(kMisspelledWord).empty());
}

}  // namespace spellcheck
