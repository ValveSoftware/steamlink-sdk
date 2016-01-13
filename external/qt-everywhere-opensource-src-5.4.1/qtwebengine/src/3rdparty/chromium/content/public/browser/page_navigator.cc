// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/page_navigator.h"

namespace content {

OpenURLParams::OpenURLParams(
    const GURL& url,
    const Referrer& referrer,
    WindowOpenDisposition disposition,
    PageTransition transition,
    bool is_renderer_initiated)
    : url(url),
      referrer(referrer),
      uses_post(false),
      frame_tree_node_id(-1),
      disposition(disposition),
      transition(transition),
      is_renderer_initiated(is_renderer_initiated),
      should_replace_current_entry(false),
      user_gesture(!is_renderer_initiated) {
}

OpenURLParams::OpenURLParams(
    const GURL& url,
    const Referrer& referrer,
    int64 frame_tree_node_id,
    WindowOpenDisposition disposition,
    PageTransition transition,
    bool is_renderer_initiated)
    : url(url),
      referrer(referrer),
      uses_post(false),
      frame_tree_node_id(frame_tree_node_id),
      disposition(disposition),
      transition(transition),
      is_renderer_initiated(is_renderer_initiated),
      should_replace_current_entry(false),
      user_gesture(!is_renderer_initiated) {
}

OpenURLParams::OpenURLParams()
    : uses_post(false),
      frame_tree_node_id(-1),
      disposition(UNKNOWN),
      transition(PageTransitionFromInt(0)),
      is_renderer_initiated(false),
      should_replace_current_entry(false),
      user_gesture(true) {
}

OpenURLParams::~OpenURLParams() {
}

}  // namespace content
