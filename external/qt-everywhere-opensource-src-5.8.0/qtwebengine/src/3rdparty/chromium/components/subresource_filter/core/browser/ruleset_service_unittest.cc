// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/browser/ruleset_service.h"

#include <stddef.h>
#include <stdint.h>

#include <iterator>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/prefs/testing_pref_service.h"
#include "components/subresource_filter/core/browser/ruleset_distributor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

namespace {

const char kTestContentVersion1[] = "1.2.3.4";
const char kTestContentVersion2[] = "1.2.3.5";

const uint8_t kDummyRuleset1[] = {11, 0, 12, 13, 14};
const uint8_t kDummyRuleset2[] = {21, 0, 22, 23};

class MockRulesetDistributor : public RulesetDistributor {
 public:
  MockRulesetDistributor() = default;
  ~MockRulesetDistributor() override = default;

  void PublishNewVersion(base::File ruleset_data) override {
    published_rulesets_.push_back(std::move(ruleset_data));
  }

  std::vector<base::File>& published_rulesets() { return published_rulesets_; }

 private:
  std::vector<base::File> published_rulesets_;

  DISALLOW_COPY_AND_ASSIGN(MockRulesetDistributor);
};

std::vector<uint8_t> ReadFileContents(base::File* file) {
  size_t length = base::checked_cast<size_t>(file->GetLength());
  std::vector<uint8_t> contents(length);
  static_assert(sizeof(uint8_t) == sizeof(char), "Expected char = byte.");
  file->Read(0, reinterpret_cast<char*>(contents.data()),
             base::checked_cast<int>(length));
  return contents;
}

template <typename T, int N>
std::vector<T> AsVector(const T (&array)[N]) {
  return std::vector<T>(std::begin(array), std::end(array));
}

}  // namespace

class SubresourceFilteringRulesetServiceTest : public ::testing::Test {
 public:
  SubresourceFilteringRulesetServiceTest()
      : task_runner_(new base::TestSimpleTaskRunner),
        task_runner_handle_(task_runner_),
        mock_distributor_(nullptr) {}

 protected:
  void SetUp() override {
    RulesetVersion::RegisterPrefs(pref_service_.registry());

    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    ResetService(CreateRulesetService());
    RunUntilIdle();
  }

  std::unique_ptr<RulesetService> CreateRulesetService() {
    return base::WrapUnique(
        new RulesetService(&pref_service_, task_runner_, base_dir()));
  }

  void ResetService(std::unique_ptr<RulesetService> new_service = nullptr) {
    service_ = std::move(new_service);
    if (service_) {
      service_->RegisterDistributor(
          base::WrapUnique(mock_distributor_ = new MockRulesetDistributor()));
    } else {
      mock_distributor_ = nullptr;
    }
  }

  void StoreAndPublishUpdatedRuleset(const std::vector<uint8_t>& ruleset_data,
                                     const std::string& new_content_version) {
    service()->StoreAndPublishUpdatedRuleset(ruleset_data, new_content_version);
  }

  void WritePreexistingRuleset(const RulesetVersion& version,
                               const std::vector<uint8_t>& data) {
    RulesetService::WriteRuleset(base_dir(), version, data);
  }

  void RunUntilIdle() { task_runner_->RunUntilIdle(); }

  void RunPendingTasksNTimes(size_t n) {
    while (n--)
      task_runner_->RunPendingTasks();
  }

  void AssertValidRulesetFileWithContents(
      base::File* file,
      const std::vector<uint8_t>& expected_contents) {
    ASSERT_TRUE(file->IsValid());
    ASSERT_EQ(expected_contents, ReadFileContents(file));
  }

  void AssertReadonlyRulesetFile(base::File* file) {
    const char kTest[] = "t";
    ASSERT_TRUE(file->IsValid());
    ASSERT_EQ(-1, file->Write(0, kTest, sizeof(kTest)));
  }

