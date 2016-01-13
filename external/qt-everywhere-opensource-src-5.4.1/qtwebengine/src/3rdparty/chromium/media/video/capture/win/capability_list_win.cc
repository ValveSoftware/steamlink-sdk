// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/win/capability_list_win.h"

#include <algorithm>

#include "base/logging.h"

namespace media {
namespace {

// Help structure used for comparing video capture capabilities.
struct ResolutionDiff {
  const VideoCaptureCapabilityWin* capability;
  int diff_height;
  int diff_width;
  int diff_frame_rate;
};

bool CompareHeight(const ResolutionDiff& item1, const ResolutionDiff& item2) {
  return abs(item1.diff_height) < abs(item2.diff_height);
}

bool CompareWidth(const ResolutionDiff& item1, const ResolutionDiff& item2) {
  return abs(item1.diff_width) < abs(item2.diff_width);
}

bool CompareFrameRate(const ResolutionDiff& item1,
                      const ResolutionDiff& item2) {
  return abs(item1.diff_frame_rate) < abs(item2.diff_frame_rate);
}

bool CompareColor(const ResolutionDiff& item1, const ResolutionDiff& item2) {
  return item1.capability->supported_format.pixel_format <
         item2.capability->supported_format.pixel_format;
}

}  // namespace.

CapabilityList::CapabilityList() {
  DetachFromThread();
}

CapabilityList::~CapabilityList() {}

// Appends an entry to the list.
void CapabilityList::Add(const VideoCaptureCapabilityWin& capability) {
  DCHECK(CalledOnValidThread());
  capabilities_.push_back(capability);
}

const VideoCaptureCapabilityWin& CapabilityList::GetBestMatchedFormat(
    int requested_width,
    int requested_height,
    float requested_frame_rate) const {
  DCHECK(CalledOnValidThread());
  DCHECK(!capabilities_.empty());

  std::list<ResolutionDiff> diff_list;

  // Loop through the candidates to create a list of differentials between the
  // requested resolution and the camera capability.
  for (Capabilities::const_iterator it = capabilities_.begin();
       it != capabilities_.end(); ++it) {
    ResolutionDiff diff;
    diff.capability = &(*it);
    diff.diff_width = it->supported_format.frame_size.width() - requested_width;
    diff.diff_height =
        it->supported_format.frame_size.height() - requested_height;
    // The 1000 allows using integer arithmetic for f.i. 29.971 fps.
    diff.diff_frame_rate =
        1000 * ((static_cast<float>(it->frame_rate_numerator) /
                 it->frame_rate_denominator) -
                requested_frame_rate);
    diff_list.push_back(diff);
  }

  // Sort the best height candidates.
  diff_list.sort(&CompareHeight);
  int best_diff = diff_list.front().diff_height;
  for (std::list<ResolutionDiff>::iterator it = diff_list.begin();
       it != diff_list.end(); ++it) {
    if (it->diff_height != best_diff) {
      // Remove all candidates but the best.
      diff_list.erase(it, diff_list.end());
      break;
    }
  }

  // Sort the best width candidates.
  diff_list.sort(&CompareWidth);
  best_diff = diff_list.front().diff_width;
  for (std::list<ResolutionDiff>::iterator it = diff_list.begin();
       it != diff_list.end(); ++it) {
    if (it->diff_width != best_diff) {
      // Remove all candidates but the best.
      diff_list.erase(it, diff_list.end());
      break;
    }
  }

  // Sort the best frame rate candidates.
  diff_list.sort(&CompareFrameRate);
  best_diff = diff_list.front().diff_frame_rate;
  for (std::list<ResolutionDiff>::iterator it = diff_list.begin();
       it != diff_list.end(); ++it) {
    if (it->diff_frame_rate != best_diff) {
      diff_list.erase(it, diff_list.end());
      break;
    }
  }

  // Decide the best color format.
  diff_list.sort(&CompareColor);
  return *diff_list.front().capability;
}

}  // namespace media
