// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_devices_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <string>

#include "base/command_line.h"
#include "base/location.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/public/browser/browser_thread.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_manager.h"
#include "media/base/media_switches.h"

#if defined(OS_MACOSX)
#include "base/bind_helpers.h"
#include "base/profiler/scoped_tracker.h"
#include "base/single_thread_task_runner.h"
#include "content/browser/browser_main_loop.h"
#include "media/device_monitors/device_monitor_mac.h"
#endif

namespace content {

namespace {

// Private helper method to generate a string for the log message that lists the
// human readable names of |devices|.
std::string GetLogMessageString(MediaDeviceType device_type,
                                const MediaDeviceInfoArray& device_infos) {
  std::string output_string =
      base::StringPrintf("Getting devices of type %d:\n", device_type);
  if (device_infos.empty())
    return output_string + "No devices found.";
  for (const auto& device_info : device_infos)
    output_string += "  " + device_info.label + "\n";
  return output_string;
}

MediaDeviceInfoArray EnumerateAudioDevicesOnDeviceThread(
    media::AudioManager* audio_manager,
    bool is_input) {
  DCHECK(audio_manager->GetTaskRunner()->BelongsToCurrentThread());

  MediaDeviceInfoArray snapshot;
  media::AudioDeviceNames device_names;
  if (is_input)
    audio_manager->GetAudioInputDeviceNames(&device_names);
  else
    audio_manager->GetAudioOutputDeviceNames(&device_names);

  for (const media::AudioDeviceName& name : device_names) {
    snapshot.emplace_back(
        name.unique_id, name.device_name,
        is_input ? audio_manager->GetGroupIDInput(name.unique_id)
                 : audio_manager->GetGroupIDOutput(name.unique_id));
  }

  return snapshot;
}

MediaDeviceInfoArray GetFakeAudioDevices(bool is_input) {
  MediaDeviceInfoArray result;
  if (is_input) {
    result.emplace_back(media::AudioDeviceDescription::kDefaultDeviceId,
                        "Fake Default Audio Input",
                        "fake_group_audio_input_default");
    result.emplace_back("fake_audio_input_1", "Fake Audio Input 1",
                        "fake_group_audio_input_1");
    result.emplace_back("fake_audio_input_2", "Fake Audio Input 2",
                        "fake_group_audio_input_2");
  } else {
    result.emplace_back(media::AudioDeviceDescription::kDefaultDeviceId,
                        "Fake Default Audio Output",
                        "fake_group_audio_output_default");
    result.emplace_back("fake_audio_output_1", "Fake Audio Output 1",
                        "fake_group_audio_output_1");
    result.emplace_back("fake_audio_output_2", "Fake Audio Output 2",
                        "fake_group_audio_output_2");
  }

  return result;
}

}  // namespace

struct MediaDevicesManager::EnumerationRequest {
  EnumerationRequest(const BoolDeviceTypes& requested_types,
                     const EnumerationCallback& callback)
      : callback(callback) {
    requested = requested_types;
    has_seen_result.fill(false);
  }

  BoolDeviceTypes requested;
  BoolDeviceTypes has_seen_result;
  EnumerationCallback callback;
};

// This class helps manage the consistency of cached enumeration results.
// It uses a sequence number for each invalidation and enumeration.
// A cache is considered valid if the sequence number for the last enumeration
// is greater than the sequence number for the last invalidation.
// The advantage of using invalidations over directly issuing enumerations upon
// each system notification is that some platforms issue multiple notifications
// on each device change. The cost of performing multiple redundant
// invalidations is significantly lower than the cost of issuing multiple
// redundant enumerations.
class MediaDevicesManager::CacheInfo {
 public:
  CacheInfo()
      : current_event_sequence_(0),
        seq_last_update_(0),
        seq_last_invalidation_(0),
        is_update_ongoing_(false) {}

  void InvalidateCache() {
    DCHECK(thread_checker_.CalledOnValidThread());
    seq_last_invalidation_ = NewEventSequence();
  }

  bool IsLastUpdateValid() const {
    DCHECK(thread_checker_.CalledOnValidThread());
    return seq_last_update_ > seq_last_invalidation_ && !is_update_ongoing_;
  }

  void UpdateStarted() {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(!is_update_ongoing_);
    seq_last_update_ = NewEventSequence();
    is_update_ongoing_ = true;
  }

  void UpdateCompleted() {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(is_update_ongoing_);
    is_update_ongoing_ = false;
  }

