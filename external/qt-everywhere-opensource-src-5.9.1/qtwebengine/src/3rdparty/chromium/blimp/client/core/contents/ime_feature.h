// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_CONTENTS_IME_FEATURE_H_
#define BLIMP_CLIENT_CORE_CONTENTS_IME_FEATURE_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "blimp/net/blimp_message_processor.h"
#include "ui/base/ime/text_input_type.h"

namespace blimp {
namespace client {

// Handles all incoming and outgoing protobuf messages for text input of type
// BlimpMessage::IME for blimp client.
// Upon receiving a text input request from the engine, the ImeFeature
// delegates the request to the appropriate Delegate to show IME along with a
// Callback bound with respective tab id and render widget id.
// Any time user taps on an input text, ImeMessage::SHOW_IME message will be
// sent to client. Similarly, any time the text input is out of focus (e.g. if
// user navigates away from the currently page or the page loads for the first
// time), ImeMessage::HIDE_IME will be sent.

class ImeFeature : public BlimpMessageProcessor {
 public:
  struct WebInputResponse;

  // A callback to show IME.
  using ShowImeCallback = base::Callback<void(const WebInputResponse&)>;

  // A bundle of params required by the client.
  struct WebInputRequest {
    WebInputRequest();
    ~WebInputRequest();

    ui::TextInputType input_type;
    std::string text;
    ShowImeCallback show_ime_callback;
  };

  // A bundle of params to be sent to the engine.
  struct WebInputResponse {
    WebInputResponse();
    ~WebInputResponse() = default;

    std::string text;
    bool submit;
  };

  // A delegate to be notified of text input requests.
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void OnShowImeRequested(const WebInputRequest& request) = 0;
    virtual void OnHideImeRequested() = 0;
  };

  ImeFeature();
  ~ImeFeature() override;

  // Set the BlimpMessageProcessor that will be used to send BlimpMessage::IME
  // messages to the engine.
  void set_outgoing_message_processor(
      std::unique_ptr<BlimpMessageProcessor> processor) {
    outgoing_message_processor_ = std::move(processor);
  }

  // Sets a Delegate to be notified of all text input messages.
  // There must be a valid non-null Delegate set before routing messages
  // to the ImeFeature for processing, until the last message is processed.
  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  // BlimpMessageProcessor implementation.
  void ProcessMessage(std::unique_ptr<BlimpMessage> message,
                      const net::CompletionCallback& callback) override;

 private:
  // Sends text from IME to the blimp engine.
  void OnImeTextEntered(int tab_id,
                        int render_widget_id,
                        const WebInputResponse& response);

  // Used to actually show or hide the IME. See notes on |set_delegate|.
  Delegate* delegate_ = nullptr;

  // Used to send BlimpMessage::IME messages to the engine.
  std::unique_ptr<BlimpMessageProcessor> outgoing_message_processor_;

  base::WeakPtrFactory<ImeFeature> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ImeFeature);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_CONTENTS_IME_FEATURE_H_
