// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/renderer/media/audio_device_factory.h"
#include "content/renderer/media/audio_renderer_sink_cache_impl.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_renderer_sink.h"
#include "url/origin.h"

namespace content {

constexpr int kDeleteTimeoutMs = 5000;

namespace {

enum GetOutputDeviceInfoCacheUtilization {
  // No cached sink found.
  SINK_CACHE_MISS_NO_SINK = 0,

  // If session id is used to specify a device, we always have to create and
  // cache a new sink.
  SINK_CACHE_MISS_CANNOT_LOOKUP_BY_SESSION_ID = 1,

  // Output parmeters for an already-cached sink are requested.
  SINK_CACHE_HIT = 2,

  // For UMA.
  SINK_CACHE_LAST_ENTRY
};

bool SinkIsHealthy(media::AudioRendererSink* sink) {
  return sink->GetOutputDeviceInfo().device_status() ==
         media::OUTPUT_DEVICE_STATUS_OK;
}

}  // namespace

// Cached sink data.
struct AudioRendererSinkCacheImpl::CacheEntry {
  int source_render_frame_id;
  std::string device_id;
  url::Origin security_origin;
  scoped_refptr<media::AudioRendererSink> sink;  // Sink instance
  bool used;                                     // True if in use by a client.
};

// static
std::unique_ptr<AudioRendererSinkCache> AudioRendererSinkCache::Create() {
  return base::WrapUnique(new AudioRendererSinkCacheImpl(
      base::ThreadTaskRunnerHandle::Get(),
      base::Bind(&AudioDeviceFactory::NewAudioRendererMixerSink),
      base::TimeDelta::FromMilliseconds(kDeleteTimeoutMs)));
}

AudioRendererSinkCacheImpl::AudioRendererSinkCacheImpl(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const CreateSinkCallback& create_sink_cb,
    base::TimeDelta delete_timeout)
    : task_runner_(std::move(task_runner)),
      create_sink_cb_(create_sink_cb),
      delete_timeout_(delete_timeout),
      weak_ptr_factory_(this) {
  weak_this_ = weak_ptr_factory_.GetWeakPtr();
}

AudioRendererSinkCacheImpl::~AudioRendererSinkCacheImpl() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  // We just release all the cached sinks here. Stop them first.
  // We can stop all the sinks, no matter they are used or not, since everything
  // is being destroyed anyways.
  for (auto& entry : cache_)
    entry.sink->Stop();
}

media::OutputDeviceInfo AudioRendererSinkCacheImpl::GetSinkInfo(
    int source_render_frame_id,
    int session_id,
    const std::string& device_id,
    const url::Origin& security_origin) {
  if (media::AudioDeviceDescription::UseSessionIdToSelectDevice(session_id,
                                                                device_id)) {
    // We are provided with session id instead of device id. Session id is
    // unique, so we can't find any matching sink. Creating a new one.
    scoped_refptr<media::AudioRendererSink> sink = create_sink_cb_.Run(
        source_render_frame_id, session_id, device_id, security_origin);

    CacheUnusedSinkIfHealthy(source_render_frame_id,
                             sink->GetOutputDeviceInfo().device_id(),
                             security_origin, sink);
    UMA_HISTOGRAM_ENUMERATION(
        "Media.Audio.Render.SinkCache.GetOutputDeviceInfoCacheUtilization",
        SINK_CACHE_MISS_CANNOT_LOOKUP_BY_SESSION_ID, SINK_CACHE_LAST_ENTRY);
    return sink->GetOutputDeviceInfo();
  }
  // Ignore session id.
  {
    base::AutoLock auto_lock(cache_lock_);
    auto cache_iter =
        FindCacheEntry_Locked(source_render_frame_id, device_id,
                              security_origin, false /* unused_only */);
    if (cache_iter != cache_.end()) {
      // A matching cached sink is found.
      UMA_HISTOGRAM_ENUMERATION(
          "Media.Audio.Render.SinkCache.GetOutputDeviceInfoCacheUtilization",
          SINK_CACHE_HIT, SINK_CACHE_LAST_ENTRY);
      return cache_iter->sink->GetOutputDeviceInfo();
    }
  }

  // No matching sink found, create a new one.
  scoped_refptr<media::AudioRendererSink> sink = create_sink_cb_.Run(
      source_render_frame_id, 0 /* session_id */, device_id, security_origin);
  CacheUnusedSinkIfHealthy(source_render_frame_id, device_id, security_origin,
                           sink);
  UMA_HISTOGRAM_ENUMERATION(
      "Media.Audio.Render.SinkCache.GetOutputDeviceInfoCacheUtilization",
      SINK_CACHE_MISS_NO_SINK, SINK_CACHE_LAST_ENTRY);

  //|sink| is ref-counted, so it's ok if it is removed from cache before we get
  // here.
  return sink->GetOutputDeviceInfo();
}