  bool is_update_ongoing() const {
    DCHECK(thread_checker_.CalledOnValidThread());
    return is_update_ongoing_;
  }

 private:
  int64_t NewEventSequence() {
    DCHECK(thread_checker_.CalledOnValidThread());
    return ++current_event_sequence_;
  }

  int64_t current_event_sequence_;
  int64_t seq_last_update_;
  int64_t seq_last_invalidation_;
  bool is_update_ongoing_;
  base::ThreadChecker thread_checker_;
};

MediaDevicesManager::MediaDevicesManager(
    media::AudioManager* audio_manager,
    const scoped_refptr<VideoCaptureManager>& video_capture_manager,
    MediaStreamManager* media_stream_manager)
    : use_fake_devices_(base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeDeviceForMediaStream)),
      audio_manager_(audio_manager),
      video_capture_manager_(video_capture_manager),
      media_stream_manager_(media_stream_manager),
      cache_infos_(NUM_MEDIA_DEVICE_TYPES),
      monitoring_started_(false),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(audio_manager_);
  DCHECK(video_capture_manager_.get());
  cache_policies_.fill(CachePolicy::NO_CACHE);
  has_seen_result_.fill(false);
}

MediaDevicesManager::~MediaDevicesManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void MediaDevicesManager::EnumerateDevices(
    const BoolDeviceTypes& requested_types,
    const EnumerationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  StartMonitoring();

  requests_.emplace_back(requested_types, callback);
  bool all_results_cached = true;
  for (size_t i = 0; i < NUM_MEDIA_DEVICE_TYPES; ++i) {
    if (requested_types[i] && cache_policies_[i] == CachePolicy::NO_CACHE) {
      all_results_cached = false;
      DoEnumerateDevices(static_cast<MediaDeviceType>(i));
    }
  }

  if (all_results_cached)
    ProcessRequests();
}

void MediaDevicesManager::SubscribeDeviceChangeNotifications(
    MediaDeviceType type,
    MediaDeviceChangeSubscriber* subscriber) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto it = std::find(device_change_subscribers_[type].begin(),
                      device_change_subscribers_[type].end(), subscriber);
  if (it == device_change_subscribers_[type].end())
    device_change_subscribers_[type].push_back(subscriber);
}

void MediaDevicesManager::UnsubscribeDeviceChangeNotifications(
    MediaDeviceType type,
    MediaDeviceChangeSubscriber* subscriber) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  auto it = std::find(device_change_subscribers_[type].begin(),
                      device_change_subscribers_[type].end(), subscriber);
  if (it != device_change_subscribers_[type].end())
    device_change_subscribers_[type].erase(it);
}

void MediaDevicesManager::SetCachePolicy(MediaDeviceType type,
                                         CachePolicy policy) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(IsValidMediaDeviceType(type));
  if (cache_policies_[type] == policy)
    return;

  cache_policies_[type] = policy;
  // If the new policy is SYSTEM_MONITOR, issue an enumeration to populate the
  // cache.
  if (policy == CachePolicy::SYSTEM_MONITOR) {
    cache_infos_[type].InvalidateCache();
    DoEnumerateDevices(type);
  }
}

void MediaDevicesManager::StartMonitoring() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (monitoring_started_)
    return;

  if (!base::SystemMonitor::Get())
    return;

  monitoring_started_ = true;
  base::SystemMonitor::Get()->AddDevicesChangedObserver(this);

  for (size_t i = 0; i < NUM_MEDIA_DEVICE_TYPES; ++i) {
    DCHECK(cache_policies_[i] != CachePolicy::SYSTEM_MONITOR);
    SetCachePolicy(static_cast<MediaDeviceType>(i),
                   CachePolicy::SYSTEM_MONITOR);
  }

#if defined(OS_MACOSX)
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&MediaDevicesManager::StartMonitoringOnUIThread,
                 base::Unretained(this)));

  // TODO(guidou): Remove this statement once the Mac device monitor is fixed to
  //  correctly report device-change events for output-only audio devices.
  // See http://crbug.com/648173.
  SetCachePolicy(MEDIA_DEVICE_TYPE_AUDIO_OUTPUT, CachePolicy::NO_CACHE);
#endif
}

