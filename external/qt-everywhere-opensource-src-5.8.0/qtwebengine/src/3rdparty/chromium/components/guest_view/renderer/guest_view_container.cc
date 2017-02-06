// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_view/renderer/guest_view_container.h"

#include "base/macros.h"
#include "components/guest_view/common/guest_view_constants.h"
#include "components/guest_view/common/guest_view_messages.h"
#include "components/guest_view/renderer/guest_view_request.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_view.h"

namespace {

using GuestViewContainerMap = std::map<int, guest_view::GuestViewContainer*>;
static base::LazyInstance<GuestViewContainerMap> g_guest_view_container_map =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace guest_view {

class GuestViewContainer::RenderFrameLifetimeObserver
    : public content::RenderFrameObserver {
 public:
  RenderFrameLifetimeObserver(GuestViewContainer* container,
                              content::RenderFrame* render_frame);

  // content::RenderFrameObserver overrides.
  void OnDestruct() override;

 private:
  GuestViewContainer* container_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameLifetimeObserver);
};

GuestViewContainer::RenderFrameLifetimeObserver::RenderFrameLifetimeObserver(
    GuestViewContainer* container,
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      container_(container) {}

void GuestViewContainer::RenderFrameLifetimeObserver::OnDestruct() {
  container_->RenderFrameDestroyed();
}

GuestViewContainer::GuestViewContainer(content::RenderFrame* render_frame)
    : ready_(false),
      element_instance_id_(guest_view::kInstanceIDNone),
      render_frame_(render_frame),
      in_destruction_(false),
      destruction_isolate_(nullptr),
      weak_ptr_factory_(this) {
  render_frame_lifetime_observer_.reset(
      new RenderFrameLifetimeObserver(this, render_frame_));
}

GuestViewContainer::~GuestViewContainer() {
  // Note: Cleanups should be done in GuestViewContainer::Destroy(), not here.
}

// static.
GuestViewContainer* GuestViewContainer::FromID(int element_instance_id) {
  GuestViewContainerMap* guest_view_containers =
      g_guest_view_container_map.Pointer();
  auto it = guest_view_containers->find(element_instance_id);
  return it == guest_view_containers->end() ? nullptr : it->second;
}

// Right now a GuestViewContainer can be destroyed in one of the following
// ways:
//
// 1. If GuestViewContainer is driven by content/, the element (browser plugin)
//   can destroy GuestViewContainer when the element is destroyed.
// 2. If GuestViewContainer is managed outside of content/, then the
//   <webview> element's GC will destroy it.
// 3. If GuestViewContainer's embedder frame is destroyed, we'd also destroy
//   GuestViewContainer.
void GuestViewContainer::Destroy(bool embedder_frame_destroyed) {
  if (in_destruction_)
    return;

  in_destruction_ = true;

  // Give our derived class an opportunity to perform some cleanup prior to
  // destruction.
  OnDestroy(embedder_frame_destroyed);

  RunDestructionCallback(embedder_frame_destroyed);

  // Invalidate weak references to us to avoid late arriving tasks from running
  // during destruction
  weak_ptr_factory_.InvalidateWeakPtrs();

  if (element_instance_id() != guest_view::kInstanceIDNone)
    g_guest_view_container_map.Get().erase(element_instance_id());

  if (!embedder_frame_destroyed) {
    if (pending_response_.get())
      pending_response_->ExecuteCallbackIfAvailable(0 /* argc */, nullptr);

    while (pending_requests_.size() > 0) {
      linked_ptr<GuestViewRequest> pending_request = pending_requests_.front();
      pending_requests_.pop_front();
      // Call the JavaScript callbacks with no arguments which implies an error.
      pending_request->ExecuteCallbackIfAvailable(0 /* argc */, nullptr);
    }
  }

  delete this;
}

void GuestViewContainer::RegisterDestructionCallback(
    v8::Local<v8::Function> callback,
    v8::Isolate* isolate) {
  destruction_callback_.Reset(isolate, callback);
  destruction_isolate_ = isolate;
}

void GuestViewContainer::RenderFrameDestroyed() {
  OnRenderFrameDestroyed();
  render_frame_ = nullptr;
  Destroy(true /* embedder_frame_destroyed */);
}

void GuestViewContainer::IssueRequest(linked_ptr<GuestViewRequest> request) {
  EnqueueRequest(request);
  PerformPendingRequest();
}

void GuestViewContainer::EnqueueRequest(linked_ptr<GuestViewRequest> request) {
  pending_requests_.push_back(request);
}

void GuestViewContainer::PerformPendingRequest() {
  if (!ready_ || pending_requests_.empty() || pending_response_.get())
    return;

  linked_ptr<GuestViewRequest> pending_request = pending_requests_.front();
  pending_requests_.pop_front();
  pending_request->PerformRequest();
  pending_response_ = pending_request;
}

void GuestViewContainer::HandlePendingResponseCallback(
    const IPC::Message& message) {
  CHECK(pending_response_.get());
  linked_ptr<GuestViewRequest> pending_response(pending_response_.release());
  pending_response->HandleResponse(message);
}

void GuestViewContainer::RunDestructionCallback(bool embedder_frame_destroyed) {
  // Do not attempt to run |destruction_callback_| if the embedder frame was
  // destroyed. Trying to invoke callback on RenderFrame destruction results in
  // assertion failure when calling v8::MicrotasksScope.
  if (embedder_frame_destroyed)
    return;

  // Call the destruction callback, if one is registered.
  if (!destruction_callback_.IsEmpty()) {
    v8::HandleScope handle_scope(destruction_isolate_);
    v8::Local<v8::Function> callback = v8::Local<v8::Function>::New(
        destruction_isolate_, destruction_callback_);
    v8::Local<v8::Context> context = callback->CreationContext();
    if (context.IsEmpty())
      return;

    v8::Context::Scope context_scope(context);
    v8::MicrotasksScope microtasks(
        destruction_isolate_, v8::MicrotasksScope::kDoNotRunMicrotasks);

    callback->Call(context->Global(), 0 /* argc */, nullptr);
  }
}

void GuestViewContainer::OnHandleCallback(const IPC::Message& message) {
  // Handle the callback for the current request with a pending response.
  HandlePendingResponseCallback(message);
  // Perform the subsequent request if one exists.
  PerformPendingRequest();
}

bool GuestViewContainer::OnMessage(const IPC::Message& message) {
  return false;
}

bool GuestViewContainer::OnMessageReceived(const IPC::Message& message) {
  if (OnMessage(message))
    return true;

  OnHandleCallback(message);
  return true;
}

void GuestViewContainer::Ready() {
  ready_ = true;
  CHECK(!pending_response_.get());
  PerformPendingRequest();

  // Give the derived type an opportunity to perform some actions when the
  // container acquires a geometry.
  OnReady();
}

void GuestViewContainer::SetElementInstanceID(int element_instance_id) {
  DCHECK_EQ(element_instance_id_, guest_view::kInstanceIDNone);
  element_instance_id_ = element_instance_id;

  DCHECK(!g_guest_view_container_map.Get().count(element_instance_id));
  g_guest_view_container_map.Get().insert(
      std::make_pair(element_instance_id, this));
}

void GuestViewContainer::DidDestroyElement() {
  Destroy(false);
}

base::WeakPtr<content::BrowserPluginDelegate> GuestViewContainer::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace guest_view
