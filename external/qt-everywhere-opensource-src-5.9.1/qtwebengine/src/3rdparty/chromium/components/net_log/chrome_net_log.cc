// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/net_log/chrome_net_log.h"

#include <stdio.h>
#include <utility>

#include "base/command_line.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_store.h"
#include "components/net_log/net_log_file_writer.h"
#include "components/version_info/version_info.h"
#include "net/log/net_log_util.h"
#include "net/log/trace_net_log_observer.h"
#include "net/log/write_to_file_net_log_observer.h"

namespace net_log {

ChromeNetLog::ChromeNetLog(
    const base::FilePath& log_file,
    net::NetLogCaptureMode log_file_mode,
    const base::CommandLine::StringType& command_line_string,
    const std::string& channel_string)
    : net_log_file_writer_(
          new NetLogFileWriter(this, command_line_string, channel_string)) {
  if (!log_file.empty()) {
    // Much like logging.h, bypass threading restrictions by using fopen
    // directly.  Have to write on a thread that's shutdown to handle events on
    // shutdown properly, and posting events to another thread as they occur
    // would result in an unbounded buffer size, so not much can be gained by
    // doing this on another thread.  It's only used when debugging Chrome, so
    // performance is not a big concern.
    base::ScopedFILE file;
#if defined(OS_WIN)
    file.reset(_wfopen(log_file.value().c_str(), L"w"));
#elif defined(OS_POSIX)
    file.reset(fopen(log_file.value().c_str(), "w"));
#endif

    if (!file) {
      LOG(ERROR) << "Could not open file " << log_file.value()
                 << " for net logging";
    } else {
      std::unique_ptr<base::Value> constants(
          GetConstants(command_line_string, channel_string));
      write_to_file_observer_.reset(new net::WriteToFileNetLogObserver());

      write_to_file_observer_->set_capture_mode(log_file_mode);

      write_to_file_observer_->StartObserving(this, std::move(file),
                                              constants.get(), nullptr);
    }
  }

  trace_net_log_observer_.reset(new net::TraceNetLogObserver());
  trace_net_log_observer_->WatchForTraceStart(this);
}

ChromeNetLog::~ChromeNetLog() {
  net_log_file_writer_.reset();
  // Remove the observers we own before we're destroyed.
  if (write_to_file_observer_)
    write_to_file_observer_->StopObserving(nullptr);
  if (trace_net_log_observer_)
    trace_net_log_observer_->StopWatchForTraceStart();
}

// static
std::unique_ptr<base::Value> ChromeNetLog::GetConstants(
    const base::CommandLine::StringType& command_line_string,
    const std::string& channel_string) {
  std::unique_ptr<base::DictionaryValue> constants_dict =
      net::GetNetConstants();
  DCHECK(constants_dict);

  // Add a dictionary with the version of the client and its command line
  // arguments.
  auto dict = base::MakeUnique<base::DictionaryValue>();

  // We have everything we need to send the right values.
  dict->SetString("name", version_info::GetProductName());
  dict->SetString("version", version_info::GetVersionNumber());
  dict->SetString("cl", version_info::GetLastChange());
  dict->SetString("version_mod", channel_string);
  dict->SetString("official",
                  version_info::IsOfficialBuild() ? "official" : "unofficial");
  std::string os_type = base::StringPrintf(
      "%s: %s (%s)", base::SysInfo::OperatingSystemName().c_str(),
      base::SysInfo::OperatingSystemVersion().c_str(),
      base::SysInfo::OperatingSystemArchitecture().c_str());
  dict->SetString("os_type", os_type);
  dict->SetString("command_line", command_line_string);

  constants_dict->Set("clientInfo", std::move(dict));

  data_reduction_proxy::DataReductionProxyEventStore::AddConstants(
      constants_dict.get());

  return std::move(constants_dict);
}

}  // namespace net_log
