// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/common/page_load_timing.h"

namespace page_load_metrics {

PageLoadTiming::PageLoadTiming() {}

PageLoadTiming::PageLoadTiming(const PageLoadTiming& other) = default;

PageLoadTiming::~PageLoadTiming() {}

bool PageLoadTiming::operator==(const PageLoadTiming& other) const {
  return navigation_start == other.navigation_start &&
         response_start == other.response_start &&
         dom_loading == other.dom_loading &&
         dom_content_loaded_event_start ==
             other.dom_content_loaded_event_start &&
         load_event_start == other.load_event_start &&
         first_layout == other.first_layout &&
         first_paint == other.first_paint &&
         first_text_paint == other.first_text_paint &&
         first_image_paint == other.first_image_paint &&
         first_contentful_paint == other.first_contentful_paint &&
         parse_start == other.parse_start && parse_stop == other.parse_stop &&
         parse_blocked_on_script_load_duration ==
             other.parse_blocked_on_script_load_duration &&
         parse_blocked_on_script_load_from_document_write_duration ==
             other.parse_blocked_on_script_load_from_document_write_duration;
}

bool PageLoadTiming::IsEmpty() const {
  return navigation_start.is_null() && response_start.is_zero() &&
         dom_loading.is_zero() && dom_content_loaded_event_start.is_zero() &&
         load_event_start.is_zero() && first_layout.is_zero() &&
         first_paint.is_zero() && first_text_paint.is_zero() &&
         first_image_paint.is_zero() && first_contentful_paint.is_zero() &&
         parse_start.is_zero() && parse_stop.is_zero() &&
         parse_blocked_on_script_load_duration.is_zero() &&
         parse_blocked_on_script_load_from_document_write_duration.is_zero();
}

PageLoadMetadata::PageLoadMetadata() {}

bool PageLoadMetadata::operator==(const PageLoadMetadata& other) const {
  return behavior_flags == other.behavior_flags;
}

}  // namespace page_load_metrics
