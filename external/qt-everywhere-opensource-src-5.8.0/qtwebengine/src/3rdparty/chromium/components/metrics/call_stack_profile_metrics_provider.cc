// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/call_stack_profile_metrics_provider.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cstring>
#include <map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/metrics_hashes.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/metrics/proto/chrome_user_metrics_extension.pb.h"

using base::StackSamplingProfiler;

namespace metrics {

namespace {

// ProfilesState --------------------------------------------------------------

// A set of profiles and the CallStackProfileMetricsProvider state associated
// with them.
struct ProfilesState {
  ProfilesState(const CallStackProfileMetricsProvider::Params& params,
                const base::StackSamplingProfiler::CallStackProfiles& profiles,
                base::TimeTicks start_timestamp);

  // The metrics-related parameters provided to
  // CallStackProfileMetricsProvider::GetProfilerCallback().
  CallStackProfileMetricsProvider::Params params;

  // The call stack profiles collected by the profiler.
  base::StackSamplingProfiler::CallStackProfiles profiles;

  // The time at which the CallStackProfileMetricsProvider became aware of the
  // request for profiling. In particular, this is when callback was requested
  // via CallStackProfileMetricsProvider::GetProfilerCallback(). Used to
  // determine if collection was disabled during the collection of the profile.
  base::TimeTicks start_timestamp;
};

ProfilesState::ProfilesState(
    const CallStackProfileMetricsProvider::Params& params,
    const base::StackSamplingProfiler::CallStackProfiles& profiles,
    base::TimeTicks start_timestamp)
    : params(params),
      profiles(profiles),
      start_timestamp(start_timestamp) {
}

// PendingProfiles ------------------------------------------------------------

// Singleton class responsible for retaining profiles received via the callback
// created by CallStackProfileMetricsProvider::GetProfilerCallback(). These are
// then sent to UMA on the invocation of
// CallStackProfileMetricsProvider::ProvideGeneralMetrics(). We need to store
// the profiles outside of a CallStackProfileMetricsProvider instance since
// callers may start profiling before the CallStackProfileMetricsProvider is
// created.
//
// Member functions on this class may be called on any thread.
class PendingProfiles {
 public:
  static PendingProfiles* GetInstance();

  void Clear();
  void Swap(std::vector<ProfilesState>* profiles);

  // Enables the collection of profiles by CollectProfilesIfCollectionEnabled if
  // |enabled| is true. Otherwise, clears current profiles and ignores profiles
  // provided to future invocations of CollectProfilesIfCollectionEnabled.
  void SetCollectionEnabled(bool enabled);

  // True if profiles are being collected.
  bool IsCollectionEnabled() const;

  // Adds |profile| to the list of profiles if collection is enabled.
  void CollectProfilesIfCollectionEnabled(const ProfilesState& profiles);

  // Allows testing against the initial state multiple times.
  void ResetToDefaultStateForTesting();

 private:
  friend struct base::DefaultSingletonTraits<PendingProfiles>;

  PendingProfiles();
  ~PendingProfiles();

  mutable base::Lock lock_;

  // If true, profiles provided to CollectProfilesIfCollectionEnabled should be
  // collected. Otherwise they will be ignored.
  bool collection_enabled_;

  // The last time collection was disabled. Used to determine if collection was
  // disabled at any point since a profile was started.
  base::TimeTicks last_collection_disable_time_;

  // The set of completed profiles that should be reported.
  std::vector<ProfilesState> profiles_;

