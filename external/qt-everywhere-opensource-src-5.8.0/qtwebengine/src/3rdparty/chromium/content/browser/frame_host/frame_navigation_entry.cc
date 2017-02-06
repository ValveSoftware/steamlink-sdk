// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/frame_navigation_entry.h"

#include <utility>

#include "content/common/page_state_serialization.h"
#include "content/common/site_isolation_policy.h"

namespace content {

FrameNavigationEntry::FrameNavigationEntry()
    : item_sequence_number_(-1), document_sequence_number_(-1), post_id_(-1) {}

FrameNavigationEntry::FrameNavigationEntry(
    const std::string& frame_unique_name,
    int64_t item_sequence_number,
    int64_t document_sequence_number,
    scoped_refptr<SiteInstanceImpl> site_instance,
    scoped_refptr<SiteInstanceImpl> source_site_instance,
    const GURL& url,
    const Referrer& referrer,
    const std::string& method,
    int64_t post_id)
    : frame_unique_name_(frame_unique_name),
      item_sequence_number_(item_sequence_number),
      document_sequence_number_(document_sequence_number),
      site_instance_(std::move(site_instance)),
      source_site_instance_(std::move(source_site_instance)),
      url_(url),
      referrer_(referrer),
      method_(method),
      post_id_(post_id) {}

FrameNavigationEntry::~FrameNavigationEntry() {
}

FrameNavigationEntry* FrameNavigationEntry::Clone() const {
  FrameNavigationEntry* copy = new FrameNavigationEntry();

  // Omit any fields cleared at commit time.
  copy->UpdateEntry(frame_unique_name_, item_sequence_number_,
                    document_sequence_number_, site_instance_.get(), nullptr,
                    url_, referrer_, page_state_, method_, post_id_);
  return copy;
}

void FrameNavigationEntry::UpdateEntry(
    const std::string& frame_unique_name,
    int64_t item_sequence_number,
    int64_t document_sequence_number,
    SiteInstanceImpl* site_instance,
    scoped_refptr<SiteInstanceImpl> source_site_instance,
    const GURL& url,
    const Referrer& referrer,
    const PageState& page_state,
    const std::string& method,
    int64_t post_id) {
  frame_unique_name_ = frame_unique_name;
  item_sequence_number_ = item_sequence_number;
  document_sequence_number_ = document_sequence_number;
  site_instance_ = site_instance;
  source_site_instance_ = std::move(source_site_instance);
  url_ = url;
  referrer_ = referrer;
  page_state_ = page_state;
  method_ = method;
  post_id_ = post_id;
}

void FrameNavigationEntry::set_item_sequence_number(
    int64_t item_sequence_number) {
  // TODO(creis): Assert that this does not change after being assigned, once
  // location.replace is classified as NEW_PAGE rather than EXISTING_PAGE.
  // Same for document sequence number.  See https://crbug.com/596707.
  item_sequence_number_ = item_sequence_number;
}

void FrameNavigationEntry::set_document_sequence_number(
    int64_t document_sequence_number) {
  document_sequence_number_ = document_sequence_number;
}

scoped_refptr<ResourceRequestBodyImpl> FrameNavigationEntry::GetPostData()
    const {
  DCHECK(SiteIsolationPolicy::UseSubframeNavigationEntries());
  if (method_ != "POST")
    return nullptr;

  // Generate the body from the PageState.
  ExplodedPageState exploded_state;
  if (!DecodePageState(page_state_.ToEncodedData(), &exploded_state))
    return nullptr;

  return exploded_state.top.http_body.request_body;
}

}  // namespace content