scoped_refptr<media::AudioRendererSink> AudioRendererSinkCacheImpl::GetSink(
    int source_render_frame_id,
    const std::string& device_id,
    const url::Origin& security_origin) {
  UMA_HISTOGRAM_BOOLEAN("Media.Audio.Render.SinkCache.UsedForSinkCreation",
                        true);

  base::AutoLock auto_lock(cache_lock_);

  auto cache_iter =
      FindCacheEntry_Locked(source_render_frame_id, device_id, security_origin,
                            true /* unused sink only */);

  if (cache_iter != cache_.end()) {
    // Found unused sink; mark it as used and return.
    cache_iter->used = true;
    UMA_HISTOGRAM_BOOLEAN(
        "Media.Audio.Render.SinkCache.InfoSinkReusedForOutput", true);
    return cache_iter->sink;
  }

  // No unused sink is found, create one, mark it used, cache it and return.
  CacheEntry cache_entry = {
      source_render_frame_id, device_id, security_origin,
      create_sink_cb_.Run(source_render_frame_id, 0 /* session_id */, device_id,
                          security_origin),
      true /* used */};

  if (SinkIsHealthy(cache_entry.sink.get()))
    cache_.push_back(cache_entry);

  return cache_entry.sink;
}

void AudioRendererSinkCacheImpl::ReleaseSink(
    const media::AudioRendererSink* sink_ptr) {
  // We don't know the sink state, so won't reuse it. Delete it immediately.
  DeleteSink(sink_ptr, true);
}

void AudioRendererSinkCacheImpl::DeleteLaterIfUnused(
    const media::AudioRendererSink* sink_ptr) {
  task_runner_->PostDelayedTask(
      FROM_HERE, base::Bind(&AudioRendererSinkCacheImpl::DeleteSink, weak_this_,
                            sink_ptr, false /*do not delete if used*/),
      delete_timeout_);
}

void AudioRendererSinkCacheImpl::DeleteSink(
    const media::AudioRendererSink* sink_ptr,
    bool force_delete_used) {
  DCHECK(sink_ptr);

  scoped_refptr<media::AudioRendererSink> sink_to_stop;

  {
    base::AutoLock auto_lock(cache_lock_);

    // Looking up the sink by its pointer.
    auto cache_iter = std::find_if(cache_.begin(), cache_.end(),
                                   [sink_ptr](const CacheEntry& val) {
                                     return val.sink.get() == sink_ptr;
                                   });

    if (cache_iter == cache_.end())
      return;

    // When |force_delete_used| is set, it's expected that we are deleting a
    // used sink.
    DCHECK((!force_delete_used) || (force_delete_used && cache_iter->used))
        << "Attempt to delete a non-aquired sink.";

    if (!force_delete_used && cache_iter->used)
      return;

    // To stop the sink before deletion if it's not used, we need to hold
    // a ref to it.
    if (!cache_iter->used) {
      sink_to_stop = cache_iter->sink;
      UMA_HISTOGRAM_BOOLEAN(
          "Media.Audio.Render.SinkCache.InfoSinkReusedForOutput", false);
    }

    cache_.erase(cache_iter);
  }  // Lock scope;

  // Stop the sink out of the lock scope.
  if (sink_to_stop.get()) {
    DCHECK_EQ(sink_ptr, sink_to_stop.get());
    sink_to_stop->Stop();
  }
}

AudioRendererSinkCacheImpl::CacheContainer::iterator
AudioRendererSinkCacheImpl::FindCacheEntry_Locked(
    int source_render_frame_id,
    const std::string& device_id,
    const url::Origin& security_origin,
    bool unused_only) {
  return std::find_if(
      cache_.begin(), cache_.end(),
      [source_render_frame_id, &device_id, &security_origin,
       unused_only](const CacheEntry& val) {
        if (val.used && unused_only)
          return false;
        if (val.source_render_frame_id != source_render_frame_id)
          return false;
        if (media::AudioDeviceDescription::IsDefaultDevice(device_id) &&
            media::AudioDeviceDescription::IsDefaultDevice(val.device_id)) {
          // Both device IDs represent the same default device => do not compare
          // them; the default device is always authorized => ignore security
          // origin.
          return true;
        }
        return val.device_id == device_id &&
               val.security_origin == security_origin;
      });
};

void AudioRendererSinkCacheImpl::CacheUnusedSinkIfHealthy(
    int source_render_frame_id,
    const std::string& device_id,
    const url::Origin& security_origin,
    scoped_refptr<media::AudioRendererSink> sink) {
  if (!SinkIsHealthy(sink.get()))
    return;

  CacheEntry cache_entry = {source_render_frame_id, device_id, security_origin,
                            sink, false /* not used */};

  {
    base::AutoLock auto_lock(cache_lock_);
    cache_.push_back(cache_entry);
  }

  DeleteLaterIfUnused(cache_entry.sink.get());
}

int AudioRendererSinkCacheImpl::GetCacheSizeForTesting() {
  return cache_.size();
}

}  // namespace content
