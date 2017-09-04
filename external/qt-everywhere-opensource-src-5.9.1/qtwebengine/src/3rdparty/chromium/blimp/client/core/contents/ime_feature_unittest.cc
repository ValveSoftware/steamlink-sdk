// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/contents/ime_feature.h"

#include <memory>

#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "blimp/client/core/contents/mock_ime_feature_delegate.h"
#include "blimp/common/create_blimp_message.h"
#include "blimp/net/input_message_converter.h"
#include "blimp/net/test_common.h"
#include "net/base/test_completion_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using ::testing::SaveArg;

namespace blimp {
namespace client {
namespace {

void SendMockImeMessage(BlimpMessageProcessor* processor,
                        int tab_id,
                        int render_widget_id,
                        ui::TextInputType input_type,
                        const std::string& existing_text,
                        ImeMessage::Type message_type) {
  ImeMessage* ime_message;
  std::unique_ptr<BlimpMessage> message =
      CreateBlimpMessage(&ime_message, tab_id);
  ime_message->set_render_widget_id(render_widget_id);
  ime_message->set_type(message_type);

  if (message_type == ImeMessage::SHOW_IME) {
    ime_message->set_text_input_type(
        InputMessageConverter::TextInputTypeToProto(input_type));
    ime_message->set_ime_text(existing_text);
  }

  net::TestCompletionCallback cb;
  processor->ProcessMessage(std::move(message), cb.callback());
  EXPECT_EQ(net::OK, cb.WaitForResult());
}

MATCHER_P4(EqualsOnImeTextEntered, tab_id, rwid, text, submit, "") {
  return arg.target_tab_id() == tab_id &&
         arg.ime().render_widget_id() == rwid && arg.ime().ime_text() == text &&
         arg.ime().auto_submit() == submit;
}

class ImeFeatureTest : public testing::Test {
 public:
  ImeFeatureTest() : out_processor_(nullptr) {}

  void SetUp() override {
    out_processor_ = new MockBlimpMessageProcessor();
    feature_.set_outgoing_message_processor(base::WrapUnique(out_processor_));
    feature_.set_delegate(&delegate_);
  }

 protected:
  // This is a raw pointer to a class that is owned by the ControlFeature.
  MockBlimpMessageProcessor* out_processor_;

  MockImeFeatureDelegate delegate_;

  ImeFeature feature_;
};

TEST_F(ImeFeatureTest, HideImeCalls) {
  MockImeFeatureDelegate delegate1;
  MockImeFeatureDelegate delegate2;

  EXPECT_CALL(delegate1, OnHideImeRequested());
  EXPECT_CALL(delegate2, OnHideImeRequested());

  feature_.set_delegate(&delegate1);
  SendMockImeMessage(&feature_, 0, 1, ui::TEXT_INPUT_TYPE_NONE, "",
                     ImeMessage::HIDE_IME);
  feature_.set_delegate(&delegate2);
  SendMockImeMessage(&feature_, 0, 1, ui::TEXT_INPUT_TYPE_NONE, "",
                     ImeMessage::HIDE_IME);
}

TEST_F(ImeFeatureTest, RepeatedShowAndHideImeCalls) {
  ImeFeature::WebInputRequest request;

  EXPECT_CALL(delegate_, OnHideImeRequested());
  EXPECT_CALL(delegate_, OnShowImeRequested(_)).WillOnce(SaveArg<0>(&request));

  SendMockImeMessage(&feature_, 0, 1, ui::TEXT_INPUT_TYPE_TEXT, "some text",
                     ImeMessage::SHOW_IME);
  SendMockImeMessage(&feature_, 0, 1, ui::TEXT_INPUT_TYPE_NONE, "",
                     ImeMessage::HIDE_IME);
  ASSERT_EQ("some text", request.text);
  ASSERT_EQ(ui::TEXT_INPUT_TYPE_TEXT, request.input_type);

  EXPECT_CALL(delegate_, OnShowImeRequested(_)).WillOnce(SaveArg<0>(&request));

  SendMockImeMessage(&feature_, 0, 1, ui::TEXT_INPUT_TYPE_TEXT_AREA,
                     "modified text", ImeMessage::SHOW_IME);
  ASSERT_EQ("modified text", request.text);
  ASSERT_EQ(ui::TEXT_INPUT_TYPE_TEXT_AREA, request.input_type);
}

TEST_F(ImeFeatureTest, CallbackHasFieldsPopulatedCorrectly) {
  ImeFeature::WebInputRequest request;
  ImeFeature::WebInputResponse response;

  EXPECT_CALL(delegate_, OnShowImeRequested(_)).WillOnce(SaveArg<0>(&request));
  SendMockImeMessage(&feature_, 1, 3, ui::TEXT_INPUT_TYPE_TEXT, "old test",
                     ImeMessage::SHOW_IME);

  EXPECT_CALL(*out_processor_,
              MockableProcessMessage(
                  EqualsOnImeTextEntered(1, 3, "new text", true), _));
  response.text = "new text";
  response.submit = true;
  base::ResetAndReturn(&request.show_ime_callback).Run(response);
}

}  // namespace
}  // namespace client
}  // namespace blimp
