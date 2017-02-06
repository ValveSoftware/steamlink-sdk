// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for |FeedbackSender| object.

#include "chrome/browser/spellchecker/feedback_sender.h"

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/spellcheck_common.h"
#include "chrome/common/spellcheck_marker.h"
#include "chrome/common/spellcheck_result.h"
#include "chrome/test/base/testing_profile.h"
#include "components/variations/entropy_provider.h"
#include "content/public/test/test_browser_thread.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace spellcheck {

namespace {

const char kCountry[] = "USA";
const char kLanguage[] = "en";
const char kText[] = "Helllo world.";
const int kMisspellingLength = 6;
const int kMisspellingStart = 0;
const int kRendererProcessId = 0;
const int kUrlFetcherId = 0;

// Builds a simple spellcheck result.
SpellCheckResult BuildSpellCheckResult() {
  return SpellCheckResult(SpellCheckResult::SPELLING,
                          kMisspellingStart,
                          kMisspellingLength,
                          base::UTF8ToUTF16("Hello"));
}

// Returns the number of times that |needle| appears in |haystack| without
// overlaps. For example, CountOccurences("bananana", "nana") returns 1.
int CountOccurences(const std::string& haystack, const std::string& needle) {
  int number_of_occurrences = 0;
  for (size_t pos = haystack.find(needle);
       pos != std::string::npos;
       pos = haystack.find(needle, pos + needle.length())) {
    ++number_of_occurrences;
  }
  return number_of_occurrences;
}

class MockFeedbackSender : public spellcheck::FeedbackSender {
 public:
  MockFeedbackSender(net::URLRequestContextGetter* request_context,
                     const std::string& language,
                     const std::string& country)
      : FeedbackSender(request_context, language, country), random_(0) {}

  void RandBytes(void* p, size_t len) override {
    memset(p, 0, len);
    if (len >= sizeof(random_))
      *(unsigned*)p = ++random_;
  }

 private:
  // For returning a different value from each call to RandUint64().
  unsigned random_;
};

std::string GetMisspellingId(const std::string& raw_data) {
  std::unique_ptr<base::Value> parsed_data(
      base::JSONReader::Read(raw_data).release());
  EXPECT_TRUE(parsed_data.get());
  base::DictionaryValue* actual_data;
  EXPECT_TRUE(parsed_data->GetAsDictionary(&actual_data));
  base::ListValue* suggestions = NULL;
  EXPECT_TRUE(actual_data->GetList("params.suggestionInfo", &suggestions));
  base::DictionaryValue* suggestion = NULL;
  EXPECT_TRUE(suggestions->GetDictionary(0, &suggestion));
  std::string value;
  EXPECT_TRUE(suggestion->GetString("userMisspellingId", &value));
  return value;
}

}  // namespace

// A test fixture to help keep tests simple.
class FeedbackSenderTest : public testing::Test {
 public:
  FeedbackSenderTest() : ui_thread_(content::BrowserThread::UI, &loop_) {
    feedback_.reset(new MockFeedbackSender(NULL, kLanguage, kCountry));
    feedback_->StartFeedbackCollection();
  }

  ~FeedbackSenderTest() override {}

