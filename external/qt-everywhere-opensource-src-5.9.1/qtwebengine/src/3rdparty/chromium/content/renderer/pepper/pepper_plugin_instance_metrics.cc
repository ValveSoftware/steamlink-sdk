// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_plugin_instance_metrics.h"

#include <stddef.h>

#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "build/build_config.h"
#include "ppapi/shared_impl/ppapi_preferences.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

#define UMA_HISTOGRAM_ASPECT_RATIO(name, width, height) \
  UMA_HISTOGRAM_SPARSE_SLOWLY(                          \
      name, (height) ? ((width)*100) / (height) : kInfiniteRatio);

namespace content {

namespace {

// Histogram tracking prevalence of tiny Flash instances. Units in pixels.
enum PluginFlashTinyContentSize {
  TINY_CONTENT_SIZE_1_1 = 0,
  TINY_CONTENT_SIZE_5_5 = 1,
  TINY_CONTENT_SIZE_10_10 = 2,
  TINY_CONTENT_SIZE_LARGE = 3,
  TINY_CONTENT_SIZE_NUM_ITEMS
};

const int kInfiniteRatio = 99999;

const char kFlashClickSizeAspectRatioHistogram[] =
    "Plugin.Flash.ClickSize.AspectRatio";
const char kFlashClickSizeHeightHistogram[] = "Plugin.Flash.ClickSize.Height";
const char kFlashClickSizeWidthHistogram[] = "Plugin.Flash.ClickSize.Width";
const char kFlashTinyContentSizeHistogram[] = "Plugin.Flash.TinyContentSize";

}  // namespace

void RecordFlashSizeMetric(int width, int height) {
  PluginFlashTinyContentSize size = TINY_CONTENT_SIZE_LARGE;

  if (width <= 1 && height <= 1)
    size = TINY_CONTENT_SIZE_1_1;
  else if (width <= 5 && height <= 5)
    size = TINY_CONTENT_SIZE_5_5;
  else if (width <= 10 && height <= 10)
    size = TINY_CONTENT_SIZE_10_10;

  UMA_HISTOGRAM_ENUMERATION(kFlashTinyContentSizeHistogram, size,
                            TINY_CONTENT_SIZE_NUM_ITEMS);
}

void RecordFlashClickSizeMetric(int width, int height) {
  base::HistogramBase* width_histogram = base::LinearHistogram::FactoryGet(
      kFlashClickSizeWidthHistogram,
      0,    // minimum width
      500,  // maximum width
      100,  // number of buckets.
      base::HistogramBase::kUmaTargetedHistogramFlag);
  width_histogram->Add(width);

  base::HistogramBase* height_histogram = base::LinearHistogram::FactoryGet(
      kFlashClickSizeHeightHistogram,
      0,    // minimum height
      400,  // maximum height
      100,  // number of buckets.
      base::HistogramBase::kUmaTargetedHistogramFlag);
  height_histogram->Add(height);

  UMA_HISTOGRAM_ASPECT_RATIO(kFlashClickSizeAspectRatioHistogram, width,
                             height);
}

void SetGPUHistogram(const ppapi::Preferences& prefs,
                     const std::vector<std::string>& arg_names,
                     const std::vector<std::string>& arg_values) {
// Calculate a histogram to let us determine how likely people are to try to
// run Stage3D content on machines that have it blacklisted.
#if defined(OS_WIN)
  bool needs_gpu = false;
  bool is_xp = base::win::GetVersion() <= base::win::VERSION_XP;

  for (size_t i = 0; i < arg_names.size(); i++) {
    if (arg_names[i] == "wmode") {
      // In theory content other than Flash could have a "wmode" argument,
      // but that's pretty unlikely.
      if (arg_values[i] == "direct" || arg_values[i] == "gpu")
        needs_gpu = true;
      break;
    }
  }
  // 0 : No 3D content and GPU is blacklisted
  // 1 : No 3D content and GPU is not blacklisted
  // 2 : 3D content but GPU is blacklisted
  // 3 : 3D content and GPU is not blacklisted
  // 4 : No 3D content and GPU is blacklisted on XP
  // 5 : No 3D content and GPU is not blacklisted on XP
  // 6 : 3D content but GPU is blacklisted on XP
  // 7 : 3D content and GPU is not blacklisted on XP
  UMA_HISTOGRAM_ENUMERATION(
      "Flash.UsesGPU", is_xp * 4 + needs_gpu * 2 + prefs.is_webgl_supported, 8);
#endif
}

}  // namespace content
