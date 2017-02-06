// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/blimp_message_multiplexer.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "blimp/common/create_blimp_message.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/net/blimp_message_processor.h"

namespace blimp {
namespace {

class MultiplexedSender : public BlimpMessageProcessor {
 public:
  MultiplexedSender(base::WeakPtr<BlimpMessageProcessor> output_processor,
                    BlimpMessage::FeatureCase feature_case);
  ~MultiplexedSender() override;

  // BlimpMessageProcessor implementation.
  // |message.feature_case|, if set, must match the sender's |feature_case_|.
  void ProcessMessage(std::unique_ptr<BlimpMessage> message,
                      const net::CompletionCallback& callback) override;

 private:
  base::WeakPtr<BlimpMessageProcessor> output_processor_;
  BlimpMessage::FeatureCase feature_case_;

  DISALLOW_COPY_AND_ASSIGN(MultiplexedSender);
};

MultiplexedSender::MultiplexedSender(
    base::WeakPtr<BlimpMessageProcessor> output_processor,
    BlimpMessage::FeatureCase feature_case)
    : output_processor_(output_processor), feature_case_(feature_case) {}

MultiplexedSender::~MultiplexedSender() {}

void MultiplexedSender::ProcessMessage(
    std::unique_ptr<BlimpMessage> message,
    const net::CompletionCallback& callback) {
  DCHECK_EQ(feature_case_, message->feature_case());
  output_processor_->ProcessMessage(std::move(message), callback);
}

}  // namespace

BlimpMessageMultiplexer::BlimpMessageMultiplexer(
    BlimpMessageProcessor* output_processor)
    : output_weak_factory_(output_processor) {}

BlimpMessageMultiplexer::~BlimpMessageMultiplexer() {}

std::unique_ptr<BlimpMessageProcessor> BlimpMessageMultiplexer::CreateSender(
    BlimpMessage::FeatureCase feature_case) {
  return base::WrapUnique(
      new MultiplexedSender(output_weak_factory_.GetWeakPtr(), feature_case));
}
}  // namespace blimp