 protected:
  // Appends the "--enable-spelling-service-feedback" switch to the
  // command-line.
  void AppendCommandLineSwitch() {
    // The command-line switch is temporary.
    // TODO(rouslan): Remove the command-line switch. http://crbug.com/247726
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableSpellingFeedbackFieldTrial);
    feedback_.reset(new MockFeedbackSender(NULL, kLanguage, kCountry));
    feedback_->StartFeedbackCollection();
  }

  // Enables the "SpellingServiceFeedback.Enabled" field trial.
  void EnableFieldTrial() {
    // The field trial is temporary.
    // TODO(rouslan): Remove the field trial. http://crbug.com/247726
    field_trial_list_.reset(
        new base::FieldTrialList(new metrics::SHA1EntropyProvider("foo")));
    field_trial_ = base::FieldTrialList::CreateFieldTrial(
        kFeedbackFieldTrialName, kFeedbackFieldTrialEnabledGroupName);
    field_trial_->group();
    feedback_.reset(new MockFeedbackSender(NULL, kLanguage, kCountry));
    feedback_->StartFeedbackCollection();
  }

  uint32_t AddPendingFeedback() {
    std::vector<SpellCheckResult> results(1, BuildSpellCheckResult());
    feedback_->OnSpellcheckResults(kRendererProcessId,
                                   base::UTF8ToUTF16(kText),
                                   std::vector<SpellCheckMarker>(),
                                   &results);
    return results[0].hash;
  }

  void ExpireSession() {
    feedback_->session_start_ =
        base::Time::Now() -
        base::TimeDelta::FromHours(chrome::spellcheck_common::kSessionHours);
  }

  bool UploadDataContains(const std::string& data) const {
    const net::TestURLFetcher* fetcher =
        fetchers_.GetFetcherByID(kUrlFetcherId);
    return fetcher && CountOccurences(fetcher->upload_data(), data) > 0;
  }

  bool UploadDataContains(const std::string& data,
                          int number_of_occurrences) const {
    const net::TestURLFetcher* fetcher =
        fetchers_.GetFetcherByID(kUrlFetcherId);
    return fetcher && CountOccurences(fetcher->upload_data(), data) ==
                          number_of_occurrences;
  }

  // Returns true if the feedback sender would be uploading data now. The test
  // does not open network connections.
  bool IsUploadingData() const {
    return !!fetchers_.GetFetcherByID(kUrlFetcherId);
  }

  void ClearUploadData() {
    fetchers_.RemoveFetcherFromMap(kUrlFetcherId);
  }

  std::string GetUploadData() const {
    const net::TestURLFetcher* fetcher =
        fetchers_.GetFetcherByID(kUrlFetcherId);
    return fetcher ? fetcher->upload_data() : std::string();
  }

  void AdjustUpdateTime(base::TimeDelta offset) {
    feedback_->last_salt_update_ += offset;
  }

  std::unique_ptr<MockFeedbackSender> feedback_;

 private:
  base::MessageLoop loop_;
  TestingProfile profile_;
  content::TestBrowserThread ui_thread_;
  std::unique_ptr<base::FieldTrialList> field_trial_list_;
  scoped_refptr<base::FieldTrial> field_trial_;
  net::TestURLFetcherFactory fetchers_;
};

// Do not send data if there's no feedback.
TEST_F(FeedbackSenderTest, NoFeedback) {
  EXPECT_FALSE(IsUploadingData());
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32_t>());
  EXPECT_FALSE(IsUploadingData());
}

// Do not send data if not aware of which markers are still in the document.
TEST_F(FeedbackSenderTest, NoDocumentMarkersReceived) {
  EXPECT_FALSE(IsUploadingData());
  uint32_t hash = AddPendingFeedback();
  EXPECT_FALSE(IsUploadingData());
  static const int kSuggestionIndex = 1;
  feedback_->SelectedSuggestion(hash, kSuggestionIndex);
  EXPECT_FALSE(IsUploadingData());
}

// Send PENDING feedback message if the marker is still in the document, and the
// user has not performed any action on it.
TEST_F(FeedbackSenderTest, PendingFeedback) {
  uint32_t hash = AddPendingFeedback();
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32_t>(1, hash));
  EXPECT_TRUE(UploadDataContains("\"actionType\":\"PENDING\""));
}

TEST_F(FeedbackSenderTest, IdenticalFeedback) {
  std::vector<uint32_t> hashes;
  hashes.push_back(AddPendingFeedback());
  hashes.push_back(AddPendingFeedback());
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId, hashes);
  std::string actual_data = GetUploadData();
  std::unique_ptr<base::DictionaryValue> actual(
      static_cast<base::DictionaryValue*>(
          base::JSONReader::Read(GetUploadData()).release()));
  base::ListValue* suggestions = NULL;
  ASSERT_TRUE(actual->GetList("params.suggestionInfo", &suggestions));
  base::DictionaryValue* suggestion0 = NULL;
  ASSERT_TRUE(suggestions->GetDictionary(0, &suggestion0));
  base::DictionaryValue* suggestion1 = NULL;
  ASSERT_TRUE(suggestions->GetDictionary(0, &suggestion1));
  std::string value0, value1;
  ASSERT_TRUE(suggestion0->GetString("userMisspellingId", &value0));
  ASSERT_TRUE(suggestion1->GetString("userMisspellingId", &value1));
  EXPECT_EQ(value0, value1);
  base::ListValue* suggestion_ids = NULL;
  ASSERT_TRUE(suggestion0->GetList("userSuggestionId", &suggestion_ids));
  ASSERT_TRUE(suggestion_ids->GetString(0, &value0));
  ASSERT_TRUE(suggestion1->GetList("userSuggestionId", &suggestion_ids));
  ASSERT_TRUE(suggestion_ids->GetString(0, &value1));
  EXPECT_EQ(value0, value1);
}