  PrefService* prefs() { return &pref_service_; }
  RulesetService* service() { return service_.get(); }
  MockRulesetDistributor* mock_distributor() { return mock_distributor_; }
  base::FilePath base_dir() const {
    return scoped_temp_dir_.path().AppendASCII("Ruleset Base Dir");
  }

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  TestingPrefServiceSimple pref_service_;

  base::ScopedTempDir scoped_temp_dir_;
  std::unique_ptr<RulesetService> service_;
  MockRulesetDistributor* mock_distributor_;  // Weak, owned by |service_|.

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilteringRulesetServiceTest);
};

TEST_F(SubresourceFilteringRulesetServiceTest, Startup_NoRulesetNotPublished) {
  EXPECT_EQ(0u, mock_distributor()->published_rulesets().size());
}

// It should not normally happen that Local State indicates that a usable
// version of the ruleset had been stored, yet the file is nowhere to be found,
// but ensure some sane behavior just in case.
TEST_F(SubresourceFilteringRulesetServiceTest,
       Startup_MissingRulesetNotPublished) {
  RulesetVersion current_version(kTestContentVersion1,
                                 RulesetVersion::CurrentFormatVersion());
  // `Forget` to write ruleset data.
  current_version.SaveToPrefs(prefs());

  ResetService(CreateRulesetService());
  RunUntilIdle();
  EXPECT_EQ(0u, mock_distributor()->published_rulesets().size());
}

TEST_F(SubresourceFilteringRulesetServiceTest,
       Startup_LegacyFormatRulesetNotPublished) {
  int legacy_format_version = RulesetVersion::CurrentFormatVersion() - 1;
  RulesetVersion legacy_version(kTestContentVersion1, legacy_format_version);
  ASSERT_TRUE(legacy_version.IsValid());
  legacy_version.SaveToPrefs(prefs());
  WritePreexistingRuleset(legacy_version, AsVector(kDummyRuleset1));

  ResetService(CreateRulesetService());
  RunUntilIdle();
  EXPECT_EQ(0u, mock_distributor()->published_rulesets().size());
}

TEST_F(SubresourceFilteringRulesetServiceTest,
       Startup_ExistingRulesetPublished) {
  const std::vector<uint8_t> dummy_ruleset(AsVector(kDummyRuleset1));
  RulesetVersion current_version(kTestContentVersion1,
                                 RulesetVersion::CurrentFormatVersion());
  current_version.SaveToPrefs(prefs());
  WritePreexistingRuleset(current_version, AsVector(kDummyRuleset1));

  ResetService(CreateRulesetService());
  RunUntilIdle();

  ASSERT_EQ(1u, mock_distributor()->published_rulesets().size());
  ASSERT_NO_FATAL_FAILURE(AssertValidRulesetFileWithContents(
      &mock_distributor()->published_rulesets()[0], dummy_ruleset));
}

TEST_F(SubresourceFilteringRulesetServiceTest, NewRuleset_Published) {
  const std::vector<uint8_t> dummy_ruleset(AsVector(kDummyRuleset1));
  service()->StoreAndPublishUpdatedRuleset(dummy_ruleset, kTestContentVersion1);
  RunUntilIdle();

  ASSERT_EQ(1u, mock_distributor()->published_rulesets().size());
  ASSERT_NO_FATAL_FAILURE(AssertValidRulesetFileWithContents(
      &mock_distributor()->published_rulesets()[0], dummy_ruleset));

  MockRulesetDistributor* new_distributor = new MockRulesetDistributor;
  service()->RegisterDistributor(base::WrapUnique(new_distributor));
  ASSERT_EQ(1u, new_distributor->published_rulesets().size());
  ASSERT_NO_FATAL_FAILURE(AssertValidRulesetFileWithContents(
      &new_distributor->published_rulesets()[0], dummy_ruleset));
}

