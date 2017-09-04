// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/render_widget/blimp_document_manager.h"

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "blimp/client/core/compositor/blimp_compositor_dependencies.h"
#include "blimp/client/core/compositor/blob_image_serialization_processor.h"
#include "blimp/client/core/render_widget/blimp_document.h"
#include "blimp/client/core/render_widget/mock_render_widget_feature.h"
#include "blimp/client/test/compositor/blimp_compositor_with_fake_host.h"
#include "blimp/client/test/compositor/mock_compositor_dependencies.h"
#include "cc/proto/compositor_message.pb.h"
#include "cc/surfaces/surface_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/gesture_detection/motion_event_generic.h"

using testing::_;
using testing::InSequence;
using testing::Sequence;

namespace blimp {
namespace client {
namespace {

const int kDummyBlimpContentsId = 0;

class MockBlimpDocument : public BlimpDocument {
 public:
  explicit MockBlimpDocument(const int document_id,
                             BlimpCompositorDependencies* compositor_deps,
                             BlimpDocumentManager* document_manager)
      : BlimpDocument(
            document_id,
            BlimpCompositorWithFakeHost::Create(compositor_deps, this),
            compositor_deps,
            document_manager) {}

  MOCK_METHOD1(SetVisible, void(bool));
  MOCK_METHOD1(OnTouchEvent, bool(const ui::MotionEvent&));
};

class BlimpDocumentManagerForTesting : public BlimpDocumentManager {
 public:
  BlimpDocumentManagerForTesting(
      int blimp_contents_id,
      RenderWidgetFeature* render_widget_feature,
      BlimpCompositorDependencies* compositor_dependencies)
      : BlimpDocumentManager(blimp_contents_id,
                             render_widget_feature,
                             compositor_dependencies) {}

  using BlimpDocumentManager::GetDocument;

  std::unique_ptr<BlimpDocument> CreateBlimpDocument(
      int document_id,
      BlimpCompositorDependencies* compositor_dependencies) override {
    return base::MakeUnique<MockBlimpDocument>(document_id,
                                               compositor_dependencies, this);
  }
};

class BlimpDocumentManagerTest : public testing::Test {
 public:
  void SetUp() override {
    EXPECT_CALL(render_widget_feature_, SetDelegate(_, _)).Times(1);
    EXPECT_CALL(render_widget_feature_, RemoveDelegate(_)).Times(1);

    compositor_dependencies_ = base::MakeUnique<BlimpCompositorDependencies>(
        base::MakeUnique<MockCompositorDependencies>());

    document_manager_ = base::MakeUnique<BlimpDocumentManagerForTesting>(
        kDummyBlimpContentsId, &render_widget_feature_,
        compositor_dependencies_.get());
  }

  void TearDown() override {
    document_manager_.reset();
    compositor_dependencies_.reset();
  }

  void SetUpDocuments() {
    delegate()->OnRenderWidgetCreated(1);
    delegate()->OnRenderWidgetCreated(2);

    mock_document1_ =
        static_cast<MockBlimpDocument*>(document_manager_->GetDocument(1));
    mock_document2_ =
        static_cast<MockBlimpDocument*>(document_manager_->GetDocument(2));

    EXPECT_EQ(mock_document1_->document_id(), 1);
    EXPECT_EQ(mock_document2_->document_id(), 2);
  }

  RenderWidgetFeature::RenderWidgetFeatureDelegate* delegate() const {
    DCHECK(document_manager_);
    return static_cast<RenderWidgetFeature::RenderWidgetFeatureDelegate*>(
        document_manager_.get());
  }

