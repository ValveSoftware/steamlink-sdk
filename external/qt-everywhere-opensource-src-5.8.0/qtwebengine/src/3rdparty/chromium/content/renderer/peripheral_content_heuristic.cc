// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/peripheral_content_heuristic.h"

#include <cmath>

#include "ui/gfx/geometry/size.h"

namespace content {

namespace {

// Content below this size in height and width is considered "tiny".
// Tiny content is never peripheral, as tiny plugins often serve a critical
// purpose, and the user often cannot find and click to unthrottle it.
const int kTinyContentSize = 5;

// Cross-origin content must have a width and height both exceeding these
// minimums to be considered "large", and thus not peripheral.
const int kLargeContentMinWidth = 398;
const int kLargeContentMinHeight = 298;

// Mark some 16:9 aspect ratio content as essential (not peripheral). This is to
// mark as "large" some medium sized video content that meets a minimum area
// requirement, even if it is below the max width/height above.
const double kEssentialVideoAspectRatio = 16.0 / 9.0;
const double kAspectRatioEpsilon = 0.01;
const int kEssentialVideoMinimumArea = 120000;

}  // namespace

// static
RenderFrame::PeripheralContentStatus
PeripheralContentHeuristic::GetPeripheralStatus(
    const std::set<url::Origin>& origin_whitelist,
    const url::Origin& main_frame_origin,
    const url::Origin& content_origin,
    const gfx::Size& unobscured_size) {
  if (main_frame_origin.IsSameOriginWith(content_origin))
    return RenderFrame::CONTENT_STATUS_ESSENTIAL_SAME_ORIGIN;

  if (origin_whitelist.count(content_origin))
    return RenderFrame::CONTENT_STATUS_ESSENTIAL_CROSS_ORIGIN_WHITELISTED;

  if (unobscured_size.IsEmpty())
    return RenderFrame::CONTENT_STATUS_ESSENTIAL_UNKNOWN_SIZE;

  if (unobscured_size.width() <= kTinyContentSize &&
      unobscured_size.height() <= kTinyContentSize) {
    return RenderFrame::CONTENT_STATUS_ESSENTIAL_CROSS_ORIGIN_TINY;
  }

  if (IsLargeContent(unobscured_size))
    return RenderFrame::CONTENT_STATUS_ESSENTIAL_CROSS_ORIGIN_BIG;

  return RenderFrame::CONTENT_STATUS_PERIPHERAL;
}

// static
bool PeripheralContentHeuristic::IsLargeContent(
    const gfx::Size& unobscured_size) {
  int width = unobscured_size.width();
  int height = unobscured_size.height();
  if (width >= kLargeContentMinWidth && height >= kLargeContentMinHeight)
    return true;

  double aspect_ratio = static_cast<double>(width) / height;
  if (std::abs(aspect_ratio - kEssentialVideoAspectRatio) <
          kAspectRatioEpsilon &&
      width * height >= kEssentialVideoMinimumArea) {
    return true;
  }

  return false;
}

}  // namespace content