TEST_F(SubresourceFilteringRulesetServiceTest, NewRuleset_Persisted) {
  const std::vector<uint8_t> dummy_ruleset(AsVector(kDummyRuleset1));
  service()->StoreAndPublishUpdatedRuleset(dummy_ruleset, kTestContentVersion1);
  RunUntilIdle();

  RulesetVersion stored_version;
  stored_version.ReadFromPrefs(prefs());
  EXPECT_EQ(kTestContentVersion1, stored_version.content_version);
  EXPECT_EQ(RulesetVersion::CurrentFormatVersion(),
            stored_version.format_version);

  ResetService(CreateRulesetService());
  RunUntilIdle();

  ASSERT_EQ(1u, mock_distributor()->published_rulesets().size());
  ASSERT_NO_FATAL_FAILURE(AssertValidRulesetFileWithContents(
      &mock_distributor()->published_rulesets()[0], dummy_ruleset));
}

TEST_F(SubresourceFilteringRulesetServiceTest,
       NewRulesetTwice_SecondRulesetPrevails) {
  const std::vector<uint8_t> dummy_ruleset_1(AsVector(kDummyRuleset1));
  const std::vector<uint8_t> dummy_ruleset_2(AsVector(kDummyRuleset2));

  StoreAndPublishUpdatedRuleset(dummy_ruleset_1, kTestContentVersion1);
  RunUntilIdle();

  StoreAndPublishUpdatedRuleset(dummy_ruleset_2, kTestContentVersion2);
  RunUntilIdle();

  // This verifies that the contents from the first version of the ruleset file
  // can still be read after it has been deprecated.
  ASSERT_EQ(2u, mock_distributor()->published_rulesets().size());
  ASSERT_NO_FATAL_FAILURE(AssertValidRulesetFileWithContents(
      &mock_distributor()->published_rulesets()[0], dummy_ruleset_1));
  ASSERT_NO_FATAL_FAILURE(AssertValidRulesetFileWithContents(
      &mock_distributor()->published_rulesets()[1], dummy_ruleset_2));

  MockRulesetDistributor* new_distributor = new MockRulesetDistributor;
  service()->RegisterDistributor(base::WrapUnique(new_distributor));
  ASSERT_EQ(1u, new_distributor->published_rulesets().size());
  ASSERT_NO_FATAL_FAILURE(AssertValidRulesetFileWithContents(
      &new_distributor->published_rulesets()[0], dummy_ruleset_2));

  RulesetVersion stored_version;
  stored_version.ReadFromPrefs(prefs());
  EXPECT_EQ(kTestContentVersion2, stored_version.content_version);
}

TEST_F(SubresourceFilteringRulesetServiceTest,
       NewRulesetSetShortlyBeforeDestruction_NoCrashes) {
  const std::vector<uint8_t> dummy_ruleset(AsVector(kDummyRuleset1));
  for (size_t num_tasks_inbetween = 0; num_tasks_inbetween < 5u;
       ++num_tasks_inbetween) {
    SCOPED_TRACE(::testing::Message() << "#Tasks: " << num_tasks_inbetween);

    StoreAndPublishUpdatedRuleset(dummy_ruleset, kTestContentVersion1);
    RunPendingTasksNTimes(num_tasks_inbetween);
    ResetService();
    RunUntilIdle();

    base::DeleteFile(base_dir(), false);
    ResetService(CreateRulesetService());
  }
}