  base::MessageLoop loop_;
  std::unique_ptr<BlimpCompositorDependencies> compositor_dependencies_;
  std::unique_ptr<BlimpDocumentManagerForTesting> document_manager_;
  BlobImageSerializationProcessor blob_image_serialization_processor_;
  MockRenderWidgetFeature render_widget_feature_;
  MockBlimpDocument* mock_document1_;
  MockBlimpDocument* mock_document2_;
};

TEST_F(BlimpDocumentManagerTest, IncomingMessagesToCompositor) {
  SetUpDocuments();

  // When receiving a compositor message from the engine, ensure that the
  // messages are forwarded to the correct compositor.
  BlimpCompositorWithFakeHost* fake_compositor =
      static_cast<BlimpCompositorWithFakeHost*>(
          mock_document1_->GetCompositor());

  EXPECT_FALSE(
      mock_document1_->GetCompositor()->HasPendingFrameUpdateFromEngine());
  delegate()->OnCompositorMessageReceived(
      1, fake_compositor->CreateFakeUpdate(cc::Layer::Create()));
  EXPECT_TRUE(
      mock_document1_->GetCompositor()->HasPendingFrameUpdateFromEngine());

  EXPECT_FALSE(
      mock_document2_->GetCompositor()->HasPendingFrameUpdateFromEngine());
  delegate()->OnCompositorMessageReceived(
      2, fake_compositor->CreateFakeUpdate(cc::Layer::Create()));
  EXPECT_TRUE(
      mock_document2_->GetCompositor()->HasPendingFrameUpdateFromEngine());

  delegate()->OnRenderWidgetDeleted(1);
  EXPECT_EQ(document_manager_->GetDocument(1), nullptr);
}

TEST_F(BlimpDocumentManagerTest, OutgoingMessagesToMessageProcessor) {
  SetUpDocuments();

  // When sending message to the engine, ensure that the messages are forwarded
  // to the message processor.
  EXPECT_CALL(render_widget_feature_, SendCompositorMessage(_, _, _)).Times(1);
  EXPECT_CALL(render_widget_feature_, SendWebGestureEvent(_, _, _)).Times(1);

  cc::proto::CompositorMessage fake_message;
  document_manager_->SendCompositorMessage(1, fake_message);

  blink::WebGestureEvent fake_gesture;
  document_manager_->SendWebGestureEvent(1, fake_gesture);
}

TEST_F(BlimpDocumentManagerTest, ForwardsViewEventsToCorrectCompositor) {
  SetUpDocuments();

  // Make the document manager visible while we don't have any render widget
  // initialized.
  document_manager_->SetVisible(true);

  // Initialize the first render widget. This should propagate the visibility,
  // and the touch events to the corresponding document.
  delegate()->OnRenderWidgetInitialized(1);
  EXPECT_TRUE(mock_document1_->GetCompositor()->IsVisible());
  EXPECT_CALL(*mock_document1_, OnTouchEvent(_));
  document_manager_->OnTouchEvent(
      ui::MotionEventGeneric(ui::MotionEvent::Action::ACTION_NONE,
                             base::TimeTicks::Now(), ui::PointerProperties()));

  // Ensure the |document_manager_| holds the correct compositor layer.
  // And the root layer only has one child.
  EXPECT_EQ(document_manager_->layer(),
            mock_document1_->GetCompositor()->layer()->parent());
  EXPECT_EQ(static_cast<int>(document_manager_->layer()->children().size()), 1);

  // Now initialize the second render widget. This should swap the documents
  // and make the first one invisible.
  delegate()->OnRenderWidgetInitialized(2);
  EXPECT_FALSE(mock_document1_->GetCompositor()->IsVisible());
  EXPECT_TRUE(mock_document2_->GetCompositor()->IsVisible());

  // Ensure the content layer corresponds to the initialized document.
  EXPECT_EQ(document_manager_->layer(),
            mock_document2_->GetCompositor()->layer()->parent());
  EXPECT_EQ(static_cast<int>(document_manager_->layer()->children().size()), 1);

  // Destroy all the widgets.
  delegate()->OnRenderWidgetDeleted(1);
  delegate()->OnRenderWidgetDeleted(2);

  // The |document_manager_| should detach the compositor layer after active
  // document is destroyed.
  EXPECT_EQ(static_cast<int>(document_manager_->layer()->children().size()), 0);
}

}  // namespace
}  // namespace client
}  // namespace blimp
