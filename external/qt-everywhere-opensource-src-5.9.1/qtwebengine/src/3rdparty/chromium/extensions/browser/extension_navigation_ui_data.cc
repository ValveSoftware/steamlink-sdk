// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_navigation_ui_data.h"

#include "content/public/browser/navigation_handle.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"

namespace extensions {

ExtensionNavigationUIData::ExtensionNavigationUIData() {}

ExtensionNavigationUIData::ExtensionNavigationUIData(
    content::NavigationHandle* navigation_handle,
    int tab_id,
    int window_id) {
  // TODO(clamy): See if it would be possible to have just one source for the
  // FrameData that works both for navigations and subresources loads.
  frame_data_.frame_id = ExtensionApiFrameIdMap::GetFrameId(navigation_handle);
  frame_data_.parent_frame_id =
      ExtensionApiFrameIdMap::GetParentFrameId(navigation_handle);
  frame_data_.tab_id = tab_id;
  frame_data_.window_id = window_id;

  WebViewGuest* web_view =
      WebViewGuest::FromWebContents(navigation_handle->GetWebContents());
  if (web_view) {
    is_web_view_ = true;
    web_view_instance_id_ = web_view->view_instance_id();
    web_view_rules_registry_id_ = web_view->rules_registry_id();
  } else {
    is_web_view_ = false;
    web_view_instance_id_ = web_view_rules_registry_id_ = 0;
  }
}

std::unique_ptr<ExtensionNavigationUIData> ExtensionNavigationUIData::DeepCopy()
    const {
  std::unique_ptr<ExtensionNavigationUIData> copy(
      new ExtensionNavigationUIData());
  copy->frame_data_ = frame_data_;
  copy->is_web_view_ = is_web_view_;
  copy->web_view_instance_id_ = web_view_instance_id_;
  copy->web_view_rules_registry_id_ = web_view_rules_registry_id_;
  return copy;
}

}  // namespace extensions