TEST_F(SubresourceFilteringRulesetServiceTest,
       NewRulesetTwiceInQuickSuccession_SecondRulesetPrevails) {
  const std::vector<uint8_t> dummy_ruleset_1(AsVector(kDummyRuleset1));
  const std::vector<uint8_t> dummy_ruleset_2(AsVector(kDummyRuleset2));
  for (size_t num_tasks_inbetween = 0; num_tasks_inbetween < 5u;
       ++num_tasks_inbetween) {
    SCOPED_TRACE(::testing::Message() << "#Tasks: " << num_tasks_inbetween);

    StoreAndPublishUpdatedRuleset(dummy_ruleset_1, kTestContentVersion1);
    RunPendingTasksNTimes(num_tasks_inbetween);

    StoreAndPublishUpdatedRuleset(dummy_ruleset_2, kTestContentVersion2);
    RunUntilIdle();

    // Optionally permit a "hazardous" publication of either the old or new
    // version of the ruleset, but the last ruleset message must be the new one.
    ASSERT_LE(1u, mock_distributor()->published_rulesets().size());
    ASSERT_GE(2u, mock_distributor()->published_rulesets().size());
    if (mock_distributor()->published_rulesets().size() == 2) {
      base::File* file = &mock_distributor()->published_rulesets()[0];
      ASSERT_TRUE(file->IsValid());
      EXPECT_THAT(ReadFileContents(file),
                  testing::AnyOf(testing::Eq(dummy_ruleset_1),
                                 testing::Eq(dummy_ruleset_2)));
    }
    ASSERT_NO_FATAL_FAILURE(AssertValidRulesetFileWithContents(
        &mock_distributor()->published_rulesets().back(), dummy_ruleset_2));

    MockRulesetDistributor* new_distributor = new MockRulesetDistributor;
    service()->RegisterDistributor(base::WrapUnique(new_distributor));
    ASSERT_EQ(1u, new_distributor->published_rulesets().size());
    ASSERT_NO_FATAL_FAILURE(AssertValidRulesetFileWithContents(
        &new_distributor->published_rulesets()[0], dummy_ruleset_2));

    RulesetVersion stored_version;
    stored_version.ReadFromPrefs(prefs());
    EXPECT_EQ(kTestContentVersion2, stored_version.content_version);
  }
}

TEST_F(SubresourceFilteringRulesetServiceTest,
       NewRulesetTwiceForTheSameVersion_SuccessAtLeastOnce) {
  const std::vector<uint8_t> dummy_ruleset(AsVector(kDummyRuleset1));

  StoreAndPublishUpdatedRuleset(dummy_ruleset, kTestContentVersion1);
  RunUntilIdle();

  StoreAndPublishUpdatedRuleset(dummy_ruleset, kTestContentVersion1);
  RunUntilIdle();

  ASSERT_LE(1u, mock_distributor()->published_rulesets().size());
  ASSERT_GE(2u, mock_distributor()->published_rulesets().size());
  ASSERT_NO_FATAL_FAILURE(AssertValidRulesetFileWithContents(
      &mock_distributor()->published_rulesets().front(), dummy_ruleset));
  ASSERT_NO_FATAL_FAILURE(AssertValidRulesetFileWithContents(
      &mock_distributor()->published_rulesets().back(), dummy_ruleset));

  MockRulesetDistributor* new_distributor = new MockRulesetDistributor;
  service()->RegisterDistributor(base::WrapUnique(new_distributor));
  ASSERT_EQ(1u, new_distributor->published_rulesets().size());
  ASSERT_NO_FATAL_FAILURE(AssertValidRulesetFileWithContents(
      &new_distributor->published_rulesets()[0], dummy_ruleset));
}

TEST_F(SubresourceFilteringRulesetServiceTest, RulesetIsReadonly) {
  const std::vector<uint8_t> dummy_ruleset(AsVector(kDummyRuleset1));

  service()->StoreAndPublishUpdatedRuleset(dummy_ruleset, kTestContentVersion1);
  RunUntilIdle();

  ASSERT_EQ(1u, mock_distributor()->published_rulesets().size());
  ASSERT_NO_FATAL_FAILURE(
      AssertReadonlyRulesetFile(&mock_distributor()->published_rulesets()[0]));

  MockRulesetDistributor* new_distributor = new MockRulesetDistributor;
  service()->RegisterDistributor(base::WrapUnique(new_distributor));
  ASSERT_EQ(1u, new_distributor->published_rulesets().size());
  ASSERT_NO_FATAL_FAILURE(
      AssertReadonlyRulesetFile(&new_distributor->published_rulesets()[0]));
}

}  // namespace subresource_filter