TEST_F(FeedbackSenderTest, NonidenticalFeedback) {
  std::vector<uint32_t> hashes;
  hashes.push_back(AddPendingFeedback());
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId, hashes);
  std::string raw_data0 = GetUploadData();
  hashes.clear();
  hashes.push_back(AddPendingFeedback());
  AdjustUpdateTime(-base::TimeDelta::FromHours(25));
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId, hashes);
  std::string raw_data1 = GetUploadData();

  std::string value0(GetMisspellingId(raw_data0));
  std::string value1(GetMisspellingId(raw_data1));
  EXPECT_NE(value0, value1);
}

// Send NO_ACTION feedback message if the marker has been removed from the
// document.
TEST_F(FeedbackSenderTest, NoActionFeedback) {
  AddPendingFeedback();
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32_t>());
  EXPECT_TRUE(UploadDataContains("\"actionType\":\"NO_ACTION\""));
}

// Send SELECT feedback message if the user has selected a spelling suggestion.
TEST_F(FeedbackSenderTest, SelectFeedback) {
  uint32_t hash = AddPendingFeedback();
  static const int kSuggestionIndex = 0;
  feedback_->SelectedSuggestion(hash, kSuggestionIndex);
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32_t>());
  EXPECT_TRUE(UploadDataContains("\"actionType\":\"SELECT\""));
  EXPECT_TRUE(UploadDataContains("\"actionTargetIndex\":" +
                                 base::StringPrintf("%d", kSuggestionIndex)));
}

// Send ADD_TO_DICT feedback message if the user has added the misspelled word
// to the custom dictionary.
TEST_F(FeedbackSenderTest, AddToDictFeedback) {
  uint32_t hash = AddPendingFeedback();
  feedback_->AddedToDictionary(hash);
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32_t>());
  EXPECT_TRUE(UploadDataContains("\"actionType\":\"ADD_TO_DICT\""));
}

// Send IN_DICTIONARY feedback message if the user has the misspelled word in
// the custom dictionary.
TEST_F(FeedbackSenderTest, InDictionaryFeedback) {
  uint32_t hash = AddPendingFeedback();
  feedback_->RecordInDictionary(hash);
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32_t>());
  EXPECT_TRUE(UploadDataContains("\"actionType\":\"IN_DICTIONARY\""));
}

// Send PENDING feedback message if the user saw the spelling suggestion, but
// decided to not select it, and the marker is still in the document.
TEST_F(FeedbackSenderTest, IgnoreFeedbackMarkerInDocument) {
  uint32_t hash = AddPendingFeedback();
  feedback_->IgnoredSuggestions(hash);
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32_t>(1, hash));
  EXPECT_TRUE(UploadDataContains("\"actionType\":\"PENDING\""));
}

// Send IGNORE feedback message if the user saw the spelling suggestion, but
// decided to not select it, and the marker is no longer in the document.
TEST_F(FeedbackSenderTest, IgnoreFeedbackMarkerNotInDocument) {
  uint32_t hash = AddPendingFeedback();
  feedback_->IgnoredSuggestions(hash);
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32_t>());
  EXPECT_TRUE(UploadDataContains("\"actionType\":\"IGNORE\""));
}

// Send MANUALLY_CORRECTED feedback message if the user manually corrected the
// misspelled word.
TEST_F(FeedbackSenderTest, ManuallyCorrectedFeedback) {
  uint32_t hash = AddPendingFeedback();
  static const std::string kManualCorrection = "Howdy";
  feedback_->ManuallyCorrected(hash, base::ASCIIToUTF16(kManualCorrection));
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32_t>());
  EXPECT_TRUE(UploadDataContains("\"actionType\":\"MANUALLY_CORRECTED\""));
  EXPECT_TRUE(UploadDataContains("\"actionTargetValue\":\"" +
                                 kManualCorrection + "\""));
}

