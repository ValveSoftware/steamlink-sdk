// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/feature/render_widget_feature.h"

#include "base/numerics/safe_conversions.h"
#include "blimp/common/create_blimp_message.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/common/proto/compositor.pb.h"
#include "blimp/common/proto/input.pb.h"
#include "blimp/common/proto/render_widget.pb.h"
#include "cc/proto/compositor_message.pb.h"
#include "net/base/net_errors.h"

namespace blimp {
namespace client {

RenderWidgetFeature::RenderWidgetFeature() {}

RenderWidgetFeature::~RenderWidgetFeature() {}

void RenderWidgetFeature::set_outgoing_input_message_processor(
    std::unique_ptr<BlimpMessageProcessor> processor) {
  outgoing_input_message_processor_ = std::move(processor);
}

void RenderWidgetFeature::set_outgoing_compositor_message_processor(
    std::unique_ptr<BlimpMessageProcessor> processor) {
  outgoing_compositor_message_processor_ = std::move(processor);
}

void RenderWidgetFeature::SendWebGestureEvent(
    const int tab_id,
    const int render_widget_id,
    const blink::WebGestureEvent& event) {
  std::unique_ptr<BlimpMessage> blimp_message =
      input_message_generator_.GenerateMessage(event);

  // Don't send unsupported WebGestureEvents.
  if (!blimp_message)
    return;

  blimp_message->set_target_tab_id(tab_id);
  blimp_message->mutable_input()->set_render_widget_id(render_widget_id);

  outgoing_input_message_processor_->ProcessMessage(std::move(blimp_message),
                                                    net::CompletionCallback());
}

void RenderWidgetFeature::SendCompositorMessage(
    const int tab_id,
    const int render_widget_id,
    const cc::proto::CompositorMessage& message) {
  CompositorMessage* compositor_message;
  std::unique_ptr<BlimpMessage> blimp_message =
      CreateBlimpMessage(&compositor_message, tab_id);

  compositor_message->set_render_widget_id(render_widget_id);
  compositor_message->mutable_payload()->resize(
      base::checked_cast<size_t>(message.ByteSize()));
  if (message.SerializeToString(compositor_message->mutable_payload())) {
    outgoing_compositor_message_processor_->ProcessMessage(
        std::move(blimp_message), net::CompletionCallback());
  } else {
    LOG(ERROR) << "Unable to serialize compositor proto.";
  }
}

void RenderWidgetFeature::SetDelegate(const int tab_id,
                                      RenderWidgetFeatureDelegate* delegate) {
  DCHECK(!FindDelegate(tab_id));
  delegates_[tab_id] = delegate;
}

void RenderWidgetFeature::RemoveDelegate(const int tab_id) {
  DelegateMap::iterator it = delegates_.find(tab_id);
  if (it != delegates_.end())
    delegates_.erase(it);
}

void RenderWidgetFeature::ProcessMessage(
    std::unique_ptr<BlimpMessage> message,
    const net::CompletionCallback& callback) {
  DCHECK(!callback.is_null());
  DCHECK(BlimpMessage::kRenderWidget == message->feature_case() ||
         BlimpMessage::kCompositor == message->feature_case());

  int target_tab_id = message->target_tab_id();
  RenderWidgetFeatureDelegate* delegate = FindDelegate(target_tab_id);
  DCHECK(delegate) << "RenderWidgetFeatureDelegate not found for "
      << target_tab_id;

  switch (message->feature_case()) {
    case BlimpMessage::kRenderWidget:
      ProcessRenderWidgetMessage(delegate, message->render_widget());
      break;
    case BlimpMessage::kCompositor:
      ProcessCompositorMessage(delegate, message->compositor());
      break;
    default:
      NOTREACHED();
      break;
  }

  callback.Run(net::OK);
}

void RenderWidgetFeature::ProcessRenderWidgetMessage(
    RenderWidgetFeatureDelegate* delegate,
    const RenderWidgetMessage& message) {
  int render_widget_id = message.render_widget_id();

  switch (message.type()) {
    case RenderWidgetMessage::CREATED:
      delegate->OnRenderWidgetCreated(render_widget_id);
      break;
    case RenderWidgetMessage::INITIALIZE:
      delegate->OnRenderWidgetInitialized(render_widget_id);
      break;
    case RenderWidgetMessage::DELETED:
      delegate->OnRenderWidgetDeleted(render_widget_id);
      break;
    case RenderWidgetMessage::UNKNOWN:
      NOTREACHED();
      break;
  }
}

void RenderWidgetFeature::ProcessCompositorMessage(
    RenderWidgetFeatureDelegate* delegate,
    const CompositorMessage& message) {
  int render_widget_id = message.render_widget_id();

  std::unique_ptr<cc::proto::CompositorMessage> payload(
      new cc::proto::CompositorMessage);
  if (payload->ParseFromString(message.payload())) {
    delegate->OnCompositorMessageReceived(render_widget_id,
                                          std::move(payload));
  }
}

RenderWidgetFeature::RenderWidgetFeatureDelegate*
RenderWidgetFeature::FindDelegate(const int tab_id) {
  DelegateMap::const_iterator it = delegates_.find(tab_id);
  if (it != delegates_.end())
    return it->second;
  return nullptr;
}

}  // namespace client
}  // namespace blimp
