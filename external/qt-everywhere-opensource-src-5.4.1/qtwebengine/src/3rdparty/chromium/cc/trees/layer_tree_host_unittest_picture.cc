// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host.h"

#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_picture_layer.h"
#include "cc/test/fake_picture_layer_impl.h"
#include "cc/test/layer_tree_test.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {
namespace {

// These tests deal with picture layers.
class LayerTreeHostPictureTest : public LayerTreeTest {
 protected:
  virtual void InitializeSettings(LayerTreeSettings* settings) OVERRIDE {
    // PictureLayer can only be used with impl side painting enabled.
    settings->impl_side_painting = true;
  }
};

class LayerTreeHostPictureTestTwinLayer
    : public LayerTreeHostPictureTest {
  virtual void SetupTree() OVERRIDE {
    LayerTreeHostPictureTest::SetupTree();

    scoped_refptr<FakePictureLayer> picture =
        FakePictureLayer::Create(&client_);
    layer_tree_host()->root_layer()->AddChild(picture);
  }

  virtual void BeginTest() OVERRIDE {
    activates_ = 0;
    PostSetNeedsCommitToMainThread();
  }

  virtual void DidCommit() OVERRIDE {
    switch (layer_tree_host()->source_frame_number()) {
      case 2:
        // Drop the picture layer from the tree.
        layer_tree_host()->root_layer()->children()[0]->RemoveFromParent();
        break;
      case 3:
        // Add a new picture layer.
        scoped_refptr<FakePictureLayer> picture =
            FakePictureLayer::Create(&client_);
        layer_tree_host()->root_layer()->AddChild(picture);
        break;
    }
  }

  virtual void WillActivateTreeOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    LayerImpl* pending_root_impl = impl->pending_tree()->root_layer();
    LayerImpl* active_root_impl = impl->active_tree()->root_layer();

    if (pending_root_impl->children().empty()) {
      EXPECT_EQ(2, activates_);
      return;
    }

    FakePictureLayerImpl* pending_picture_impl =
        static_cast<FakePictureLayerImpl*>(pending_root_impl->children()[0]);

    if (!active_root_impl) {
      EXPECT_EQ(0, activates_);
      EXPECT_EQ(NULL, pending_picture_impl->twin_layer());
      return;
    }

    if (active_root_impl->children().empty()) {
      EXPECT_EQ(3, activates_);
      EXPECT_EQ(NULL, pending_picture_impl->twin_layer());
      return;
    }

    FakePictureLayerImpl* active_picture_impl =
        static_cast<FakePictureLayerImpl*>(active_root_impl->children()[0]);

    // After the first activation, when we commit again, we'll have a pending
    // and active layer. Then we recreate a picture layer in the 4th activate
    // and the next commit will have a pending and active twin again.
    EXPECT_TRUE(activates_ == 1 || activates_ == 4);

    EXPECT_EQ(pending_picture_impl, active_picture_impl->twin_layer());
    EXPECT_EQ(active_picture_impl, pending_picture_impl->twin_layer());
  }

  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    LayerImpl* active_root_impl = impl->active_tree()->root_layer();

    if (active_root_impl->children().empty()) {
      EXPECT_EQ(2, activates_);
    } else {
      FakePictureLayerImpl* active_picture_impl =
          static_cast<FakePictureLayerImpl*>(active_root_impl->children()[0]);

      EXPECT_EQ(NULL, active_picture_impl->twin_layer());
    }

    ++activates_;
    if (activates_ <= 4)
      PostSetNeedsCommitToMainThread();
    else
      EndTest();
  }

  virtual void AfterTest() OVERRIDE {}

  FakeContentLayerClient client_;
  int activates_;
};

MULTI_THREAD_TEST_F(LayerTreeHostPictureTestTwinLayer);

}  // namespace
}  // namespace cc
