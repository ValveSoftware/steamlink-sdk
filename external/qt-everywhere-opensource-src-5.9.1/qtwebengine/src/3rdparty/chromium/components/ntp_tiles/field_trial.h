// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_FIELD_TRIAL_H_
#define COMPONENTS_NTP_TILES_FIELD_TRIAL_H_

namespace ntp_tiles {

void SetUpFirstLaunchFieldTrial(bool is_stable_channel);

bool ShouldShowPopularSites();

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_FIELD_TRIAL_H_
