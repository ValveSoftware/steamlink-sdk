// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_REQUEST_EXTRA_DATA_H_
#define CONTENT_CHILD_REQUEST_EXTRA_DATA_H_

#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "content/public/common/page_transition_types.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebPageVisibilityState.h"

namespace content {

// Can be used by callers to store extra data on every ResourceRequest
// which will be incorporated into the ResourceHostMsg_Request message
// sent by ResourceDispatcher.
class CONTENT_EXPORT RequestExtraData
    : public NON_EXPORTED_BASE(blink::WebURLRequest::ExtraData) {
 public:
  RequestExtraData();
  virtual ~RequestExtraData();

  blink::WebPageVisibilityState visibility_state() const {
    return visibility_state_;
  }
  void set_visibility_state(blink::WebPageVisibilityState visibility_state) {
    visibility_state_ = visibility_state;
  }
  int render_frame_id() const { return render_frame_id_; }
  void set_render_frame_id(int render_frame_id) {
    render_frame_id_ = render_frame_id;
  }
  bool is_main_frame() const { return is_main_frame_; }
  void set_is_main_frame(bool is_main_frame) {
    is_main_frame_ = is_main_frame;
  }
  GURL frame_origin() const { return frame_origin_; }
  void set_frame_origin(const GURL& frame_origin) {
    frame_origin_ = frame_origin;
  }
  bool parent_is_main_frame() const { return parent_is_main_frame_; }
  void set_parent_is_main_frame(bool parent_is_main_frame) {
    parent_is_main_frame_ = parent_is_main_frame;
  }
  int parent_render_frame_id() const { return parent_render_frame_id_; }
  void set_parent_render_frame_id(int parent_render_frame_id) {
    parent_render_frame_id_ = parent_render_frame_id;
  }
  bool allow_download() const { return allow_download_; }
  void set_allow_download(bool allow_download) {
    allow_download_ = allow_download;
  }
  PageTransition transition_type() const { return transition_type_; }
  void set_transition_type(PageTransition transition_type) {
    transition_type_ = transition_type;
  }
  bool should_replace_current_entry() const {
    return should_replace_current_entry_;
  }
  void set_should_replace_current_entry(
      bool should_replace_current_entry) {
    should_replace_current_entry_ = should_replace_current_entry;
  }
  int transferred_request_child_id() const {
    return transferred_request_child_id_;
  }
  void set_transferred_request_child_id(
      int transferred_request_child_id) {
    transferred_request_child_id_ = transferred_request_child_id;
  }
  int transferred_request_request_id() const {
    return transferred_request_request_id_;
  }
  void set_transferred_request_request_id(
      int transferred_request_request_id) {
    transferred_request_request_id_ = transferred_request_request_id;
  }
  int service_worker_provider_id() const {
    return service_worker_provider_id_;
  }
  void set_service_worker_provider_id(
      int service_worker_provider_id) {
    service_worker_provider_id_ = service_worker_provider_id;
  }
  // |custom_user_agent| is used to communicate an overriding custom user agent
  // to |RenderViewImpl::willSendRequest()|; set to a null string to indicate no
  // override and an empty string to indicate that there should be no user
  // agent.
  const blink::WebString& custom_user_agent() const {
      return custom_user_agent_;
  }
  void set_custom_user_agent(
      const blink::WebString& custom_user_agent) {
    custom_user_agent_ = custom_user_agent;
  }
  bool was_after_preconnect_request() { return was_after_preconnect_request_; }
  void set_was_after_preconnect_request(
      bool was_after_preconnect_request) {
    was_after_preconnect_request_ = was_after_preconnect_request;
  }

 private:
  blink::WebPageVisibilityState visibility_state_;
  int render_frame_id_;
  bool is_main_frame_;
  GURL frame_origin_;
  bool parent_is_main_frame_;
  int parent_render_frame_id_;
  bool allow_download_;
  PageTransition transition_type_;
  bool should_replace_current_entry_;
  int transferred_request_child_id_;
  int transferred_request_request_id_;
  int service_worker_provider_id_;
  blink::WebString custom_user_agent_;
  bool was_after_preconnect_request_;

  DISALLOW_COPY_AND_ASSIGN(RequestExtraData);
};

}  // namespace content

#endif  // CONTENT_CHILD_REQUEST_EXTRA_DATA_H_
