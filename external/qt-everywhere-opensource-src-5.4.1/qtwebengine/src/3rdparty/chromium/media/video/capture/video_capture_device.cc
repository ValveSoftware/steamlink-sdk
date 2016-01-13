// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/video_capture_device.h"

#include "base/i18n/timezone.h"
#include "base/strings/string_util.h"

namespace media {

const std::string VideoCaptureDevice::Name::GetNameAndModel() const {
  const std::string model_id = GetModel();
  if (model_id.empty())
    return device_name_;
  const std::string suffix = " (" + model_id + ")";
  if (EndsWith(device_name_, suffix, true))  // |true| means case-sensitive.
    return device_name_;
  return device_name_ + suffix;
}

VideoCaptureDevice::~VideoCaptureDevice() {}

int VideoCaptureDevice::GetPowerLineFrequencyForLocation() const {
  std::string current_country = base::CountryCodeForCurrentTimezone();
  if (current_country.empty())
    return 0;
  // Sorted out list of countries with 60Hz power line frequency, from
  // http://en.wikipedia.org/wiki/Mains_electricity_by_country
  const char* countries_using_60Hz[] = {
      "AI", "AO", "AS", "AW", "AZ", "BM", "BR", "BS", "BZ", "CA", "CO",
      "CR", "CU", "DO", "EC", "FM", "GT", "GU", "GY", "HN", "HT", "JP",
      "KN", "KR", "KY", "MS", "MX", "NI", "PA", "PE", "PF", "PH", "PR",
      "PW", "SA", "SR", "SV", "TT", "TW", "UM", "US", "VG", "VI", "VE"};
  const char** countries_using_60Hz_end =
      countries_using_60Hz + arraysize(countries_using_60Hz);
  if (std::find(countries_using_60Hz, countries_using_60Hz_end,
                current_country) == countries_using_60Hz_end) {
    return kPowerLine50Hz;
  }
  return kPowerLine60Hz;
}

}  // namespace media
