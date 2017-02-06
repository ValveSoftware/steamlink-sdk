// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_task_runner.h"

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "base/threading/thread.h"
#include "media/base/media_switches.h"

namespace media {

class MediaThread : public base::Thread {
 public:
  MediaThread() : base::Thread("BrowserMediaThread") {
    Start();
  }
};

// Create media thread
base::LazyInstance<MediaThread>::Leaky g_media_thread =
    LAZY_INSTANCE_INITIALIZER;

scoped_refptr<base::SingleThreadTaskRunner> GetMediaTaskRunner() {
  return g_media_thread.Pointer()->task_runner();
}

bool UseMediaThreadForMediaPlayback() {
  const std::string group_name =
      base::FieldTrialList::FindFullName("EnableMediaThreadForMediaPlayback");

  // Command line switches take precedence over filed trial groups.
  // The disable switch takes precedence over enable switch.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableMediaThreadForMediaPlayback)) {
    return false;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableMediaThreadForMediaPlayback)) {
    return true;
  }

  DVLOG(1) << __FUNCTION__ << ": group_name:'" << group_name << "'";

  return base::StartsWith(group_name, "Enabled", base::CompareCase::SENSITIVE);
}

}  // namespace media
