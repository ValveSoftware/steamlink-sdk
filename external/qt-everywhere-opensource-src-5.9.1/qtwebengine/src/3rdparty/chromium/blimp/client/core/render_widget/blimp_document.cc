// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/render_widget/blimp_document.h"

#include "base/command_line.h"
#include "base/threading/thread_task_runner_handle.h"
#include "blimp/client/core/compositor/blimp_compositor_dependencies.h"
#include "blimp/client/core/input/blimp_input_manager.h"
#include "blimp/client/core/render_widget/blimp_document_manager.h"
#include "blimp/client/core/switches/blimp_client_switches.h"
#include "cc/layers/layer.h"

namespace blimp {
namespace client {

BlimpDocument::BlimpDocument(int document_id,
                             BlimpCompositorDependencies* compositor_deps,
                             BlimpDocumentManager* document_manager)
    : BlimpDocument(document_id,
                    BlimpCompositor::Create(compositor_deps, this),
                    compositor_deps,
                    document_manager) {}

BlimpDocument::BlimpDocument(int document_id,
                             std::unique_ptr<BlimpCompositor> compositor,
                             BlimpCompositorDependencies* compositor_deps,
                             BlimpDocumentManager* document_manager)
    : document_id_(document_id),
      compositor_(std::move(compositor)),
      manager_(document_manager) {
  input_manager_ =
      BlimpInputManager::Create(this, base::ThreadTaskRunnerHandle::Get(),
                                compositor_deps->GetCompositorTaskRunner(),
                                compositor_->GetInputHandler());
}

BlimpDocument::~BlimpDocument() {
  compositor_.reset();

  // Destroy the old input manager state.
  // It is important to destroy the LayerTreeHost before destroying the input
  // manager as it has a reference to the cc::InputHandlerClient owned by the
  // BlimpInputManager.
  input_manager_.reset();
}

BlimpCompositor* BlimpDocument::GetCompositor() {
  return compositor_.get();
}

bool BlimpDocument::OnTouchEvent(const ui::MotionEvent& motion_event) {
  if (input_manager_)
    return input_manager_->OnTouchEvent(motion_event);
  return false;
}

void BlimpDocument::SendCompositorMessage(
    const cc::proto::CompositorMessage& message) {
  manager_->SendCompositorMessage(document_id_, message);
}

void BlimpDocument::SendWebGestureEvent(
    const blink::WebGestureEvent& gesture_event) {
  manager_->SendWebGestureEvent(document_id_, gesture_event);
}

}  // namespace client
}  // namespace blimp
