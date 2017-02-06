// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/time_zone_monitor.h"

#include <stddef.h>
#include <stdlib.h>

#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"

#if !defined(OS_CHROMEOS)

namespace content {

namespace {
class TimeZoneMonitorLinuxImpl;
}  // namespace

class TimeZoneMonitorLinux : public TimeZoneMonitor {
 public:
  TimeZoneMonitorLinux();
  ~TimeZoneMonitorLinux() override;

  void NotifyRenderersFromImpl() {
    NotifyRenderers();
  }

 private:
  scoped_refptr<TimeZoneMonitorLinuxImpl> impl_;

  DISALLOW_COPY_AND_ASSIGN(TimeZoneMonitorLinux);
};

namespace {

// FilePathWatcher needs to run on the FILE thread, but TimeZoneMonitor runs
// on the UI thread. TimeZoneMonitorLinuxImpl is the bridge between these
// threads.
class TimeZoneMonitorLinuxImpl
    : public base::RefCountedThreadSafe<TimeZoneMonitorLinuxImpl> {
 public:
  explicit TimeZoneMonitorLinuxImpl(TimeZoneMonitorLinux* owner)
      : base::RefCountedThreadSafe<TimeZoneMonitorLinuxImpl>(),
        file_path_watchers_(),
        owner_(owner) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&TimeZoneMonitorLinuxImpl::StartWatchingOnFileThread, this));
  }

  void StopWatching() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    owner_ = NULL;
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&TimeZoneMonitorLinuxImpl::StopWatchingOnFileThread, this));
  }

 private:
  friend class base::RefCountedThreadSafe<TimeZoneMonitorLinuxImpl>;

  ~TimeZoneMonitorLinuxImpl() {
    DCHECK(!owner_);
    STLDeleteElements(&file_path_watchers_);
  }

  void StartWatchingOnFileThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::FILE);

    // There is no true standard for where time zone information is actually
    // stored. glibc uses /etc/localtime, uClibc uses /etc/TZ, and some older
    // systems store the name of the time zone file within /usr/share/zoneinfo
    // in /etc/timezone. Different libraries and custom builds may mean that
    // still more paths are used. Just watch all three of these paths, because
    // false positives are harmless, assuming the false positive rate is
    // reasonable.
    const char* const kFilesToWatch[] = {
      "/etc/localtime",
      "/etc/timezone",
      "/etc/TZ",
    };

    for (size_t index = 0; index < arraysize(kFilesToWatch); ++index) {
      file_path_watchers_.push_back(new base::FilePathWatcher());
      file_path_watchers_.back()->Watch(
          base::FilePath(kFilesToWatch[index]),
          false,
          base::Bind(&TimeZoneMonitorLinuxImpl::OnTimeZoneFileChanged, this));
    }
  }

  void StopWatchingOnFileThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::FILE);
    STLDeleteElements(&file_path_watchers_);
  }

  void OnTimeZoneFileChanged(const base::FilePath& path, bool error) {
    DCHECK_CURRENTLY_ON(BrowserThread::FILE);
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&TimeZoneMonitorLinuxImpl::OnTimeZoneFileChangedOnUIThread,
                   this));
  }

  void OnTimeZoneFileChangedOnUIThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (owner_) {
      owner_->NotifyRenderersFromImpl();
    }
  }

  std::vector<base::FilePathWatcher*> file_path_watchers_;
  TimeZoneMonitorLinux* owner_;

  DISALLOW_COPY_AND_ASSIGN(TimeZoneMonitorLinuxImpl);
};

}  // namespace

TimeZoneMonitorLinux::TimeZoneMonitorLinux()
    : TimeZoneMonitor(),
      impl_() {
  // If the TZ environment variable is set, its value specifies the time zone
  // specification, and it's pointless to monitor any files in /etc for
  // changes because such changes would have no effect on the TZ environment
  // variable and thus the interpretation of the local time zone in the
  // or renderer processes.
  //
  // The system-specific format for the TZ environment variable beginning with
  // a colon is implemented by glibc as the path to a time zone data file, and
  // it would be possible to monitor this file for changes if a TZ variable of
  // this format was encountered, but this is not necessary: when loading a
  // time zone specification in this way, glibc does not reload the file when
  // it changes, so it's pointless to respond to a notification that it has
  // changed.
  if (!getenv("TZ")) {
    impl_ = new TimeZoneMonitorLinuxImpl(this);
  }
}

TimeZoneMonitorLinux::~TimeZoneMonitorLinux() {
  if (impl_.get()) {
    impl_->StopWatching();
  }
}

// static
std::unique_ptr<TimeZoneMonitor> TimeZoneMonitor::Create() {
  return std::unique_ptr<TimeZoneMonitor>(new TimeZoneMonitorLinux());
}

}  // namespace content

#endif  // !OS_CHROMEOS
