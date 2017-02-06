// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/navigation_controller.h"

#include "base/memory/ref_counted_memory.h"
#include "build/build_config.h"

namespace content {

NavigationController::LoadURLParams::LoadURLParams(const GURL& url)
    : url(url),
      load_type(LOAD_TYPE_DEFAULT),
      transition_type(ui::PAGE_TRANSITION_LINK),
      frame_tree_node_id(-1),
      is_renderer_initiated(false),
      override_user_agent(UA_OVERRIDE_INHERIT),
      post_data(nullptr),
      can_load_local_resources(false),
      should_replace_current_entry(false),
#if defined(OS_ANDROID)
      intent_received_timestamp(0),
      has_user_gesture(false),
#endif
      should_clear_history_list(false) {
}

NavigationController::LoadURLParams::~LoadURLParams() {
}

NavigationController::LoadURLParams::LoadURLParams(
    const NavigationController::LoadURLParams& other)
    : url(other.url),
      load_type(other.load_type),
      transition_type(other.transition_type),
      frame_tree_node_id(other.frame_tree_node_id),
      referrer(other.referrer),
      extra_headers(other.extra_headers),
      is_renderer_initiated(other.is_renderer_initiated),
      override_user_agent(other.override_user_agent),
      transferred_global_request_id(other.transferred_global_request_id),
      base_url_for_data_url(other.base_url_for_data_url),
      virtual_url_for_data_url(other.virtual_url_for_data_url),
      post_data(other.post_data),
      should_replace_current_entry(false),
#if defined(OS_ANDROID)
      intent_received_timestamp(other.intent_received_timestamp),
      has_user_gesture(other.has_user_gesture),
#endif
      should_clear_history_list(false) {
}

NavigationController::LoadURLParams&
NavigationController::LoadURLParams::operator=(
    const NavigationController::LoadURLParams& other) {
  url = other.url;
  load_type = other.load_type;
  transition_type = other.transition_type;
  frame_tree_node_id = other.frame_tree_node_id;
  referrer = other.referrer;
  redirect_chain = other.redirect_chain;
  extra_headers = other.extra_headers;
  is_renderer_initiated = other.is_renderer_initiated;
  override_user_agent = other.override_user_agent;
  transferred_global_request_id = other.transferred_global_request_id;
  base_url_for_data_url = other.base_url_for_data_url;
  virtual_url_for_data_url = other.virtual_url_for_data_url;
  post_data = other.post_data;
  should_replace_current_entry = other.should_replace_current_entry;
  should_clear_history_list = other.should_clear_history_list;
#if defined(OS_ANDROID)
  intent_received_timestamp = other.intent_received_timestamp;
  has_user_gesture = other.has_user_gesture;
#endif

  return *this;
}

}  // namespace content
