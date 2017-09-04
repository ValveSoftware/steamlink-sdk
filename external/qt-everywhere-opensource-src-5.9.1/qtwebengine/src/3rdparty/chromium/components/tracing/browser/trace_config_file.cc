// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/browser/trace_config_file.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/tracing/common/tracing_switches.h"

namespace tracing {

namespace {

// Maximum trace config file size that will be loaded, in bytes.
const size_t kTraceConfigFileSizeLimit = 64 * 1024;

// Trace config file path:
// - Android: /data/local/chrome-trace-config.json
// - Others: specified by --trace-config-file flag.
#if defined(OS_ANDROID)
const base::FilePath::CharType kAndroidTraceConfigFile[] =
    FILE_PATH_LITERAL("/data/local/chrome-trace-config.json");
#endif

const base::FilePath::CharType kDefaultResultFile[] =
    FILE_PATH_LITERAL("chrometrace.log");

// String parameters that can be used to parse the trace config file content.
const char kTraceConfigParam[] = "trace_config";
const char kStartupDurationParam[] = "startup_duration";
const char kResultFileParam[] = "result_file";

} // namespace

TraceConfigFile* TraceConfigFile::GetInstance() {
  return base::Singleton<TraceConfigFile,
                         base::DefaultSingletonTraits<TraceConfigFile>>::get();
}

TraceConfigFile::TraceConfigFile()
    : is_enabled_(false),
      trace_config_(base::trace_event::TraceConfig()),
      startup_duration_(0),
      result_file_(kDefaultResultFile) {
#if defined(OS_ANDROID)
  base::FilePath trace_config_file(kAndroidTraceConfigFile);
#else
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(switches::kTraceConfigFile) ||
      command_line.HasSwitch(switches::kTraceStartup) ||
      command_line.HasSwitch(switches::kTraceShutdown)) {
    return;
  }
  base::FilePath trace_config_file =
      command_line.GetSwitchValuePath(switches::kTraceConfigFile);
#endif

  if (trace_config_file.empty()) {
    // If the trace config file path is not specified, trace Chrome with the
    // default configuration for 5 sec.
    startup_duration_ = 5;
    is_enabled_ = true;
    DLOG(WARNING) << "Use default trace config.";
    return;
  }

  if (!base::PathExists(trace_config_file)) {
    DLOG(WARNING) << "The trace config file does not exist.";
    return;
  }

  std::string trace_config_file_content;
  if (!base::ReadFileToStringWithMaxSize(trace_config_file,
                                         &trace_config_file_content,
                                         kTraceConfigFileSizeLimit)) {
    DLOG(WARNING) << "Cannot read the trace config file correctly.";
    return;
  }
  is_enabled_ = ParseTraceConfigFileContent(trace_config_file_content);
  if (!is_enabled_)
    DLOG(WARNING) << "Cannot parse the trace config file correctly.";
}

TraceConfigFile::~TraceConfigFile() {
}

bool TraceConfigFile::ParseTraceConfigFileContent(const std::string& content) {
  std::unique_ptr<base::Value> value(base::JSONReader::Read(content));
  if (!value || !value->IsType(base::Value::TYPE_DICTIONARY))
    return false;

  std::unique_ptr<base::DictionaryValue> dict(
      static_cast<base::DictionaryValue*>(value.release()));

  base::DictionaryValue* trace_config_dict = NULL;
  if (!dict->GetDictionary(kTraceConfigParam, &trace_config_dict))
    return false;

  trace_config_ = base::trace_event::TraceConfig(*trace_config_dict);

  if (!dict->GetInteger(kStartupDurationParam, &startup_duration_))
      startup_duration_ = 0;

  if (startup_duration_ < 0)
      startup_duration_ = 0;

  base::FilePath::StringType result_file_str;
  if (dict->GetString(kResultFileParam, &result_file_str))
    result_file_ = base::FilePath(result_file_str);

  return true;
}

bool TraceConfigFile::IsEnabled() const {
  return is_enabled_;
}

base::trace_event::TraceConfig TraceConfigFile::GetTraceConfig() const {
  DCHECK(IsEnabled());
  return trace_config_;
}

int TraceConfigFile::GetStartupDuration() const {
  DCHECK(IsEnabled());
  return startup_duration_;
}

#if !defined(OS_ANDROID)
base::FilePath TraceConfigFile::GetResultFile() const {
  DCHECK(IsEnabled());
  return result_file_;
}
#endif

}  // namespace tracing