// Send feedback messages in batch.
TEST_F(FeedbackSenderTest, BatchFeedback) {
  std::vector<SpellCheckResult> results;
  results.push_back(SpellCheckResult(SpellCheckResult::SPELLING,
                                     kMisspellingStart,
                                     kMisspellingLength,
                                     base::ASCIIToUTF16("Hello")));
  static const int kSecondMisspellingStart = 7;
  static const int kSecondMisspellingLength = 5;
  results.push_back(SpellCheckResult(SpellCheckResult::SPELLING,
                                     kSecondMisspellingStart,
                                     kSecondMisspellingLength,
                                     base::ASCIIToUTF16("world")));
  feedback_->OnSpellcheckResults(kRendererProcessId,
                                 base::UTF8ToUTF16(kText),
                                 std::vector<SpellCheckMarker>(),
                                 &results);
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32_t>());
  EXPECT_TRUE(UploadDataContains("\"actionType\":\"NO_ACTION\"", 2));
}

// Send a series of PENDING feedback messages and one final NO_ACTION feedback
// message with the same hash identifier for a single misspelling.
TEST_F(FeedbackSenderTest, SameHashFeedback) {
  uint32_t hash = AddPendingFeedback();
  std::vector<uint32_t> remaining_markers(1, hash);

  feedback_->OnReceiveDocumentMarkers(kRendererProcessId, remaining_markers);
  EXPECT_TRUE(UploadDataContains("\"actionType\":\"PENDING\""));
  std::string hash_string = base::StringPrintf("\"suggestionId\":\"%u\"", hash);
  EXPECT_TRUE(UploadDataContains(hash_string));
  ClearUploadData();

  feedback_->OnReceiveDocumentMarkers(kRendererProcessId, remaining_markers);
  EXPECT_TRUE(UploadDataContains("\"actionType\":\"PENDING\""));
  EXPECT_TRUE(UploadDataContains(hash_string));
  ClearUploadData();

  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32_t>());
  EXPECT_TRUE(UploadDataContains("\"actionType\":\"NO_ACTION\""));
  EXPECT_TRUE(UploadDataContains(hash_string));
  ClearUploadData();

  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32_t>());
  EXPECT_FALSE(IsUploadingData());
}

// When a session expires:
// 1) Pending feedback is finalized and sent to the server in the last message
//    batch in the session.
// 2) No feedback is sent until a spellcheck request happens.
// 3) Existing markers get new hash identifiers.
TEST_F(FeedbackSenderTest, SessionExpirationFeedback) {
  std::vector<SpellCheckResult> results(
      1,
      SpellCheckResult(SpellCheckResult::SPELLING,
                       kMisspellingStart,
                       kMisspellingLength,
                       base::ASCIIToUTF16("Hello")));
  feedback_->OnSpellcheckResults(kRendererProcessId,
                                 base::UTF8ToUTF16(kText),
                                 std::vector<SpellCheckMarker>(),
                                 &results);
  uint32_t original_hash = results[0].hash;
  std::vector<uint32_t> remaining_markers(1, original_hash);

  feedback_->OnReceiveDocumentMarkers(kRendererProcessId, remaining_markers);
  EXPECT_FALSE(UploadDataContains("\"actionType\":\"NO_ACTION\""));
  EXPECT_TRUE(UploadDataContains("\"actionType\":\"PENDING\""));
  std::string original_hash_string =
      base::StringPrintf("\"suggestionId\":\"%u\"", original_hash);
  EXPECT_TRUE(UploadDataContains(original_hash_string));
  ClearUploadData();

  ExpireSession();

  // Last message batch in the current session has only finalized messages.
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId, remaining_markers);
  EXPECT_TRUE(UploadDataContains("\"actionType\":\"NO_ACTION\""));
  EXPECT_FALSE(UploadDataContains("\"actionType\":\"PENDING\""));
  EXPECT_TRUE(UploadDataContains(original_hash_string));
  ClearUploadData();

  // The next session starts on the next spellchecker request. Until then,
  // there's no more feedback sent.
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId, remaining_markers);
  EXPECT_FALSE(IsUploadingData());

  // The first spellcheck request after session expiration creates different
  // document marker hash identifiers.
  std::vector<SpellCheckMarker> original_markers(
      1, SpellCheckMarker(results[0].hash, results[0].location));
  results[0] = SpellCheckResult(SpellCheckResult::SPELLING,
                                kMisspellingStart,
                                kMisspellingLength,
                                base::ASCIIToUTF16("Hello"));
  feedback_->OnSpellcheckResults(
      kRendererProcessId, base::UTF8ToUTF16(kText), original_markers, &results);
  uint32_t updated_hash = results[0].hash;
  EXPECT_NE(updated_hash, original_hash);
  remaining_markers[0] = updated_hash;

  // The first feedback message batch in session |i + 1| has the new document
  // marker hash identifiers.
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId, remaining_markers);
  EXPECT_FALSE(UploadDataContains("\"actionType\":\"NO_ACTION\""));
  EXPECT_TRUE(UploadDataContains("\"actionType\":\"PENDING\""));
  EXPECT_FALSE(UploadDataContains(original_hash_string));
  std::string updated_hash_string =
      base::StringPrintf("\"suggestionId\":\"%u\"", updated_hash);
  EXPECT_TRUE(UploadDataContains(updated_hash_string));
}

