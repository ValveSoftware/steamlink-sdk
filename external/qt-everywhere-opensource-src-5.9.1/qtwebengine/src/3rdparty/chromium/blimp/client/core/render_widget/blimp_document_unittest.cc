// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/render_widget/blimp_document.h"

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "blimp/client/core/compositor/blimp_compositor_dependencies.h"
#include "blimp/client/core/compositor/blob_image_serialization_processor.h"
#include "blimp/client/core/input/blimp_input_manager.h"
#include "blimp/client/core/render_widget/blimp_document_manager.h"
#include "blimp/client/core/render_widget/mock_render_widget_feature.h"
#include "blimp/client/test/compositor/mock_compositor_dependencies.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace blimp {
namespace client {
namespace {

const int kDefaultContentId = 1;
const int kDefaultDocumentId = 1;

class MockBlimpDocumentManager : public BlimpDocumentManager {
 public:
  explicit MockBlimpDocumentManager(
      RenderWidgetFeature* render_widget_feature,
      BlimpCompositorDependencies* compositor_dependencies)
      : BlimpDocumentManager(kDefaultContentId,
                             render_widget_feature,
                             compositor_dependencies) {}

  MOCK_METHOD2(SendWebGestureEvent,
               void(const int, const blink::WebGestureEvent&));
  MOCK_METHOD2(SendCompositorMessage,
               void(const int, const cc::proto::CompositorMessage&));
};

class BlimpDocumentTest : public testing::Test {
 public:
  void SetUp() override {
    compositor_dependencies_ = base::MakeUnique<BlimpCompositorDependencies>(
        base::MakeUnique<MockCompositorDependencies>());

    render_widget_feature_ = base::MakeUnique<MockRenderWidgetFeature>();
    document_manager_ = base::MakeUnique<MockBlimpDocumentManager>(
        render_widget_feature_.get(), compositor_dependencies_.get());
  }

  void TearDown() override {
    document_manager_.reset();
    compositor_dependencies_.reset();
    render_widget_feature_.reset();
  }

  base::MessageLoop loop_;
  BlobImageSerializationProcessor blob_image_serialization_processor_;
  std::unique_ptr<MockBlimpDocumentManager> document_manager_;
  std::unique_ptr<BlimpCompositorDependencies> compositor_dependencies_;
  std::unique_ptr<MockRenderWidgetFeature> render_widget_feature_;
};

TEST_F(BlimpDocumentTest, Constructor) {
  BlimpDocument doc(kDefaultDocumentId, compositor_dependencies_.get(),
                    document_manager_.get());
  EXPECT_EQ(doc.document_id(), kDefaultDocumentId);
  EXPECT_NE(doc.GetCompositor(), nullptr);
}

TEST_F(BlimpDocumentTest, ForwardMessagesToManager) {
  BlimpDocument doc(kDefaultDocumentId, compositor_dependencies_.get(),
                    document_manager_.get());
  EXPECT_CALL(*document_manager_, SendCompositorMessage(_, _)).Times(1);
  EXPECT_CALL(*document_manager_, SendWebGestureEvent(_, _)).Times(1);

  // When sending messages to the engine, ensure the messages are forwarded
  // to the document manager.
  BlimpCompositorClient* compositor_client =
      static_cast<BlimpCompositorClient*>(&doc);
  cc::proto::CompositorMessage fake_message;
  compositor_client->SendCompositorMessage(fake_message);

  BlimpInputManagerClient* input_manager_client =
      static_cast<BlimpInputManagerClient*>(&doc);
  blink::WebGestureEvent fake_gesture;
  input_manager_client->SendWebGestureEvent(fake_gesture);
}

}  // namespace
}  // namespace client
}  // namespace blimp
