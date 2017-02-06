// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/feature/ime_feature.h"

#include <string>

#include "blimp/common/create_blimp_message.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/net/input_message_converter.h"
#include "net/base/net_errors.h"

namespace blimp {
namespace client {

ImeFeature::ImeFeature() {}

ImeFeature::~ImeFeature() {}

void ImeFeature::OnImeTextEntered(const std::string& text) {
  DCHECK_LE(0, tab_id_);
  DCHECK_LT(0, render_widget_id_);

  ImeMessage* ime_message;
  std::unique_ptr<BlimpMessage> blimp_message =
      CreateBlimpMessage(&ime_message, tab_id_);
  ime_message->set_render_widget_id(render_widget_id_);
  ime_message->set_type(ImeMessage::SET_TEXT);
  ime_message->set_ime_text(text);

  outgoing_message_processor_->ProcessMessage(std::move(blimp_message),
                                              net::CompletionCallback());
}

void ImeFeature::ProcessMessage(std::unique_ptr<BlimpMessage> message,
                                const net::CompletionCallback& callback) {
  DCHECK(!callback.is_null());
  DCHECK_EQ(BlimpMessage::kIme, message->feature_case());

  DCHECK(delegate_);

  const ImeMessage& ime_message = message->ime();

  switch (ime_message.type()) {
    case ImeMessage::SHOW_IME:
      if (!message->has_target_tab_id() || message->target_tab_id() < 0 ||
          ime_message.render_widget_id() <= 0) {
        callback.Run(net::ERR_INVALID_ARGUMENT);
        return;
      }

      tab_id_ = message->target_tab_id();
      render_widget_id_ = ime_message.render_widget_id();

      delegate_->OnShowImeRequested(
          InputMessageConverter::TextInputTypeFromProto(
              ime_message.text_input_type()),
          ime_message.ime_text());
      break;
    case ImeMessage::HIDE_IME:
      tab_id_ = -1;
      render_widget_id_ = 0;
      delegate_->OnHideImeRequested();
      break;
    case ImeMessage::SET_TEXT:
    case ImeMessage::UNKNOWN:
      NOTREACHED();
      callback.Run(net::ERR_UNEXPECTED);
      return;
  }

  callback.Run(net::OK);
}

}  // namespace client
}  // namespace blimp