// First message in session has an indicator.
TEST_F(FeedbackSenderTest, FirstMessageInSessionIndicator) {
  // Session 1, message 1
  AddPendingFeedback();
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32_t>());
  EXPECT_TRUE(UploadDataContains("\"isFirstInSession\":true"));

  // Session 1, message 2
  AddPendingFeedback();
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32_t>());
  EXPECT_TRUE(UploadDataContains("\"isFirstInSession\":false"));

  ExpireSession();

  // Session 1, message 3 (last)
  AddPendingFeedback();
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32_t>());
  EXPECT_TRUE(UploadDataContains("\"isFirstInSession\":false"));

  // Session 2, message 1
  AddPendingFeedback();
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32_t>());
  EXPECT_TRUE(UploadDataContains("\"isFirstInSession\":true"));

  // Session 2, message 2
  AddPendingFeedback();
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32_t>());
  EXPECT_TRUE(UploadDataContains("\"isFirstInSession\":false"));
}

// Flush all feedback when the spellcheck language and country change.
TEST_F(FeedbackSenderTest, OnLanguageCountryChange) {
  AddPendingFeedback();
  feedback_->OnLanguageCountryChange("pt", "BR");
  EXPECT_TRUE(UploadDataContains("\"language\":\"en\""));
  AddPendingFeedback();
  feedback_->OnLanguageCountryChange("en", "US");
  EXPECT_TRUE(UploadDataContains("\"language\":\"pt\""));
}

// The field names and types should correspond to the API.
TEST_F(FeedbackSenderTest, FeedbackAPI) {
  AddPendingFeedback();
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32_t>());
  std::string actual_data = GetUploadData();
  std::unique_ptr<base::DictionaryValue> actual(
      static_cast<base::DictionaryValue*>(
          base::JSONReader::Read(actual_data).release()));
  actual->SetString("params.key", "TestDummyKey");
  base::ListValue* suggestions = nullptr;
  actual->GetList("params.suggestionInfo", &suggestions);
  base::DictionaryValue* suggestion = nullptr;
  suggestions->GetDictionary(0, &suggestion);
  suggestion->SetString("suggestionId", "42");
  suggestion->SetString("timestamp", "9001");
  static const std::string expected_data =
      "{\"apiVersion\":\"v2\","
      "\"method\":\"spelling.feedback\","
      "\"params\":"
      "{\"clientName\":\"Chrome\","
      "\"originCountry\":\"USA\","
      "\"key\":\"TestDummyKey\","
      "\"language\":\"en\","
      "\"suggestionInfo\":[{"
      "\"isAutoCorrection\":false,"
      "\"isFirstInSession\":true,"
      "\"misspelledLength\":6,"
      "\"misspelledStart\":0,"
      "\"originalText\":\"Helllo world\","
      "\"suggestionId\":\"42\","
      "\"suggestions\":[\"Hello\"],"
      "\"timestamp\":\"9001\","
      "\"userActions\":[{\"actionType\":\"NO_ACTION\"}],"
      "\"userMisspellingId\":\"14573599553589145012\","
      "\"userSuggestionId\":[\"14761077877524043800\"]}]}}";
  std::unique_ptr<base::Value> expected = base::JSONReader::Read(expected_data);
  EXPECT_TRUE(expected->Equals(actual.get()))
      << "Expected data: " << expected_data
      << "\nActual data:   " << actual_data;
}

