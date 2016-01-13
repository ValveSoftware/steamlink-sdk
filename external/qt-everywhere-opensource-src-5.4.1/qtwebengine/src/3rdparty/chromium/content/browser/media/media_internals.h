// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_INTERNALS_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_INTERNALS_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/strings/string16.h"
#include "base/synchronization/lock.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "media/audio/audio_logging.h"

namespace media {
class AudioParameters;
struct MediaLogEvent;
}

namespace content {

// This class stores information about currently active media.
class CONTENT_EXPORT MediaInternals
    : NON_EXPORTED_BASE(public media::AudioLogFactory) {
 public:
  static MediaInternals* GetInstance();

  virtual ~MediaInternals();

  // Called when a MediaEvent occurs.
  void OnMediaEvents(int render_process_id,
                     const std::vector<media::MediaLogEvent>& events);

  // Called with the update string.
  typedef base::Callback<void(const base::string16&)> UpdateCallback;

  // Add/remove update callbacks (see above).  Must be called on the IO thread.
  void AddUpdateCallback(const UpdateCallback& callback);
  void RemoveUpdateCallback(const UpdateCallback& callback);

  // Sends all cached data to each registered UpdateCallback.
  void SendEverything();

  // AudioLogFactory implementation.  Safe to call from any thread.
  virtual scoped_ptr<media::AudioLog> CreateAudioLog(
      AudioComponent component) OVERRIDE;

 private:
  friend class AudioLogImpl;
  friend class MediaInternalsTest;
  friend struct base::DefaultLazyInstanceTraits<MediaInternals>;

  MediaInternals();

  // Sends |update| to each registered UpdateCallback.  Safe to call from any
  // thread, but will forward to the IO thread.
  void SendUpdate(const base::string16& update);

  // Caches |value| under |cache_key| so that future SendEverything() calls will
  // include the current data.  Calls JavaScript |function|(|value|) for each
  // registered UpdateCallback.  SendUpdateAndPurgeCache() is similar but purges
  // the cache entry after completion instead.
  void SendUpdateAndCache(const std::string& cache_key,
                          const std::string& function,
                          const base::DictionaryValue* value);
  void SendUpdateAndPurgeCache(const std::string& cache_key,
                               const std::string& function,
                               const base::DictionaryValue* value);
  // Must only be accessed on the IO thread.
  std::vector<UpdateCallback> update_callbacks_;

  // All variables below must be accessed under |lock_|.
  base::Lock lock_;
  base::DictionaryValue cached_data_;
  int owner_ids_[AUDIO_COMPONENT_MAX];

  DISALLOW_COPY_AND_ASSIGN(MediaInternals);
};

} // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_INTERNALS_H_