  DISALLOW_COPY_AND_ASSIGN(PendingProfiles);
};

// static
PendingProfiles* PendingProfiles::GetInstance() {
  // Leaky for performance rather than correctness reasons.
  return base::Singleton<PendingProfiles,
                         base::LeakySingletonTraits<PendingProfiles>>::get();
}

void PendingProfiles::Clear() {
  base::AutoLock scoped_lock(lock_);
  profiles_.clear();
}

void PendingProfiles::Swap(std::vector<ProfilesState>* profiles) {
  base::AutoLock scoped_lock(lock_);
  profiles_.swap(*profiles);
}

void PendingProfiles::SetCollectionEnabled(bool enabled) {
  base::AutoLock scoped_lock(lock_);

  collection_enabled_ = enabled;

  if (!collection_enabled_) {
    profiles_.clear();
    last_collection_disable_time_ = base::TimeTicks::Now();
  }
}

bool PendingProfiles::IsCollectionEnabled() const {
  base::AutoLock scoped_lock(lock_);
  return collection_enabled_;
}

void PendingProfiles::CollectProfilesIfCollectionEnabled(
    const ProfilesState& profiles) {
  base::AutoLock scoped_lock(lock_);

  // Only collect if collection is not disabled and hasn't been disabled
  // since the start of collection for this profile.
  if (!collection_enabled_ ||
      (!last_collection_disable_time_.is_null() &&
       last_collection_disable_time_ >= profiles.start_timestamp)) {
    return;
  }

  profiles_.push_back(profiles);
}

void PendingProfiles::ResetToDefaultStateForTesting() {
  base::AutoLock scoped_lock(lock_);

  collection_enabled_ = true;
  last_collection_disable_time_ = base::TimeTicks();
  profiles_.clear();
}

// |collection_enabled_| is initialized to true to collect any profiles that are
// generated prior to creation of the CallStackProfileMetricsProvider. The
// ultimate disposition of these pre-creation collected profiles will be
// determined by the initial recording state provided to
// CallStackProfileMetricsProvider.
PendingProfiles::PendingProfiles() : collection_enabled_(true) {}

PendingProfiles::~PendingProfiles() {}

// Functions to process completed profiles ------------------------------------

// Invoked on the profiler's thread. Provides the profiles to PendingProfiles to
// append, if the collecting state allows.
void ReceiveCompletedProfiles(
    const CallStackProfileMetricsProvider::Params& params,
    base::TimeTicks start_timestamp,
    const StackSamplingProfiler::CallStackProfiles& profiles) {
  PendingProfiles::GetInstance()->CollectProfilesIfCollectionEnabled(
      ProfilesState(params, profiles, start_timestamp));
}

// Invoked on an arbitrary thread. Ignores the provided profiles.
void IgnoreCompletedProfiles(
    const StackSamplingProfiler::CallStackProfiles& profiles) {
}

// Functions to encode protobufs ----------------------------------------------

// The protobuf expects the MD5 checksum prefix of the module name.
uint64_t HashModuleFilename(const base::FilePath& filename) {
  const base::FilePath::StringType basename = filename.BaseName().value();
  // Copy the bytes in basename into a string buffer.
  size_t basename_length_in_bytes =
      basename.size() * sizeof(base::FilePath::CharType);
  std::string name_bytes(basename_length_in_bytes, '\0');
  memcpy(&name_bytes[0], &basename[0], basename_length_in_bytes);
  return base::HashMetricName(name_bytes);
}

// Transcode |sample| into |proto_sample|, using base addresses in |modules| to
// compute module instruction pointer offsets.
void CopySampleToProto(
    const StackSamplingProfiler::Sample& sample,
    const std::vector<StackSamplingProfiler::Module>& modules,
    CallStackProfile::Sample* proto_sample) {
  for (const StackSamplingProfiler::Frame& frame : sample) {
    CallStackProfile::Entry* entry = proto_sample->add_entry();
    // A frame may not have a valid module. If so, we can't compute the
    // instruction pointer offset, and we don't want to send bare pointers, so
    // leave call_stack_entry empty.
    if (frame.module_index == StackSamplingProfiler::Frame::kUnknownModuleIndex)
      continue;
    int64_t module_offset =
        reinterpret_cast<const char*>(frame.instruction_pointer) -
        reinterpret_cast<const char*>(modules[frame.module_index].base_address);
    DCHECK_GE(module_offset, 0);
    entry->set_address(static_cast<uint64_t>(module_offset));
    entry->set_module_id_index(frame.module_index);
  }
}

// Transcode |profile| into |proto_profile|.
void CopyProfileToProto(
    const StackSamplingProfiler::CallStackProfile& profile,
    bool preserve_sample_ordering,
    CallStackProfile* proto_profile) {
  if (profile.samples.empty())
    return;

  if (preserve_sample_ordering) {
    // Collapse only consecutive repeated samples together.
    CallStackProfile::Sample* current_sample_proto = nullptr;
    for (auto it = profile.samples.begin(); it != profile.samples.end(); ++it) {
      if (!current_sample_proto || *it != *(it - 1)) {
        current_sample_proto = proto_profile->add_sample();
        CopySampleToProto(*it, profile.modules, current_sample_proto);
        current_sample_proto->set_count(1);
      } else {
        current_sample_proto->set_count(current_sample_proto->count() + 1);
      }
    }
  } else {
    // Collapse all repeated samples together.
    std::map<StackSamplingProfiler::Sample, int> sample_index;
    for (auto it = profile.samples.begin(); it != profile.samples.end(); ++it) {
      auto location = sample_index.find(*it);
      if (location == sample_index.end()) {
        CallStackProfile::Sample* sample_proto = proto_profile->add_sample();
        CopySampleToProto(*it, profile.modules, sample_proto);
        sample_proto->set_count(1);
        sample_index.insert(
            std::make_pair(
                *it, static_cast<int>(proto_profile->sample().size()) - 1));
      } else {
        CallStackProfile::Sample* sample_proto =
            proto_profile->mutable_sample()->Mutable(location->second);
        sample_proto->set_count(sample_proto->count() + 1);
      }
    }
  }

  for (const StackSamplingProfiler::Module& module : profile.modules) {
    CallStackProfile::ModuleIdentifier* module_id =
        proto_profile->add_module_id();
    module_id->set_build_id(module.id);
    module_id->set_name_md5_prefix(HashModuleFilename(module.filename));
  }

  proto_profile->set_profile_duration_ms(
      profile.profile_duration.InMilliseconds());
  proto_profile->set_sampling_period_ms(
      profile.sampling_period.InMilliseconds());
}

// Translates CallStackProfileMetricsProvider's trigger to the corresponding
// SampledProfile TriggerEvent.
SampledProfile::TriggerEvent ToSampledProfileTriggerEvent(
    CallStackProfileMetricsProvider::Trigger trigger) {
  switch (trigger) {
    case CallStackProfileMetricsProvider::UNKNOWN:
      return SampledProfile::UNKNOWN_TRIGGER_EVENT;
      break;
    case CallStackProfileMetricsProvider::PROCESS_STARTUP:
      return SampledProfile::PROCESS_STARTUP;
      break;
    case CallStackProfileMetricsProvider::JANKY_TASK:
      return SampledProfile::JANKY_TASK;
      break;
    case CallStackProfileMetricsProvider::THREAD_HUNG:
      return SampledProfile::THREAD_HUNG;
      break;
  }
  NOTREACHED();
  return SampledProfile::UNKNOWN_TRIGGER_EVENT;
}

}  // namespace

// CallStackProfileMetricsProvider::Params ------------------------------------

CallStackProfileMetricsProvider::Params::Params(
    CallStackProfileMetricsProvider::Trigger trigger)
    : Params(trigger, false) {
}

CallStackProfileMetricsProvider::Params::Params(
    CallStackProfileMetricsProvider::Trigger trigger,
    bool preserve_sample_ordering)
    : trigger(trigger),
      preserve_sample_ordering(preserve_sample_ordering) {
}

// CallStackProfileMetricsProvider --------------------------------------------

const char CallStackProfileMetricsProvider::kFieldTrialName[] =
    "StackProfiling";
const char CallStackProfileMetricsProvider::kReportProfilesGroupName[] =
    "Report profiles";

CallStackProfileMetricsProvider::CallStackProfileMetricsProvider() {
}

CallStackProfileMetricsProvider::~CallStackProfileMetricsProvider() {
}

// This function can be invoked on an abitrary thread.
base::StackSamplingProfiler::CompletedCallback
CallStackProfileMetricsProvider::GetProfilerCallback(const Params& params) {
  // Ignore the profiles if the collection is disabled. If the collection state
  // changes while collecting, this will be detected by the callback and
  // profiles will be ignored at that point.
  if (!PendingProfiles::GetInstance()->IsCollectionEnabled())
    return base::Bind(&IgnoreCompletedProfiles);

  return base::Bind(&ReceiveCompletedProfiles, params, base::TimeTicks::Now());
}

void CallStackProfileMetricsProvider::OnRecordingEnabled() {
  PendingProfiles::GetInstance()->SetCollectionEnabled(true);
}

void CallStackProfileMetricsProvider::OnRecordingDisabled() {
  PendingProfiles::GetInstance()->SetCollectionEnabled(false);
}

void CallStackProfileMetricsProvider::ProvideGeneralMetrics(
    ChromeUserMetricsExtension* uma_proto) {
  std::vector<ProfilesState> pending_profiles;
  PendingProfiles::GetInstance()->Swap(&pending_profiles);

  DCHECK(IsReportingEnabledByFieldTrial() || pending_profiles.empty());

  for (const ProfilesState& profiles_state : pending_profiles) {
    for (const StackSamplingProfiler::CallStackProfile& profile :
             profiles_state.profiles) {
      SampledProfile* sampled_profile = uma_proto->add_sampled_profile();
      sampled_profile->set_trigger_event(ToSampledProfileTriggerEvent(
          profiles_state.params.trigger));
      CopyProfileToProto(profile,
                         profiles_state.params.preserve_sample_ordering,
                         sampled_profile->mutable_call_stack_profile());
    }
  }
}

// static
void CallStackProfileMetricsProvider::ResetStaticStateForTesting() {
  PendingProfiles::GetInstance()->ResetToDefaultStateForTesting();
}

// static
bool CallStackProfileMetricsProvider::IsReportingEnabledByFieldTrial() {
  const std::string group_name = base::FieldTrialList::FindFullName(
      CallStackProfileMetricsProvider::kFieldTrialName);
  return group_name ==
      CallStackProfileMetricsProvider::kReportProfilesGroupName;
}

}  // namespace metrics