// The default API version is "v2".
TEST_F(FeedbackSenderTest, DefaultApiVersion) {
  AddPendingFeedback();
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32_t>());
  EXPECT_TRUE(UploadDataContains("\"apiVersion\":\"v2\""));
  EXPECT_FALSE(UploadDataContains("\"apiVersion\":\"v2-internal\""));
}

// The API version should not change for field-trial participants that do not
// append the command-line switch.
TEST_F(FeedbackSenderTest, FieldTrialAloneHasSameApiVersion) {
  EnableFieldTrial();

  AddPendingFeedback();
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32_t>());

  EXPECT_TRUE(UploadDataContains("\"apiVersion\":\"v2\""));
  EXPECT_FALSE(UploadDataContains("\"apiVersion\":\"v2-internal\""));
}

// The API version should not change if the command-line switch is appended, but
// the user is not participating in the field-trial.
TEST_F(FeedbackSenderTest, CommandLineSwitchAloneHasSameApiVersion) {
  AppendCommandLineSwitch();

  AddPendingFeedback();
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32_t>());

  EXPECT_TRUE(UploadDataContains("\"apiVersion\":\"v2\""));
  EXPECT_FALSE(UploadDataContains("\"apiVersion\":\"v2-internal\""));
}

// The API version should be different for field-trial participants that also
// append the command-line switch.
TEST_F(FeedbackSenderTest, InternalApiVersion) {
  AppendCommandLineSwitch();
  EnableFieldTrial();

  AddPendingFeedback();
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32_t>());

  EXPECT_FALSE(UploadDataContains("\"apiVersion\":\"v2\""));
  EXPECT_TRUE(UploadDataContains("\"apiVersion\":\"v2-internal\""));
}

// Duplicate spellcheck results should be matched to the existing markers.
TEST_F(FeedbackSenderTest, MatchDupliateResultsWithExistingMarkers) {
  uint32_t hash = AddPendingFeedback();
  std::vector<SpellCheckResult> results(
      1,
      SpellCheckResult(SpellCheckResult::SPELLING,
                       kMisspellingStart,
                       kMisspellingLength,
                       base::ASCIIToUTF16("Hello")));
  std::vector<SpellCheckMarker> markers(
      1, SpellCheckMarker(hash, results[0].location));
  EXPECT_EQ(static_cast<uint32_t>(0), results[0].hash);
  feedback_->OnSpellcheckResults(
      kRendererProcessId, base::UTF8ToUTF16(kText), markers, &results);
  EXPECT_EQ(hash, results[0].hash);
}

// Adding a word to dictionary should trigger ADD_TO_DICT feedback for every
// occurrence of that word.
TEST_F(FeedbackSenderTest, MultipleAddToDictFeedback) {
  std::vector<SpellCheckResult> results;
  static const int kSentenceLength = 14;
  static const int kNumberOfSentences = 2;
  static const base::string16 kTextWithDuplicates =
      base::ASCIIToUTF16("Helllo world. Helllo world.");
  for (int i = 0; i < kNumberOfSentences; ++i) {
    results.push_back(SpellCheckResult(SpellCheckResult::SPELLING,
                                       kMisspellingStart + i * kSentenceLength,
                                       kMisspellingLength,
                                       base::ASCIIToUTF16("Hello")));
  }
  static const int kNumberOfRenderers = 2;
  int last_renderer_process_id = -1;
  for (int i = 0; i < kNumberOfRenderers; ++i) {
    feedback_->OnSpellcheckResults(kRendererProcessId + i,
                                   kTextWithDuplicates,
                                   std::vector<SpellCheckMarker>(),
                                   &results);
    last_renderer_process_id = kRendererProcessId + i;
  }
  std::vector<uint32_t> remaining_markers;
  for (size_t i = 0; i < results.size(); ++i)
    remaining_markers.push_back(results[i].hash);
  feedback_->OnReceiveDocumentMarkers(last_renderer_process_id,
                                      remaining_markers);
  EXPECT_TRUE(UploadDataContains("PENDING", 2));
  EXPECT_FALSE(UploadDataContains("ADD_TO_DICT"));

  feedback_->AddedToDictionary(results[0].hash);
  feedback_->OnReceiveDocumentMarkers(last_renderer_process_id,
                                      remaining_markers);
  EXPECT_FALSE(UploadDataContains("PENDING"));
  EXPECT_TRUE(UploadDataContains("ADD_TO_DICT", 2));
}

