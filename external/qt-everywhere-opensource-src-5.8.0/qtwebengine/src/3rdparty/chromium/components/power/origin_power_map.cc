// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power/origin_power_map.h"

#include "base/logging.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

namespace power {

OriginPowerMap::OriginPowerMap() : total_consumed_(0.0) {
}

OriginPowerMap::~OriginPowerMap() {
}

int OriginPowerMap::GetPowerForOrigin(const GURL& url) {
  if (!total_consumed_)
    return 0;

  OriginMap::const_iterator it = origin_map_.find(url.GetOrigin());
  return it == origin_map_.end() ? 0 :
      static_cast<int>(it->second * 100 / total_consumed_ + 0.5);
}

void OriginPowerMap::AddPowerForOrigin(const GURL& url, double power) {
  DCHECK_GE(power, 0);
  GURL origin = url.GetOrigin();
  if (!origin.is_valid() || origin.SchemeIs(content::kChromeUIScheme))
    return;

  origin_map_[origin] += power;
  total_consumed_ += power;
}

OriginPowerMap::PercentOriginMap OriginPowerMap::GetPercentOriginMap() {
  OriginPowerMap::PercentOriginMap percent_map;

  if (!total_consumed_)
    return percent_map;

  for (OriginMap::iterator it = origin_map_.begin(); it != origin_map_.end();
       ++it) {
    percent_map[it->first] =
        static_cast<int>(it->second * 100 / total_consumed_ + 0.5);
  }
  return percent_map;
}

std::unique_ptr<OriginPowerMap::Subscription>
OriginPowerMap::AddPowerConsumptionUpdatedCallback(
    const base::Closure& callback) {
  return callback_list_.Add(callback);
}

void OriginPowerMap::OnAllOriginsUpdated() {
  callback_list_.Notify();
}

void OriginPowerMap::ClearOriginMap() {
  origin_map_.clear();
  total_consumed_ = 0;
}

}  // namespace power
