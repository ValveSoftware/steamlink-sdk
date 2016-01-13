// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/document_state.h"

#include "content/public/renderer/navigation_state.h"

namespace content {

DocumentState::DocumentState()
    : load_histograms_recorded_(false),
      web_timing_histograms_recorded_(false),
      was_fetched_via_spdy_(false),
      was_npn_negotiated_(false),
      was_alternate_protocol_available_(false),
      connection_info_(net::HttpResponseInfo::CONNECTION_INFO_UNKNOWN),
      was_fetched_via_proxy_(false),
      was_prefetcher_(false),
      was_referred_by_prefetcher_(false),
      was_after_preconnect_request_(false),
      load_type_(UNDEFINED_LOAD),
      can_load_local_resources_(false) {
}

DocumentState::~DocumentState() {}

void DocumentState::set_navigation_state(NavigationState* navigation_state) {
  navigation_state_.reset(navigation_state);
}

}  // namespace content