#if defined(OS_MACOSX)
void MediaDevicesManager::StartMonitoringOnUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(erikchen): Remove ScopedTracker below once crbug.com/458404 is fixed.
  tracked_objects::ScopedTracker tracking_profile1(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "458404 MediaDevicesManager::GetBrowserMainLoop"));
  BrowserMainLoop* browser_main_loop = content::BrowserMainLoop::GetInstance();
  if (!browser_main_loop)
    return;

  // TODO(erikchen): Remove ScopedTracker below once crbug.com/458404 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile2(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "458404 MediaDevicesManager::GetTaskRunner"));
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      audio_manager_->GetTaskRunner();
  // TODO(erikchen): Remove ScopedTracker below once crbug.com/458404 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile3(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "458404 MediaDevicesManager::DeviceMonitorMac::StartMonitoring"));
  browser_main_loop->device_monitor_mac()->StartMonitoring(task_runner);
}
#endif

void MediaDevicesManager::StopMonitoring() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!monitoring_started_)
    return;
  base::SystemMonitor::Get()->RemoveDevicesChangedObserver(this);
  monitoring_started_ = false;
  for (size_t i = 0; i < NUM_MEDIA_DEVICE_TYPES; ++i)
    SetCachePolicy(static_cast<MediaDeviceType>(i), CachePolicy::NO_CACHE);
}

bool MediaDevicesManager::IsMonitoringStarted() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return monitoring_started_;
}

void MediaDevicesManager::OnDevicesChanged(
    base::SystemMonitor::DeviceType device_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  switch (device_type) {
    case base::SystemMonitor::DEVTYPE_AUDIO:
      HandleDevicesChanged(MEDIA_DEVICE_TYPE_AUDIO_INPUT);
      HandleDevicesChanged(MEDIA_DEVICE_TYPE_AUDIO_OUTPUT);
      break;
    case base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE:
      HandleDevicesChanged(MEDIA_DEVICE_TYPE_VIDEO_INPUT);
      break;
    default:
      break;  // Uninteresting device change.
  }
}

MediaDeviceInfoArray MediaDevicesManager::GetCachedDeviceInfo(
    MediaDeviceType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return current_snapshot_[type];
}

void MediaDevicesManager::DoEnumerateDevices(MediaDeviceType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(IsValidMediaDeviceType(type));
  CacheInfo& cache_info = cache_infos_[type];
  if (cache_info.is_update_ongoing())
    return;

  cache_info.UpdateStarted();
  switch (type) {
    case MEDIA_DEVICE_TYPE_AUDIO_INPUT:
      EnumerateAudioDevices(true /* is_input */);
      break;
    case MEDIA_DEVICE_TYPE_VIDEO_INPUT:
      video_capture_manager_->EnumerateDevices(
          base::Bind(&MediaDevicesManager::VideoInputDevicesEnumerated,
                     weak_factory_.GetWeakPtr()));
      break;
    case MEDIA_DEVICE_TYPE_AUDIO_OUTPUT:
      EnumerateAudioDevices(false /* is_input */);
      break;
    default:
      NOTREACHED();
  }
}

void MediaDevicesManager::EnumerateAudioDevices(bool is_input) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  MediaDeviceType type =
      is_input ? MEDIA_DEVICE_TYPE_AUDIO_INPUT : MEDIA_DEVICE_TYPE_AUDIO_OUTPUT;
  if (use_fake_devices_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&MediaDevicesManager::DevicesEnumerated,
                              weak_factory_.GetWeakPtr(), type,
                              GetFakeAudioDevices(is_input)));
    return;
  }
  base::PostTaskAndReplyWithResult(
      audio_manager_->GetTaskRunner(), FROM_HERE,
      base::Bind(&EnumerateAudioDevicesOnDeviceThread, audio_manager_,
                 is_input),
      base::Bind(&MediaDevicesManager::DevicesEnumerated,
                 weak_factory_.GetWeakPtr(), type));
}

void MediaDevicesManager::VideoInputDevicesEnumerated(
    const media::VideoCaptureDeviceDescriptors& descriptors) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  MediaDeviceInfoArray snapshot;
  for (const auto& descriptor : descriptors) {
    snapshot.emplace_back(descriptor.device_id, descriptor.GetNameAndModel(),
                          std::string());
  }
  DevicesEnumerated(MEDIA_DEVICE_TYPE_VIDEO_INPUT, snapshot);
}

