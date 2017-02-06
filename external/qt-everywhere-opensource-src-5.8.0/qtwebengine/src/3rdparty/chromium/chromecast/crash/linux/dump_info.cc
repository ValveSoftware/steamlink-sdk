// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chromecast/crash/linux/dump_info.h"

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"

namespace chromecast {

namespace {

const char kDumpTimeFormat[] = "%Y-%m-%d %H:%M:%S";
const unsigned kDumpTimeMaxLen = 255;

const int kNumRequiredParams = 5;

const char kNameKey[] = "name";
const char kDumpTimeKey[] = "dump_time";
const char kDumpKey[] = "dump";
const char kUptimeKey[] = "uptime";
const char kLogfileKey[] = "logfile";
const char kSuffixKey[] = "suffix";
const char kPrevAppNameKey[] = "prev_app_name";
const char kCurAppNameKey[] = "cur_app_name";
const char kLastAppNameKey[] = "last_app_name";
const char kReleaseVersionKey[] = "release_version";
const char kBuildNumberKey[] = "build_number";
const char kReasonKey[] = "reason";

}  // namespace

DumpInfo::DumpInfo(const base::Value* entry) : valid_(ParseEntry(entry)) {
}

DumpInfo::DumpInfo(const std::string& crashed_process_dump,
                   const std::string& logfile,
                   const time_t& dump_time,
                   const MinidumpParams& params)
    : crashed_process_dump_(crashed_process_dump),
      logfile_(logfile),
      dump_time_(dump_time),
      params_(params),
      valid_(false) {

  // Validate the time passed in.
  struct tm* tm = gmtime(&dump_time);
  char buf[kDumpTimeMaxLen];
  int n = strftime(buf, kDumpTimeMaxLen, kDumpTimeFormat, tm);
  if (n <= 0) {
    LOG(INFO) << "strftime failed";
    return;
  }

  valid_ = true;
}

DumpInfo::~DumpInfo() {
}

std::unique_ptr<base::Value> DumpInfo::GetAsValue() const {
  std::unique_ptr<base::Value> result =
      base::WrapUnique(new base::DictionaryValue());
  base::DictionaryValue* entry;
  result->GetAsDictionary(&entry);
  entry->SetString(kNameKey, params_.process_name);

  struct tm* tm = gmtime(&dump_time_);
  char buf[kDumpTimeMaxLen];
  int n = strftime(buf, kDumpTimeMaxLen, kDumpTimeFormat, tm);
  DCHECK_GT(n, 0);
  std::string dump_time(buf);
  entry->SetString(kDumpTimeKey, dump_time);

  entry->SetString(kDumpKey, crashed_process_dump_);
  std::string uptime = std::to_string(params_.process_uptime);
  entry->SetString(kUptimeKey, uptime);
  entry->SetString(kLogfileKey, logfile_);
  entry->SetString(kSuffixKey, params_.suffix);
  entry->SetString(kPrevAppNameKey, params_.previous_app_name);
  entry->SetString(kCurAppNameKey, params_.current_app_name);
  entry->SetString(kLastAppNameKey, params_.last_app_name);
  entry->SetString(kReleaseVersionKey, params_.cast_release_version);
  entry->SetString(kBuildNumberKey, params_.cast_build_number);
  entry->SetString(kReasonKey, params_.reason);

  return result;
}

bool DumpInfo::ParseEntry(const base::Value* entry) {
  valid_ = false;

  if (!entry)
    return false;

  const base::DictionaryValue* dict;
  if (!entry->GetAsDictionary(&dict))
    return false;

  // Extract required fields.
  if (!dict->GetString(kNameKey, &params_.process_name))
    return false;

  std::string dump_time;
  if (!dict->GetString(kDumpTimeKey, &dump_time))
    return false;
  if (!SetDumpTimeFromString(dump_time))
    return false;

  if (!dict->GetString(kDumpKey, &crashed_process_dump_))
    return false;

  std::string uptime;
  if (!dict->GetString(kUptimeKey, &uptime))
    return false;
  errno = 0;
  params_.process_uptime = strtoull(uptime.c_str(), nullptr, 0);
  if (errno != 0)
    return false;

  if (!dict->GetString(kLogfileKey, &logfile_))
    return false;
  size_t num_params = kNumRequiredParams;

  // Extract all other optional fields.
  if (dict->GetString(kSuffixKey, &params_.suffix))
    ++num_params;
  if (dict->GetString(kPrevAppNameKey, &params_.previous_app_name))
    ++num_params;
  if (dict->GetString(kCurAppNameKey, &params_.current_app_name))
    ++num_params;
  if (dict->GetString(kLastAppNameKey, &params_.last_app_name))
    ++num_params;
  if (dict->GetString(kReleaseVersionKey, &params_.cast_release_version))
    ++num_params;
  if (dict->GetString(kBuildNumberKey, &params_.cast_build_number))
    ++num_params;
  if (dict->GetString(kReasonKey, &params_.reason))
    ++num_params;

  // Disallow extraneous params
  if (dict->size() != num_params)
    return false;

  valid_ = true;
  return true;
}

bool DumpInfo::SetDumpTimeFromString(const std::string& timestr) {
  struct tm tm = {0};
  char* text = strptime(timestr.c_str(), kDumpTimeFormat, &tm);
  dump_time_ = mktime(&tm);
  if (!text || dump_time_ < 0) {
    LOG(INFO) << "Failed to convert dump time invalid";
    return false;
  }
  return true;
}

}  // namespace chromecast
