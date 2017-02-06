// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/feature/tab_control_feature.h"

#include "blimp/common/create_blimp_message.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/common/proto/tab_control.pb.h"
#include "blimp/net/blimp_message_processor.h"
#include "net/base/net_errors.h"

namespace blimp {
namespace client {

TabControlFeature::TabControlFeature() {}

TabControlFeature::~TabControlFeature() {}

void TabControlFeature::set_outgoing_message_processor(
    std::unique_ptr<BlimpMessageProcessor> processor) {
  outgoing_message_processor_ = std::move(processor);
}

void TabControlFeature::SetSizeAndScale(const gfx::Size& size,
                                        float device_pixel_ratio) {
  if (last_size_ == size && last_device_pixel_ratio_ == device_pixel_ratio) {
    return;
  }

  last_size_ = size;
  last_device_pixel_ratio_ = device_pixel_ratio;

  SizeMessage* size_details;
  std::unique_ptr<BlimpMessage> message = CreateBlimpMessage(&size_details);
  size_details->set_width(size.width());
  size_details->set_height(size.height());
  size_details->set_device_pixel_ratio(device_pixel_ratio);

  // TODO(dtrainor): Don't keep sending size events to the server.  Wait for a
  // CompletionCallback to return before sending future size updates.
  outgoing_message_processor_->ProcessMessage(std::move(message),
                                              net::CompletionCallback());
}

void TabControlFeature::CreateTab(int tab_id) {
  TabControlMessage* tab_control;
  std::unique_ptr<BlimpMessage> message = CreateBlimpMessage(&tab_control);
  tab_control->mutable_create_tab();
  outgoing_message_processor_->ProcessMessage(std::move(message),
                                              net::CompletionCallback());
}

void TabControlFeature::CloseTab(int tab_id) {
  TabControlMessage* tab_control;
  std::unique_ptr<BlimpMessage> message = CreateBlimpMessage(&tab_control);
  tab_control->mutable_close_tab();
  outgoing_message_processor_->ProcessMessage(std::move(message),
                                              net::CompletionCallback());
}

void TabControlFeature::ProcessMessage(
    std::unique_ptr<BlimpMessage> message,
    const net::CompletionCallback& callback) {
  DCHECK(!callback.is_null());
  callback.Run(net::OK);

  NOTIMPLEMENTED();
}

}  // namespace client
}  // namespace blimp
