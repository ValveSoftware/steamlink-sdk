// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_net_log.h"

#include <stdio.h>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/values.h"
#include "content/public/common/content_switches.h"
#include "net/log/net_log_util.h"
#include "net/log/write_to_file_net_log_observer.h"

namespace chromecast {

namespace {

base::DictionaryValue* GetShellConstants() {
  std::unique_ptr<base::DictionaryValue> constants_dict =
      net::GetNetConstants();

  // Add a dictionary with client information
  base::DictionaryValue* dict = new base::DictionaryValue();

  dict->SetString("name", "cast_shell");
  dict->SetString(
      "command_line",
      base::CommandLine::ForCurrentProcess()->GetCommandLineString());

  constants_dict->Set("clientInfo", dict);

  return constants_dict.release();
}

}  // namespace

CastNetLog::CastNetLog() {
  // TODO(derekjchow): This code is virtually identical to ShellNetLog which is
  //     nearly identical to code in ChromeNetLog. Consider merging the code.
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(switches::kLogNetLog)) {
    base::FilePath log_path =
        command_line->GetSwitchValuePath(switches::kLogNetLog);
    // Much like logging.h, bypass threading restrictions by using fopen
    // directly.  Have to write on a thread that's shutdown to handle events on
    // shutdown properly, and posting events to another thread as they occur
    // would result in an unbounded buffer size, so not much can be gained by
    // doing this on another thread.  It's only used when debugging, so
    // performance is not a big concern.
    base::ScopedFILE file;
    file.reset(fopen(log_path.value().c_str(), "w"));

    if (!file) {
      LOG(ERROR) << "Could not open file " << log_path.value()
                 << " for net logging";
    } else {
      std::unique_ptr<base::Value> constants(GetShellConstants());
      write_to_file_observer_.reset(new net::WriteToFileNetLogObserver());
      write_to_file_observer_->StartObserving(this, std::move(file),
                                              constants.get(), nullptr);
    }
  }
}

CastNetLog::~CastNetLog() {
  // Remove the observer we own before we're destroyed.
  if (write_to_file_observer_)
    write_to_file_observer_->StopObserving(nullptr);
}

}  // namespace chromecast
