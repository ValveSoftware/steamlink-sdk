// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/resource_request_info_impl.h"

#include "content/browser/loader/global_routing_id.h"
#include "content/browser/loader/resource_message_filter.h"
#include "content/browser/worker_host/worker_service_impl.h"
#include "content/common/net/url_request_user_data.h"
#include "content/public/browser/global_request_id.h"
#include "net/url_request/url_request.h"

namespace content {

// ----------------------------------------------------------------------------
// ResourceRequestInfo

// static
const ResourceRequestInfo* ResourceRequestInfo::ForRequest(
    const net::URLRequest* request) {
  return ResourceRequestInfoImpl::ForRequest(request);
}

// static
void ResourceRequestInfo::AllocateForTesting(
    net::URLRequest* request,
    ResourceType::Type resource_type,
    ResourceContext* context,
    int render_process_id,
    int render_view_id,
    int render_frame_id,
    bool is_async) {
  ResourceRequestInfoImpl* info =
      new ResourceRequestInfoImpl(
          PROCESS_TYPE_RENDERER,             // process_type
          render_process_id,                 // child_id
          render_view_id,                    // route_id
          0,                                 // origin_pid
          0,                                 // request_id
          render_frame_id,                   // render_frame_id
          resource_type == ResourceType::MAIN_FRAME,  // is_main_frame
          false,                             // parent_is_main_frame
          0,                                 // parent_render_frame_id
          resource_type,                     // resource_type
          PAGE_TRANSITION_LINK,              // transition_type
          false,                             // should_replace_current_entry
          false,                             // is_download
          false,                             // is_stream
          true,                              // allow_download
          false,                             // has_user_gesture
          blink::WebReferrerPolicyDefault,   // referrer_policy
          blink::WebPageVisibilityStateVisible,  // visibility_state
          context,                           // context
          base::WeakPtr<ResourceMessageFilter>(),  // filter
          is_async);                         // is_async
  info->AssociateWithRequest(request);
}

// static
bool ResourceRequestInfo::GetRenderFrameForRequest(
    const net::URLRequest* request,
    int* render_process_id,
    int* render_frame_id) {
  URLRequestUserData* user_data = static_cast<URLRequestUserData*>(
      request->GetUserData(URLRequestUserData::kUserDataKey));
  if (!user_data)
    return false;
  *render_process_id = user_data->render_process_id();
  *render_frame_id = user_data->render_frame_id();
  return true;
}

// ----------------------------------------------------------------------------
// ResourceRequestInfoImpl

// static
ResourceRequestInfoImpl* ResourceRequestInfoImpl::ForRequest(
    net::URLRequest* request) {
  return static_cast<ResourceRequestInfoImpl*>(request->GetUserData(NULL));
}

// static
const ResourceRequestInfoImpl* ResourceRequestInfoImpl::ForRequest(
    const net::URLRequest* request) {
  return ForRequest(const_cast<net::URLRequest*>(request));
}

ResourceRequestInfoImpl::ResourceRequestInfoImpl(
    int process_type,
    int child_id,
    int route_id,
    int origin_pid,
    int request_id,
    int render_frame_id,
    bool is_main_frame,
    bool parent_is_main_frame,
    int parent_render_frame_id,
    ResourceType::Type resource_type,
    PageTransition transition_type,
    bool should_replace_current_entry,
    bool is_download,
    bool is_stream,
    bool allow_download,
    bool has_user_gesture,
    blink::WebReferrerPolicy referrer_policy,
    blink::WebPageVisibilityState visibility_state,
    ResourceContext* context,
    base::WeakPtr<ResourceMessageFilter> filter,
    bool is_async)
    : cross_site_handler_(NULL),
      detachable_handler_(NULL),
      process_type_(process_type),
      child_id_(child_id),
      route_id_(route_id),
      origin_pid_(origin_pid),
      request_id_(request_id),
      render_frame_id_(render_frame_id),
      is_main_frame_(is_main_frame),
      parent_is_main_frame_(parent_is_main_frame),
      parent_render_frame_id_(parent_render_frame_id),
      should_replace_current_entry_(should_replace_current_entry),
      is_download_(is_download),
      is_stream_(is_stream),
      allow_download_(allow_download),
      has_user_gesture_(has_user_gesture),
      was_ignored_by_handler_(false),
      resource_type_(resource_type),
      transition_type_(transition_type),
      memory_cost_(0),
      referrer_policy_(referrer_policy),
      visibility_state_(visibility_state),
      context_(context),
      filter_(filter),
      is_async_(is_async) {
}

ResourceRequestInfoImpl::~ResourceRequestInfoImpl() {
}

ResourceContext* ResourceRequestInfoImpl::GetContext() const {
  return context_;
}

int ResourceRequestInfoImpl::GetChildID() const {
  return child_id_;
}

int ResourceRequestInfoImpl::GetRouteID() const {
  return route_id_;
}

int ResourceRequestInfoImpl::GetOriginPID() const {
  return origin_pid_;
}

int ResourceRequestInfoImpl::GetRequestID() const {
  return request_id_;
}

int ResourceRequestInfoImpl::GetRenderFrameID() const {
  return render_frame_id_;
}

bool ResourceRequestInfoImpl::IsMainFrame() const {
  return is_main_frame_;
}

bool ResourceRequestInfoImpl::ParentIsMainFrame() const {
  return parent_is_main_frame_;
}

int ResourceRequestInfoImpl::GetParentRenderFrameID() const {
  return parent_render_frame_id_;
}

ResourceType::Type ResourceRequestInfoImpl::GetResourceType() const {
  return resource_type_;
}

int ResourceRequestInfoImpl::GetProcessType() const {
  return process_type_;
}

blink::WebReferrerPolicy ResourceRequestInfoImpl::GetReferrerPolicy() const {
  return referrer_policy_;
}

blink::WebPageVisibilityState
ResourceRequestInfoImpl::GetVisibilityState() const {
  return visibility_state_;
}

PageTransition ResourceRequestInfoImpl::GetPageTransition() const {
  return transition_type_;
}

bool ResourceRequestInfoImpl::HasUserGesture() const {
  return has_user_gesture_;
}

bool ResourceRequestInfoImpl::WasIgnoredByHandler() const {
  return was_ignored_by_handler_;
}

bool ResourceRequestInfoImpl::GetAssociatedRenderFrame(
    int* render_process_id,
    int* render_frame_id) const {
  // If the request is from the worker process, find a content that owns the
  // worker.
  if (process_type_ == PROCESS_TYPE_WORKER) {
    // Need to display some related UI for this network request - pick an
    // arbitrary parent to do so.
    if (!WorkerServiceImpl::GetInstance()->GetRendererForWorker(
            child_id_, render_process_id, render_frame_id)) {
      *render_process_id = -1;
      *render_frame_id = -1;
      return false;
    }
  } else if (process_type_ == PROCESS_TYPE_PLUGIN) {
    *render_process_id = origin_pid_;
    *render_frame_id = render_frame_id_;
  } else {
    *render_process_id = child_id_;
    *render_frame_id = render_frame_id_;
  }
  return true;
}

bool ResourceRequestInfoImpl::IsAsync() const {
  return is_async_;
}

bool ResourceRequestInfoImpl::IsDownload() const {
  return is_download_;
}

void ResourceRequestInfoImpl::AssociateWithRequest(net::URLRequest* request) {
  request->SetUserData(NULL, this);
  int render_process_id;
  int render_frame_id;
  if (GetAssociatedRenderFrame(&render_process_id, &render_frame_id)) {
    request->SetUserData(
        URLRequestUserData::kUserDataKey,
        new URLRequestUserData(render_process_id, render_frame_id));
  }
}

GlobalRequestID ResourceRequestInfoImpl::GetGlobalRequestID() const {
  return GlobalRequestID(child_id_, request_id_);
}

GlobalRoutingID ResourceRequestInfoImpl::GetGlobalRoutingID() const {
  return GlobalRoutingID(child_id_, route_id_);
}

void ResourceRequestInfoImpl::UpdateForTransfer(
    int child_id,
    int route_id,
    int origin_pid,
    int request_id,
    int parent_render_frame_id,
    base::WeakPtr<ResourceMessageFilter> filter) {
  child_id_ = child_id;
  route_id_ = route_id;
  origin_pid_ = origin_pid;
  request_id_ = request_id;
  parent_render_frame_id_ = parent_render_frame_id;
  filter_ = filter;
}

}  // namespace content
