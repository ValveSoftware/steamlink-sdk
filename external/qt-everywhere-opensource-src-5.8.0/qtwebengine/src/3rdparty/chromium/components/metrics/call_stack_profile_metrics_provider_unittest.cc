// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/call_stack_profile_metrics_provider.h"

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "components/metrics/proto/chrome_user_metrics_extension.pb.h"
#include "components/variations/entropy_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::StackSamplingProfiler;
using Frame = StackSamplingProfiler::Frame;
using Module = StackSamplingProfiler::Module;
using Profile = StackSamplingProfiler::CallStackProfile;
using Profiles = StackSamplingProfiler::CallStackProfiles;
using Sample = StackSamplingProfiler::Sample;

namespace metrics {

using Params = CallStackProfileMetricsProvider::Params;

// This test fixture enables the field trial that
// CallStackProfileMetricsProvider depends on to report profiles.
class CallStackProfileMetricsProviderTest : public testing::Test {
 public:
  CallStackProfileMetricsProviderTest()
      : field_trial_list_(nullptr) {
    base::FieldTrialList::CreateFieldTrial(
        TestState::kFieldTrialName,
        TestState::kReportProfilesGroupName);
    TestState::ResetStaticStateForTesting();
  }

  ~CallStackProfileMetricsProviderTest() override {}

  // Utility function to append profiles to the metrics provider.
  void AppendProfiles(const Params& params, const Profiles& profiles) {
    CallStackProfileMetricsProvider::GetProfilerCallback(params).Run(profiles);
  }

 private:
  // Exposes field trial/group names from the CallStackProfileMetricsProvider.
  class TestState : public CallStackProfileMetricsProvider {
   public:
    using CallStackProfileMetricsProvider::kFieldTrialName;
    using CallStackProfileMetricsProvider::kReportProfilesGroupName;
    using CallStackProfileMetricsProvider::ResetStaticStateForTesting;
  };

  base::FieldTrialList field_trial_list_;