// ADD_TO_DICT feedback for multiple occurrences of a word should trigger only
// for pending feedback.
TEST_F(FeedbackSenderTest, AddToDictOnlyPending) {
  AddPendingFeedback();
  uint32_t add_to_dict_hash = AddPendingFeedback();
  uint32_t select_hash = AddPendingFeedback();
  feedback_->SelectedSuggestion(select_hash, 0);
  feedback_->AddedToDictionary(add_to_dict_hash);
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32_t>());
  EXPECT_TRUE(UploadDataContains("SELECT", 1));
  EXPECT_TRUE(UploadDataContains("ADD_TO_DICT", 2));
}

// Spellcheck results that are out-of-bounds are not added to feedback.
TEST_F(FeedbackSenderTest, IgnoreOutOfBounds) {
  std::vector<SpellCheckResult> results;
  results.push_back(SpellCheckResult(
      SpellCheckResult::SPELLING, 0, 100, base::UTF8ToUTF16("Hello")));
  results.push_back(SpellCheckResult(
      SpellCheckResult::SPELLING, 100, 3, base::UTF8ToUTF16("world")));
  results.push_back(SpellCheckResult(
      SpellCheckResult::SPELLING, -1, 3, base::UTF8ToUTF16("how")));
  results.push_back(SpellCheckResult(
      SpellCheckResult::SPELLING, 0, 0, base::UTF8ToUTF16("are")));
  results.push_back(SpellCheckResult(
      SpellCheckResult::SPELLING, 2, -1, base::UTF8ToUTF16("you")));
  feedback_->OnSpellcheckResults(kRendererProcessId,
                                 base::UTF8ToUTF16(kText),
                                 std::vector<SpellCheckMarker>(),
                                 &results);
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32_t>());
  EXPECT_FALSE(IsUploadingData());
}

// FeedbackSender does not collect and upload feedback when instructed to stop.
TEST_F(FeedbackSenderTest, CanStopFeedbackCollection) {
  feedback_->StopFeedbackCollection();
  AddPendingFeedback();
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32_t>());
  EXPECT_FALSE(IsUploadingData());
}

// FeedbackSender resumes collecting and uploading feedback when instructed to
// start after stopping.
TEST_F(FeedbackSenderTest, CanResumeFeedbackCollection) {
  feedback_->StopFeedbackCollection();
  feedback_->StartFeedbackCollection();
  AddPendingFeedback();
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32_t>());
  EXPECT_TRUE(IsUploadingData());
}

// FeedbackSender does not collect data while being stopped and upload it later.
TEST_F(FeedbackSenderTest, NoFeedbackCollectionWhenStopped) {
  feedback_->StopFeedbackCollection();
  AddPendingFeedback();
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32_t>());
  feedback_->StartFeedbackCollection();
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32_t>());
  EXPECT_FALSE(IsUploadingData());
}

// The feedback context is trimmed to 2 words on the left and 2 words on the
// right side of the misspelling.
TEST_F(FeedbackSenderTest, TrimFeedback) {
  std::vector<SpellCheckResult> results(
      1, SpellCheckResult(SpellCheckResult::SPELLING, 13, 3,
                          base::UTF8ToUTF16("the")));
  feedback_->OnSpellcheckResults(
      kRendererProcessId,
      base::UTF8ToUTF16("Far and away teh best prize that life has to offer is "
                        "the chance to work hard at work worth doing."),
      std::vector<SpellCheckMarker>(), &results);
  feedback_->OnReceiveDocumentMarkers(kRendererProcessId,
                                      std::vector<uint32_t>());
  EXPECT_TRUE(
      UploadDataContains(",\"originalText\":\"and away teh best prize\","));
  EXPECT_TRUE(UploadDataContains(",\"misspelledStart\":9,"));
}

}  // namespace spellcheck
