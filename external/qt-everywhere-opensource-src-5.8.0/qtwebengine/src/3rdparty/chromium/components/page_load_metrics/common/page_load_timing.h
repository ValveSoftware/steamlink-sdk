// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_COMMON_PAGE_LOAD_TIMING_H_
#define COMPONENTS_PAGE_LOAD_METRICS_COMMON_PAGE_LOAD_TIMING_H_

#include "base/time/time.h"
#include "third_party/WebKit/public/platform/WebLoadingBehaviorFlag.h"

namespace page_load_metrics {

// PageLoadTiming contains timing metrics associated with a page load. Many of
// the metrics here are based on the Navigation Timing spec:
// http://www.w3.org/TR/navigation-timing/.
struct PageLoadTiming {
 public:
  PageLoadTiming();
  PageLoadTiming(const PageLoadTiming& other);
  ~PageLoadTiming();

  bool operator==(const PageLoadTiming& other) const;

  bool IsEmpty() const;

  // Time that the navigation for the associated page was initiated.
  base::Time navigation_start;

  // All TimeDeltas are relative to navigation_start

  // TODO(shivanisha): Issue 596367 shows that it is possible for a valid
  // TimeDelta value to be 0 (2 TimeTicks can have the same value even if they
  // were assigned in separate instructions if the clock speed is less
  // granular). The solution there was to use base::Optional for those values.
  // Consider changing the below values to Optional as well.

  // Time that the first byte of the response is received.
  base::TimeDelta response_start;

  // Time that the document transitions to the 'loading' state. This is roughly
  // the time that the HTML parser begins parsing the main HTML resource.
  base::TimeDelta dom_loading;

  // Time immediately before the DOMContentLoaded event is fired.
  base::TimeDelta dom_content_loaded_event_start;

  // Time immediately before the load event is fired.
  base::TimeDelta load_event_start;

  // Time when the first layout is completed.
  base::TimeDelta first_layout;

  // Time when the first paint is performed.
  base::TimeDelta first_paint;
  // Time when the first non-blank text is painted.
  base::TimeDelta first_text_paint;
  // Time when the first image is painted.
  base::TimeDelta first_image_paint;
  // Time when the first contentful thing (image, text, etc.) is painted.
  base::TimeDelta first_contentful_paint;

  // Time that the document's parser started and stopped parsing main resource
  // content.
  base::TimeDelta parse_start;
  base::TimeDelta parse_stop;

  // Sum of times when the parser is blocked waiting on the load of a script.
  // This duration takes place between parser_start and parser_stop, and thus
  // must be less than or equal to parser_stop - parser_start.
  base::TimeDelta parse_blocked_on_script_load_duration;

  // Sum of times when the parser is blocked waiting on the load of a script
  // that was inserted from document.write. This duration must be less than or
  // equal to parse_blocked_on_script_load_duration. Note that some uncommon
  // cases where scripts are loaded via document.write are not currently covered
  // by this field. See crbug/600711 for details.
  base::TimeDelta parse_blocked_on_script_load_from_document_write_duration;

  // If you add additional members, also be sure to update operator==,
  // page_load_metrics_messages.h, and IsEmpty().
};

struct PageLoadMetadata {
  PageLoadMetadata();
  bool operator==(const PageLoadMetadata& other) const;
  // These are packed blink::WebLoadingBehaviorFlag enums.
  int behavior_flags = blink::WebLoadingBehaviorNone;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_COMMON_PAGE_LOAD_TIMING_H_
