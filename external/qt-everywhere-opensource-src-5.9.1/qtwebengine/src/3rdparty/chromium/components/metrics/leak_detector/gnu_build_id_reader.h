// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_LEAK_DETECTOR_GNU_BUILD_ID_READER_H_
#define COMPONENTS_METRICS_LEAK_DETECTOR_GNU_BUILD_ID_READER_H_

#include <stdint.h>

#include <vector>

namespace metrics {
namespace leak_detector {
namespace gnu_build_id_reader {

// Reads the build ID from the GNU build notes and stores it in |*build_id|.
// Returns true if it was successfully read, or false otherwise.
bool ReadBuildID(std::vector<uint8_t>* build_id);

}  // namespace gnu_build_id_reader
}  // namespace leak_detector
}  // namespace metrics

#endif  // COMPONENTS_METRICS_LEAK_DETECTOR_GNU_BUILD_ID_READER_H_