  DISALLOW_COPY_AND_ASSIGN(CallStackProfileMetricsProviderTest);
};

// Checks that all properties from multiple profiles are filled as expected.
TEST_F(CallStackProfileMetricsProviderTest, MultipleProfiles) {
  const uintptr_t module1_base_address = 0x1000;
  const uintptr_t module2_base_address = 0x2000;
  const uintptr_t module3_base_address = 0x3000;

  const Module profile_modules[][2] = {
    {
      Module(
          module1_base_address,
          "ABCD",
#if defined(OS_WIN)
          base::FilePath(L"c:\\some\\path\\to\\chrome.exe")
#else
          base::FilePath("/some/path/to/chrome")
#endif
      ),
      Module(
          module2_base_address,
          "EFGH",
#if defined(OS_WIN)
          base::FilePath(L"c:\\some\\path\\to\\third_party.dll")
#else
          base::FilePath("/some/path/to/third_party.so")
#endif
      ),
    },
    {
      Module(
          module3_base_address,
          "MNOP",
#if defined(OS_WIN)
          base::FilePath(L"c:\\some\\path\\to\\third_party2.dll")
#else
          base::FilePath("/some/path/to/third_party2.so")
#endif
      ),
      Module( // Repeated from the first profile.
          module1_base_address,
          "ABCD",
#if defined(OS_WIN)
          base::FilePath(L"c:\\some\\path\\to\\chrome.exe")
#else
          base::FilePath("/some/path/to/chrome")
#endif
      )
    }
  };

  // Values for Windows generated with:
  // perl -MDigest::MD5=md5 -MEncode=encode
  //     -e 'for(@ARGV){printf "%x\n", unpack "Q>", md5 encode "UTF-16LE", $_}'
  //     chrome.exe third_party.dll third_party2.dll
  //
  // Values for Linux generated with:
  // perl -MDigest::MD5=md5
  //     -e 'for(@ARGV){printf "%x\n", unpack "Q>", md5 $_}'
  //     chrome third_party.so third_party2.so
  const uint64_t profile_expected_name_md5_prefixes[][2] = {
      {
#if defined(OS_WIN)
          0x46c3e4166659ac02ULL, 0x7e2b8bfddeae1abaULL
#else
          0x554838a8451ac36cUL, 0x843661148659c9f8UL
#endif
      },
      {
#if defined(OS_WIN)
          0x87b66f4573a4d5caULL, 0x46c3e4166659ac02ULL
#else
          0xb4647e539fa6ec9eUL, 0x554838a8451ac36cUL
#endif
      }};

  // Represents two stack samples for each of two profiles, where each stack
  // contains three frames. Each frame contains an instruction pointer and a
  // module index corresponding to the module for the profile in
  // profile_modules.
  //
  // So, the first stack sample below has its top frame in module 0 at an offset
  // of 0x10 from the module's base address, the next-to-top frame in module 1
  // at an offset of 0x20 from the module's base address, and the bottom frame
  // in module 0 at an offset of 0x30 from the module's base address
  const Frame profile_sample_frames[][2][3] = {
    {
      {
        Frame(module1_base_address + 0x10, 0),
        Frame(module2_base_address + 0x20, 1),
        Frame(module1_base_address + 0x30, 0)
      },
      {
        Frame(module2_base_address + 0x10, 1),
        Frame(module1_base_address + 0x20, 0),
        Frame(module2_base_address + 0x30, 1)
      }
    },
    {
      {
        Frame(module3_base_address + 0x10, 0),
        Frame(module1_base_address + 0x20, 1),
        Frame(module3_base_address + 0x30, 0)
      },
      {
        Frame(module1_base_address + 0x10, 1),
        Frame(module3_base_address + 0x20, 0),
        Frame(module1_base_address + 0x30, 1)
      }
    }
  };

  base::TimeDelta profile_durations[2] = {
    base::TimeDelta::FromMilliseconds(100),
    base::TimeDelta::FromMilliseconds(200)
  };

  base::TimeDelta profile_sampling_periods[2] = {
    base::TimeDelta::FromMilliseconds(10),
    base::TimeDelta::FromMilliseconds(20)
  };

  std::vector<Profile> profiles;
  for (size_t i = 0; i < arraysize(profile_sample_frames); ++i) {
    Profile profile;
    profile.modules.insert(
        profile.modules.end(), &profile_modules[i][0],
        &profile_modules[i][0] + arraysize(profile_modules[i]));

    for (size_t j = 0; j < arraysize(profile_sample_frames[i]); ++j) {
      profile.samples.push_back(Sample());
      Sample& sample = profile.samples.back();
      sample.insert(sample.end(), &profile_sample_frames[i][j][0],
                    &profile_sample_frames[i][j][0] +
                    arraysize(profile_sample_frames[i][j]));
    }

    profile.profile_duration = profile_durations[i];
    profile.sampling_period = profile_sampling_periods[i];

    profiles.push_back(profile);
  }

  CallStackProfileMetricsProvider provider;
  provider.OnRecordingEnabled();
  AppendProfiles(
      Params(CallStackProfileMetricsProvider::PROCESS_STARTUP, false),
      profiles);
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideGeneralMetrics(&uma_proto);

  ASSERT_EQ(static_cast<int>(arraysize(profile_sample_frames)),
            uma_proto.sampled_profile().size());
  for (size_t i = 0; i < arraysize(profile_sample_frames); ++i) {
    SCOPED_TRACE("profile " + base::SizeTToString(i));
    const SampledProfile& sampled_profile = uma_proto.sampled_profile().Get(i);
    ASSERT_TRUE(sampled_profile.has_call_stack_profile());
    const CallStackProfile& call_stack_profile =
        sampled_profile.call_stack_profile();

    ASSERT_EQ(static_cast<int>(arraysize(profile_sample_frames[i])),
              call_stack_profile.sample().size());
    for (size_t j = 0; j < arraysize(profile_sample_frames[i]); ++j) {
      SCOPED_TRACE("sample " + base::SizeTToString(j));
      const CallStackProfile::Sample& proto_sample =
          call_stack_profile.sample().Get(j);
      ASSERT_EQ(static_cast<int>(arraysize(profile_sample_frames[i][j])),
                proto_sample.entry().size());
      ASSERT_TRUE(proto_sample.has_count());
      EXPECT_EQ(1u, proto_sample.count());
      for (size_t k = 0; k < arraysize(profile_sample_frames[i][j]); ++k) {
        SCOPED_TRACE("frame " + base::SizeTToString(k));
        const CallStackProfile::Entry& entry = proto_sample.entry().Get(k);
        ASSERT_TRUE(entry.has_address());
        const char* instruction_pointer = reinterpret_cast<const char*>(
            profile_sample_frames[i][j][k].instruction_pointer);
        const char* module_base_address = reinterpret_cast<const char*>(
            profile_modules[i][profile_sample_frames[i][j][k].module_index]
            .base_address);
        EXPECT_EQ(
            static_cast<uint64_t>(instruction_pointer - module_base_address),
            entry.address());
        ASSERT_TRUE(entry.has_module_id_index());
        EXPECT_EQ(profile_sample_frames[i][j][k].module_index,
                  static_cast<size_t>(entry.module_id_index()));
      }
    }

    ASSERT_EQ(static_cast<int>(arraysize(profile_modules[i])),
              call_stack_profile.module_id().size());
    for (size_t j = 0; j < arraysize(profile_modules[i]); ++j) {
      SCOPED_TRACE("module " + base::SizeTToString(j));
      const CallStackProfile::ModuleIdentifier& module_identifier =
          call_stack_profile.module_id().Get(j);
      ASSERT_TRUE(module_identifier.has_build_id());
      EXPECT_EQ(profile_modules[i][j].id, module_identifier.build_id());
      ASSERT_TRUE(module_identifier.has_name_md5_prefix());
      EXPECT_EQ(profile_expected_name_md5_prefixes[i][j],
                module_identifier.name_md5_prefix());
    }

    ASSERT_TRUE(call_stack_profile.has_profile_duration_ms());
    EXPECT_EQ(profile_durations[i].InMilliseconds(),
              call_stack_profile.profile_duration_ms());
    ASSERT_TRUE(call_stack_profile.has_sampling_period_ms());
    EXPECT_EQ(profile_sampling_periods[i].InMilliseconds(),
              call_stack_profile.sampling_period_ms());
    ASSERT_TRUE(sampled_profile.has_trigger_event());
    EXPECT_EQ(SampledProfile::PROCESS_STARTUP, sampled_profile.trigger_event());
  }
}

// Checks that all duplicate samples are collapsed with
// preserve_sample_ordering = false.
TEST_F(CallStackProfileMetricsProviderTest, RepeatedStacksUnordered) {
  const uintptr_t module_base_address = 0x1000;

  const Module modules[] = {
    Module(
        module_base_address,
        "ABCD",
#if defined(OS_WIN)
        base::FilePath(L"c:\\some\\path\\to\\chrome.exe")
#else
        base::FilePath("/some/path/to/chrome")
#endif
    )
  };

  // Duplicate samples in slots 0, 2, and 3.
  const Frame sample_frames[][1] = {
    { Frame(module_base_address + 0x10, 0), },
    { Frame(module_base_address + 0x20, 0), },
    { Frame(module_base_address + 0x10, 0), },
    { Frame(module_base_address + 0x10, 0) }
  };

  Profile profile;
  profile.modules.insert(profile.modules.end(), &modules[0],
      &modules[0] + arraysize(modules));

  for (size_t i = 0; i < arraysize(sample_frames); ++i) {
    profile.samples.push_back(Sample());
    Sample& sample = profile.samples.back();
    sample.insert(sample.end(), &sample_frames[i][0],
                  &sample_frames[i][0] + arraysize(sample_frames[i]));
  }

  profile.profile_duration = base::TimeDelta::FromMilliseconds(100);
  profile.sampling_period = base::TimeDelta::FromMilliseconds(10);

  CallStackProfileMetricsProvider provider;
  provider.OnRecordingEnabled();
  AppendProfiles(
      Params(CallStackProfileMetricsProvider::PROCESS_STARTUP, false),
      std::vector<Profile>(1, profile));
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideGeneralMetrics(&uma_proto);

  ASSERT_EQ(1, uma_proto.sampled_profile().size());
  const SampledProfile& sampled_profile = uma_proto.sampled_profile().Get(0);
  ASSERT_TRUE(sampled_profile.has_call_stack_profile());
  const CallStackProfile& call_stack_profile =
      sampled_profile.call_stack_profile();

  ASSERT_EQ(2, call_stack_profile.sample().size());
  for (int i = 0; i < 2; ++i) {
    SCOPED_TRACE("sample " + base::IntToString(i));
    const CallStackProfile::Sample& proto_sample =
        call_stack_profile.sample().Get(i);
    ASSERT_EQ(static_cast<int>(arraysize(sample_frames[i])),
              proto_sample.entry().size());
    ASSERT_TRUE(proto_sample.has_count());
    EXPECT_EQ(i == 0 ? 3u : 1u, proto_sample.count());
    for (size_t j = 0; j < arraysize(sample_frames[i]); ++j) {
      SCOPED_TRACE("frame " + base::SizeTToString(j));
      const CallStackProfile::Entry& entry = proto_sample.entry().Get(j);
      ASSERT_TRUE(entry.has_address());
      const char* instruction_pointer = reinterpret_cast<const char*>(
          sample_frames[i][j].instruction_pointer);
      const char* module_base_address = reinterpret_cast<const char*>(
          modules[sample_frames[i][j].module_index].base_address);
      EXPECT_EQ(
          static_cast<uint64_t>(instruction_pointer - module_base_address),
          entry.address());
      ASSERT_TRUE(entry.has_module_id_index());
      EXPECT_EQ(sample_frames[i][j].module_index,
                static_cast<size_t>(entry.module_id_index()));
    }
  }
}

// Checks that only contiguous duplicate samples are collapsed with
// preserve_sample_ordering = true.
TEST_F(CallStackProfileMetricsProviderTest, RepeatedStacksOrdered) {
  const uintptr_t module_base_address = 0x1000;

  const Module modules[] = {
    Module(
        module_base_address,
        "ABCD",
#if defined(OS_WIN)
        base::FilePath(L"c:\\some\\path\\to\\chrome.exe")
#else
        base::FilePath("/some/path/to/chrome")
#endif
    )
  };

  // Duplicate samples in slots 0, 2, and 3.
  const Frame sample_frames[][1] = {
    { Frame(module_base_address + 0x10, 0), },
    { Frame(module_base_address + 0x20, 0), },
    { Frame(module_base_address + 0x10, 0), },
    { Frame(module_base_address + 0x10, 0) }
  };

  Profile profile;
  profile.modules.insert(profile.modules.end(), &modules[0],
      &modules[0] + arraysize(modules));

  for (size_t i = 0; i < arraysize(sample_frames); ++i) {
    profile.samples.push_back(Sample());
    Sample& sample = profile.samples.back();
    sample.insert(sample.end(), &sample_frames[i][0],
                  &sample_frames[i][0] + arraysize(sample_frames[i]));
  }

  profile.profile_duration = base::TimeDelta::FromMilliseconds(100);
  profile.sampling_period = base::TimeDelta::FromMilliseconds(10);

  CallStackProfileMetricsProvider provider;
  provider.OnRecordingEnabled();
  AppendProfiles(Params(CallStackProfileMetricsProvider::PROCESS_STARTUP, true),
                 std::vector<Profile>(1, profile));
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideGeneralMetrics(&uma_proto);

  ASSERT_EQ(1, uma_proto.sampled_profile().size());
  const SampledProfile& sampled_profile = uma_proto.sampled_profile().Get(0);
  ASSERT_TRUE(sampled_profile.has_call_stack_profile());
  const CallStackProfile& call_stack_profile =
      sampled_profile.call_stack_profile();

  ASSERT_EQ(3, call_stack_profile.sample().size());
  for (int i = 0; i < 3; ++i) {
    SCOPED_TRACE("sample " + base::IntToString(i));
    const CallStackProfile::Sample& proto_sample =
        call_stack_profile.sample().Get(i);
    ASSERT_EQ(static_cast<int>(arraysize(sample_frames[i])),
              proto_sample.entry().size());
    ASSERT_TRUE(proto_sample.has_count());
    EXPECT_EQ(i == 2 ? 2u : 1u, proto_sample.count());
    for (size_t j = 0; j < arraysize(sample_frames[i]); ++j) {
      SCOPED_TRACE("frame " + base::SizeTToString(j));
      const CallStackProfile::Entry& entry = proto_sample.entry().Get(j);
      ASSERT_TRUE(entry.has_address());
      const char* instruction_pointer = reinterpret_cast<const char*>(
          sample_frames[i][j].instruction_pointer);
      const char* module_base_address = reinterpret_cast<const char*>(
          modules[sample_frames[i][j].module_index].base_address);
      EXPECT_EQ(
          static_cast<uint64_t>(instruction_pointer - module_base_address),
          entry.address());
      ASSERT_TRUE(entry.has_module_id_index());
      EXPECT_EQ(sample_frames[i][j].module_index,
                static_cast<size_t>(entry.module_id_index()));
    }
  }
}

// Checks that unknown modules produce an empty Entry.
TEST_F(CallStackProfileMetricsProviderTest, UnknownModule) {
  const Frame frame(0x1000, Frame::kUnknownModuleIndex);

  Profile profile;

  profile.samples.push_back(Sample(1, frame));

  profile.profile_duration = base::TimeDelta::FromMilliseconds(100);
  profile.sampling_period = base::TimeDelta::FromMilliseconds(10);

  CallStackProfileMetricsProvider provider;
  provider.OnRecordingEnabled();
  AppendProfiles(
      Params(CallStackProfileMetricsProvider::PROCESS_STARTUP, false),
      std::vector<Profile>(1, profile));
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideGeneralMetrics(&uma_proto);

  ASSERT_EQ(1, uma_proto.sampled_profile().size());
  const SampledProfile& sampled_profile = uma_proto.sampled_profile().Get(0);
  ASSERT_TRUE(sampled_profile.has_call_stack_profile());
  const CallStackProfile& call_stack_profile =
      sampled_profile.call_stack_profile();

  ASSERT_EQ(1, call_stack_profile.sample().size());
  const CallStackProfile::Sample& proto_sample =
      call_stack_profile.sample().Get(0);
  ASSERT_EQ(1, proto_sample.entry().size());
  ASSERT_TRUE(proto_sample.has_count());
  EXPECT_EQ(1u, proto_sample.count());
  const CallStackProfile::Entry& entry = proto_sample.entry().Get(0);
  EXPECT_FALSE(entry.has_address());
  EXPECT_FALSE(entry.has_module_id_index());
}

// Checks that pending profiles are only passed back to ProvideGeneralMetrics
// once.
TEST_F(CallStackProfileMetricsProviderTest, ProfilesProvidedOnlyOnce) {
  CallStackProfileMetricsProvider provider;
  for (int i = 0; i < 2; ++i) {
    Profile profile;
    profile.samples.push_back(
        Sample(1, Frame(0x1000, Frame::kUnknownModuleIndex)));

    profile.profile_duration = base::TimeDelta::FromMilliseconds(100);
    // Use the sampling period to distinguish the two profiles.
    profile.sampling_period = base::TimeDelta::FromMilliseconds(i);

    provider.OnRecordingEnabled();
    AppendProfiles(
        Params(CallStackProfileMetricsProvider::PROCESS_STARTUP, false),
        std::vector<Profile>(1, profile));
    ChromeUserMetricsExtension uma_proto;
    provider.ProvideGeneralMetrics(&uma_proto);

    ASSERT_EQ(1, uma_proto.sampled_profile().size());
    const SampledProfile& sampled_profile = uma_proto.sampled_profile().Get(0);
    ASSERT_TRUE(sampled_profile.has_call_stack_profile());
    const CallStackProfile& call_stack_profile =
        sampled_profile.call_stack_profile();
    ASSERT_TRUE(call_stack_profile.has_sampling_period_ms());
    EXPECT_EQ(i, call_stack_profile.sampling_period_ms());
  }
}

// Checks that pending profiles are provided to ProvideGeneralMetrics
// when collected before CallStackProfileMetricsProvider is instantiated.
TEST_F(CallStackProfileMetricsProviderTest,
       ProfilesProvidedWhenCollectedBeforeInstantiation) {
  Profile profile;
  profile.samples.push_back(
      Sample(1, Frame(0x1000, Frame::kUnknownModuleIndex)));

  profile.profile_duration = base::TimeDelta::FromMilliseconds(100);
  profile.sampling_period = base::TimeDelta::FromMilliseconds(10);

  AppendProfiles(
      Params(CallStackProfileMetricsProvider::PROCESS_STARTUP, false),
      std::vector<Profile>(1, profile));

  CallStackProfileMetricsProvider provider;
  provider.OnRecordingEnabled();
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideGeneralMetrics(&uma_proto);

  EXPECT_EQ(1, uma_proto.sampled_profile_size());
}

// Checks that pending profiles are not provided to ProvideGeneralMetrics
// while recording is disabled.
TEST_F(CallStackProfileMetricsProviderTest, ProfilesNotProvidedWhileDisabled) {
  Profile profile;
  profile.samples.push_back(
      Sample(1, Frame(0x1000, Frame::kUnknownModuleIndex)));

  profile.profile_duration = base::TimeDelta::FromMilliseconds(100);
  profile.sampling_period = base::TimeDelta::FromMilliseconds(10);

  CallStackProfileMetricsProvider provider;
  provider.OnRecordingDisabled();
  AppendProfiles(
      Params(CallStackProfileMetricsProvider::PROCESS_STARTUP, false),
      std::vector<Profile>(1, profile));
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideGeneralMetrics(&uma_proto);

  EXPECT_EQ(0, uma_proto.sampled_profile_size());
}

// Checks that pending profiles are not provided to ProvideGeneralMetrics
// if recording is disabled while profiling.
TEST_F(CallStackProfileMetricsProviderTest,
       ProfilesNotProvidedAfterChangeToDisabled) {
  Profile profile;
  profile.samples.push_back(
      Sample(1, Frame(0x1000, Frame::kUnknownModuleIndex)));

  profile.profile_duration = base::TimeDelta::FromMilliseconds(100);
  profile.sampling_period = base::TimeDelta::FromMilliseconds(10);

  CallStackProfileMetricsProvider provider;
  provider.OnRecordingEnabled();
  base::StackSamplingProfiler::CompletedCallback callback =
      CallStackProfileMetricsProvider::GetProfilerCallback(
          Params(CallStackProfileMetricsProvider::PROCESS_STARTUP, false));

  provider.OnRecordingDisabled();
  callback.Run(std::vector<Profile>(1, profile));
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideGeneralMetrics(&uma_proto);

  EXPECT_EQ(0, uma_proto.sampled_profile_size());
}

// Checks that pending profiles are not provided to ProvideGeneralMetrics if
// recording is enabled, but then disabled and reenabled while profiling.
TEST_F(CallStackProfileMetricsProviderTest,
       ProfilesNotProvidedAfterChangeToDisabledThenEnabled) {
  Profile profile;
  profile.samples.push_back(
      Sample(1, Frame(0x1000, Frame::kUnknownModuleIndex)));

  profile.profile_duration = base::TimeDelta::FromMilliseconds(100);
  profile.sampling_period = base::TimeDelta::FromMilliseconds(10);

  CallStackProfileMetricsProvider provider;
  provider.OnRecordingEnabled();
  base::StackSamplingProfiler::CompletedCallback callback =
      CallStackProfileMetricsProvider::GetProfilerCallback(
          Params(CallStackProfileMetricsProvider::PROCESS_STARTUP, false));

  provider.OnRecordingDisabled();
  provider.OnRecordingEnabled();
  callback.Run(std::vector<Profile>(1, profile));
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideGeneralMetrics(&uma_proto);

  EXPECT_EQ(0, uma_proto.sampled_profile_size());
}

// Checks that pending profiles are not provided to ProvideGeneralMetrics
// if recording is disabled, but then enabled while profiling.
TEST_F(CallStackProfileMetricsProviderTest,
       ProfilesNotProvidedAfterChangeFromDisabled) {
  Profile profile;
  profile.samples.push_back(
      Sample(1, Frame(0x1000, Frame::kUnknownModuleIndex)));

  profile.profile_duration = base::TimeDelta::FromMilliseconds(100);
  profile.sampling_period = base::TimeDelta::FromMilliseconds(10);

  CallStackProfileMetricsProvider provider;
  provider.OnRecordingDisabled();
  base::StackSamplingProfiler::CompletedCallback callback =
      CallStackProfileMetricsProvider::GetProfilerCallback(
          Params(CallStackProfileMetricsProvider::PROCESS_STARTUP, false));

  provider.OnRecordingEnabled();
  callback.Run(std::vector<Profile>(1, profile));
  ChromeUserMetricsExtension uma_proto;
  provider.ProvideGeneralMetrics(&uma_proto);

  EXPECT_EQ(0, uma_proto.sampled_profile_size());
}

}  // namespace metrics