void MediaDevicesManager::DevicesEnumerated(
    MediaDeviceType type,
    const MediaDeviceInfoArray& snapshot) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(IsValidMediaDeviceType(type));
  UpdateSnapshot(type, snapshot);
  cache_infos_[type].UpdateCompleted();
  has_seen_result_[type] = true;

  std::string log_message =
      "New device enumeration result:\n" + GetLogMessageString(type, snapshot);
  MediaStreamManager::SendMessageToNativeLog(log_message);

  if (cache_policies_[type] == CachePolicy::NO_CACHE) {
    for (auto& request : requests_)
      request.has_seen_result[type] = true;
  }

  // Note that IsLastUpdateValid is always true when policy is NO_CACHE.
  if (cache_infos_[type].IsLastUpdateValid()) {
    ProcessRequests();
  } else {
    DoEnumerateDevices(type);
  }
}

void MediaDevicesManager::UpdateSnapshot(
    MediaDeviceType type,
    const MediaDeviceInfoArray& new_snapshot) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(IsValidMediaDeviceType(type));

  // Only cache the device list when the device list has been changed.
  bool need_update_device_change_subscribers = false;
  MediaDeviceInfoArray& old_snapshot = current_snapshot_[type];

  if (old_snapshot.size() != new_snapshot.size() ||
      !std::equal(new_snapshot.begin(), new_snapshot.end(),
                  old_snapshot.begin())) {
    if (type == MEDIA_DEVICE_TYPE_AUDIO_INPUT ||
        type == MEDIA_DEVICE_TYPE_VIDEO_INPUT) {
      NotifyMediaStreamManager(type, new_snapshot);
    }

    // Do not notify device-change subscribers after the first enumeration
    // result, since it is not due to an actual device change.
    need_update_device_change_subscribers =
        has_seen_result_[type] &&
        (old_snapshot.size() != 0 || new_snapshot.size() != 0);
    current_snapshot_[type] = new_snapshot;
  }

  if (need_update_device_change_subscribers)
    NotifyDeviceChangeSubscribers(type, new_snapshot);
}

void MediaDevicesManager::ProcessRequests() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  requests_.erase(std::remove_if(requests_.begin(), requests_.end(),
                                 [this](const EnumerationRequest& request) {
                                   if (IsEnumerationRequestReady(request)) {
                                     request.callback.Run(current_snapshot_);
                                     return true;
                                   }
                                   return false;
                                 }),
                  requests_.end());
}

bool MediaDevicesManager::IsEnumerationRequestReady(
    const EnumerationRequest& request_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  bool is_ready = true;
  for (size_t i = 0; i < NUM_MEDIA_DEVICE_TYPES; ++i) {
    if (!request_info.requested[i])
      continue;
    switch (cache_policies_[i]) {
      case CachePolicy::SYSTEM_MONITOR:
        if (!cache_infos_[i].IsLastUpdateValid())
          is_ready = false;
        break;
      case CachePolicy::NO_CACHE:
        if (!request_info.has_seen_result[i])
          is_ready = false;
        break;
      default:
        NOTREACHED();
    }
  }
  return is_ready;
}

void MediaDevicesManager::HandleDevicesChanged(MediaDeviceType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(IsValidMediaDeviceType(type));
  cache_infos_[type].InvalidateCache();
  DoEnumerateDevices(type);
}

void MediaDevicesManager::NotifyMediaStreamManager(
    MediaDeviceType type,
    const MediaDeviceInfoArray& new_snapshot) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(type == MEDIA_DEVICE_TYPE_AUDIO_INPUT ||
         type == MEDIA_DEVICE_TYPE_VIDEO_INPUT);

  if (!media_stream_manager_)
    return;

  for (const auto& old_device_info : current_snapshot_[type]) {
    auto it = std::find_if(new_snapshot.begin(), new_snapshot.end(),
                           [&old_device_info](const MediaDeviceInfo& info) {
                             return info.device_id == old_device_info.device_id;
                           });

    // If a device was removed, notify the MediaStreamManager to stop all
    // streams using that device.
    if (it == new_snapshot.end())
      media_stream_manager_->StopRemovedDevice(type, old_device_info);
  }

  media_stream_manager_->NotifyDevicesChanged(type, new_snapshot);
}

void MediaDevicesManager::NotifyDeviceChangeSubscribers(
    MediaDeviceType type,
    const MediaDeviceInfoArray& snapshot) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(IsValidMediaDeviceType(type));

  for (const auto& subscriber : device_change_subscribers_[type]) {
    subscriber->OnDevicesChanged(type, snapshot);
  }
}

}  // namespace content
