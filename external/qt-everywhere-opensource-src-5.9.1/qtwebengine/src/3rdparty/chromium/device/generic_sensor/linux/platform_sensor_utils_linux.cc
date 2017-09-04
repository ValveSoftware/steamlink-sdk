// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/generic_sensor/linux/platform_sensor_utils_linux.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "device/generic_sensor/public/cpp/sensor_reading.h"

namespace device {

namespace {

bool InitSensorPaths(const std::vector<std::string>& input_names,
                     const char* base_path,
                     std::vector<base::FilePath>* sensor_paths) {
  // Search the iio/devices directory for a subdirectory (eg "device0" or
  // "iio:device0") that contains the specified input_name file (eg
  // "in_illuminance_input" or "in_illuminance0_input").
  base::FileEnumerator dir_enumerator(base::FilePath(base_path), false,
                                      base::FileEnumerator::DIRECTORIES);
  for (base::FilePath check_path = dir_enumerator.Next(); !check_path.empty();
       check_path = dir_enumerator.Next()) {
    for (auto const& file_name : input_names) {
      base::FilePath full_path = check_path.Append(file_name);
      if (base::PathExists(full_path)) {
        sensor_paths->push_back(full_path);
        return true;
      }
    }
  }
  return false;
}

bool GetSensorFilePaths(const SensorDataLinux& data,
                        std::vector<base::FilePath>* sensor_paths) {
  DCHECK(sensor_paths->empty());
  // Depending on a sensor, there can be up to three sets of files that need
  // to be checked. If one of three files is not found, a sensor is
  // treated as a non-existing one.
  for (auto const& file_names : data.sensor_file_names) {
    // Supply InitSensorPaths() with a set of files.
    // Only one file from each set should be found.
    if (!InitSensorPaths(file_names, data.base_path_sensor_linux, sensor_paths))
      return false;
  }
  return true;
}

// Returns -1 if unable to read a scaling value.
// Otherwise, returns a scaling value read from a file.
double ReadSensorScalingValue(const base::FilePath& scale_file_path) {
  std::string value;
  if (!base::ReadFileToString(scale_file_path, &value))
    return -1;

  double scaling_value;
  base::TrimWhitespaceASCII(value, base::TRIM_ALL, &value);
  if (!base::StringToDouble(value, &scaling_value))
    return -1;
  return scaling_value;
}

}  // namespace

// static
std::unique_ptr<SensorReader> SensorReader::Create(
    const SensorDataLinux& data) {
  base::ThreadRestrictions::AssertIOAllowed();
  std::vector<base::FilePath> sensor_paths;
  if (!GetSensorFilePaths(data, &sensor_paths))
    return nullptr;

  DCHECK(!sensor_paths.empty());
  // Scaling value is 1 if scaling file is not specified is |data| or scaling
  // file is not found.
  double scaling_value = 1;
  if (!data.sensor_scale_name.empty()) {
    const base::FilePath scale_file_path =
        sensor_paths.back().DirName().Append(data.sensor_scale_name);
    if (base::PathExists(scale_file_path))
      scaling_value = ReadSensorScalingValue(scale_file_path);
  }

  // A file with a scaling value is found, but couldn't be read.
  if (scaling_value == -1)
    return nullptr;

  return base::WrapUnique(new SensorReader(
      std::move(sensor_paths), scaling_value, data.apply_scaling_func));
}

SensorReader::SensorReader(
    std::vector<base::FilePath> sensor_paths,
    double scaling_value,
    const SensorDataLinux::ReaderFunctor& apply_scaling_func)
    : sensor_paths_(std::move(sensor_paths)),
      scaling_value_(scaling_value),
      apply_scaling_func_(apply_scaling_func) {
  DCHECK(!sensor_paths_.empty());
}

SensorReader::~SensorReader() = default;

bool SensorReader::ReadSensorReading(SensorReading* reading) {
  base::ThreadRestrictions::AssertIOAllowed();
  SensorReading readings;
  DCHECK_LE(sensor_paths_.size(), arraysize(readings.values));
  int i = 0;
  for (const auto& path : sensor_paths_) {
    std::string new_read_value;
    if (!base::ReadFileToString(path, &new_read_value))
      return false;

    double new_value = 0;
    base::TrimWhitespaceASCII(new_read_value, base::TRIM_ALL, &new_read_value);
    if (!base::StringToDouble(new_read_value, &new_value))
      return false;
    readings.values[i++] = new_value;
  }
  if (!apply_scaling_func_.is_null())
    apply_scaling_func_.Run(scaling_value_, readings);
  *reading = readings;
  return true;
}

}  // namespace device
