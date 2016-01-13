// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "cc/test/fake_content_layer.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/layer_tree_test.h"
#include "cc/trees/layer_tree_impl.h"
#include "gpu/GLES2/gl2extchromium.h"

namespace cc {
namespace {

// These tests only use direct rendering, as there is no output to copy for
// delegated renderers.
class LayerTreeHostCopyRequestTest : public LayerTreeTest {};

class LayerTreeHostCopyRequestTestMultipleRequests
    : public LayerTreeHostCopyRequestTest {
 protected:
  virtual void SetupTree() OVERRIDE {
    root = FakeContentLayer::Create(&client_);
    root->SetBounds(gfx::Size(20, 20));

    child = FakeContentLayer::Create(&client_);
    child->SetBounds(gfx::Size(10, 10));
    root->AddChild(child);

    layer_tree_host()->SetRootLayer(root);
    LayerTreeHostCopyRequestTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  virtual void DidCommitAndDrawFrame() OVERRIDE { WaitForCallback(); }

  void WaitForCallback() {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&LayerTreeHostCopyRequestTestMultipleRequests::NextStep,
                   base::Unretained(this)));
  }

  void NextStep() {
    int frame = layer_tree_host()->source_frame_number();
    switch (frame) {
      case 1:
        child->RequestCopyOfOutput(CopyOutputRequest::CreateBitmapRequest(
            base::Bind(&LayerTreeHostCopyRequestTestMultipleRequests::
                            CopyOutputCallback,
                       base::Unretained(this))));
        EXPECT_EQ(0u, callbacks_.size());
        break;
      case 2:
        if (callbacks_.size() < 1u) {
          WaitForCallback();
          return;
        }
        EXPECT_EQ(1u, callbacks_.size());
        EXPECT_EQ(gfx::Size(10, 10).ToString(), callbacks_[0].ToString());

        child->RequestCopyOfOutput(CopyOutputRequest::CreateBitmapRequest(
            base::Bind(&LayerTreeHostCopyRequestTestMultipleRequests::
                            CopyOutputCallback,
                       base::Unretained(this))));
        root->RequestCopyOfOutput(CopyOutputRequest::CreateBitmapRequest(
            base::Bind(&LayerTreeHostCopyRequestTestMultipleRequests::
                            CopyOutputCallback,
                       base::Unretained(this))));
        child->RequestCopyOfOutput(CopyOutputRequest::CreateBitmapRequest(
            base::Bind(&LayerTreeHostCopyRequestTestMultipleRequests::
                            CopyOutputCallback,
                       base::Unretained(this))));
        EXPECT_EQ(1u, callbacks_.size());
        break;
      case 3:
        if (callbacks_.size() < 4u) {
          WaitForCallback();
          return;
        }
        EXPECT_EQ(4u, callbacks_.size());
        // The child was copied to a bitmap and passed back twice.
        EXPECT_EQ(gfx::Size(10, 10).ToString(), callbacks_[1].ToString());
        EXPECT_EQ(gfx::Size(10, 10).ToString(), callbacks_[2].ToString());
        // The root was copied to a bitmap and passed back also.
        EXPECT_EQ(gfx::Size(20, 20).ToString(), callbacks_[3].ToString());
        EndTest();
        break;
    }
  }

  void CopyOutputCallback(scoped_ptr<CopyOutputResult> result) {
    EXPECT_TRUE(layer_tree_host()->proxy()->IsMainThread());
    EXPECT_TRUE(result->HasBitmap());
    scoped_ptr<SkBitmap> bitmap = result->TakeBitmap().Pass();
    EXPECT_EQ(result->size().ToString(),
              gfx::Size(bitmap->width(), bitmap->height()).ToString());
    callbacks_.push_back(result->size());
  }

  virtual void AfterTest() OVERRIDE { EXPECT_EQ(4u, callbacks_.size()); }

  virtual scoped_ptr<FakeOutputSurface> CreateFakeOutputSurface(bool fallback)
      OVERRIDE {
    if (use_gl_renderer_)
      return FakeOutputSurface::Create3d();
    return FakeOutputSurface::CreateSoftware(
        make_scoped_ptr(new SoftwareOutputDevice));
  }

  bool use_gl_renderer_;
  std::vector<gfx::Size> callbacks_;
  FakeContentLayerClient client_;
  scoped_refptr<FakeContentLayer> root;
  scoped_refptr<FakeContentLayer> child;
};

// Readback can't be done with a delegating renderer.
TEST_F(LayerTreeHostCopyRequestTestMultipleRequests,
       GLRenderer_RunSingleThread) {
  use_gl_renderer_ = true;
  RunTest(false, false, false);
}

TEST_F(LayerTreeHostCopyRequestTestMultipleRequests,
       GLRenderer_RunMultiThread_MainThreadPainting) {
  use_gl_renderer_ = true;
  RunTest(true, false, false);
}

TEST_F(LayerTreeHostCopyRequestTestMultipleRequests,
       SoftwareRenderer_RunSingleThread) {
  use_gl_renderer_ = false;
  RunTest(false, false, false);
}

TEST_F(LayerTreeHostCopyRequestTestMultipleRequests,
       SoftwareRenderer_RunMultiThread_MainThreadPainting) {
  use_gl_renderer_ = false;
  RunTest(true, false, false);
}

class LayerTreeHostCopyRequestTestLayerDestroyed
    : public LayerTreeHostCopyRequestTest {
 protected:
  virtual void SetupTree() OVERRIDE {
    root_ = FakeContentLayer::Create(&client_);
    root_->SetBounds(gfx::Size(20, 20));

    main_destroyed_ = FakeContentLayer::Create(&client_);
    main_destroyed_->SetBounds(gfx::Size(15, 15));
    root_->AddChild(main_destroyed_);

    impl_destroyed_ = FakeContentLayer::Create(&client_);
    impl_destroyed_->SetBounds(gfx::Size(10, 10));
    root_->AddChild(impl_destroyed_);

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostCopyRequestTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE {
    callback_count_ = 0;
    PostSetNeedsCommitToMainThread();
  }

  virtual void DidCommit() OVERRIDE {
    int frame = layer_tree_host()->source_frame_number();
    switch (frame) {
      case 1:
        main_destroyed_->RequestCopyOfOutput(
            CopyOutputRequest::CreateBitmapRequest(base::Bind(
                &LayerTreeHostCopyRequestTestLayerDestroyed::CopyOutputCallback,
                base::Unretained(this))));
        impl_destroyed_->RequestCopyOfOutput(
            CopyOutputRequest::CreateBitmapRequest(base::Bind(
                &LayerTreeHostCopyRequestTestLayerDestroyed::CopyOutputCallback,
                base::Unretained(this))));
        EXPECT_EQ(0, callback_count_);

        // Destroy the main thread layer right away.
        main_destroyed_->RemoveFromParent();
        main_destroyed_ = NULL;

        // Should callback with a NULL bitmap.
        EXPECT_EQ(1, callback_count_);

        // Prevent drawing so we can't make a copy of the impl_destroyed layer.
        layer_tree_host()->SetViewportSize(gfx::Size());
        break;
      case 2:
        // Flush the message loops and make sure the callbacks run.
        layer_tree_host()->SetNeedsCommit();
        break;
      case 3:
        // No drawing means no readback yet.
        EXPECT_EQ(1, callback_count_);

        // Destroy the impl thread layer.
        impl_destroyed_->RemoveFromParent();
        impl_destroyed_ = NULL;

        // No callback yet because it's on the impl side.
        EXPECT_EQ(1, callback_count_);
        break;
      case 4:
        // Flush the message loops and make sure the callbacks run.
        layer_tree_host()->SetNeedsCommit();
        break;
      case 5:
        // We should get another callback with a NULL bitmap.
        EXPECT_EQ(2, callback_count_);
        EndTest();
        break;
    }
  }

  void CopyOutputCallback(scoped_ptr<CopyOutputResult> result) {
    EXPECT_TRUE(layer_tree_host()->proxy()->IsMainThread());
    EXPECT_TRUE(result->IsEmpty());
    ++callback_count_;
  }

  virtual void AfterTest() OVERRIDE {}

  int callback_count_;
  FakeContentLayerClient client_;
  scoped_refptr<FakeContentLayer> root_;
  scoped_refptr<FakeContentLayer> main_destroyed_;
  scoped_refptr<FakeContentLayer> impl_destroyed_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostCopyRequestTestLayerDestroyed);

class LayerTreeHostCopyRequestTestInHiddenSubtree
    : public LayerTreeHostCopyRequestTest {
 protected:
  virtual void SetupTree() OVERRIDE {
    root_ = FakeContentLayer::Create(&client_);
    root_->SetBounds(gfx::Size(20, 20));

    grand_parent_layer_ = FakeContentLayer::Create(&client_);
    grand_parent_layer_->SetBounds(gfx::Size(15, 15));
    root_->AddChild(grand_parent_layer_);

    // parent_layer_ owns a render surface.
    parent_layer_ = FakeContentLayer::Create(&client_);
    parent_layer_->SetBounds(gfx::Size(15, 15));
    parent_layer_->SetForceRenderSurface(true);
    grand_parent_layer_->AddChild(parent_layer_);

    copy_layer_ = FakeContentLayer::Create(&client_);
    copy_layer_->SetBounds(gfx::Size(10, 10));
    parent_layer_->AddChild(copy_layer_);

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostCopyRequestTest::SetupTree();
  }

  void AddCopyRequest(Layer* layer) {
    layer->RequestCopyOfOutput(
        CopyOutputRequest::CreateBitmapRequest(base::Bind(
            &LayerTreeHostCopyRequestTestInHiddenSubtree::CopyOutputCallback,
            base::Unretained(this))));
  }

  virtual void BeginTest() OVERRIDE {
    callback_count_ = 0;
    PostSetNeedsCommitToMainThread();

    AddCopyRequest(copy_layer_.get());
  }

  void CopyOutputCallback(scoped_ptr<CopyOutputResult> result) {
    ++callback_count_;
    EXPECT_TRUE(layer_tree_host()->proxy()->IsMainThread());
    EXPECT_EQ(copy_layer_->bounds().ToString(), result->size().ToString())
        << callback_count_;
    switch (callback_count_) {
      case 1:
        // Hide the copy request layer.
        grand_parent_layer_->SetHideLayerAndSubtree(false);
        parent_layer_->SetHideLayerAndSubtree(false);
        copy_layer_->SetHideLayerAndSubtree(true);
        AddCopyRequest(copy_layer_.get());
        break;
      case 2:
        // Hide the copy request layer's parent only.
        grand_parent_layer_->SetHideLayerAndSubtree(false);
        parent_layer_->SetHideLayerAndSubtree(true);
        copy_layer_->SetHideLayerAndSubtree(false);
        AddCopyRequest(copy_layer_.get());
        break;
      case 3:
        // Hide the copy request layer's grand parent only.
        grand_parent_layer_->SetHideLayerAndSubtree(true);
        parent_layer_->SetHideLayerAndSubtree(false);
        copy_layer_->SetHideLayerAndSubtree(false);
        AddCopyRequest(copy_layer_.get());
        break;
      case 4:
        // Hide the copy request layer's parent and grandparent.
        grand_parent_layer_->SetHideLayerAndSubtree(true);
        parent_layer_->SetHideLayerAndSubtree(true);
        copy_layer_->SetHideLayerAndSubtree(false);
        AddCopyRequest(copy_layer_.get());
        break;
      case 5:
        // Hide the copy request layer as well as its parent and grandparent.
        grand_parent_layer_->SetHideLayerAndSubtree(true);
        parent_layer_->SetHideLayerAndSubtree(true);
        copy_layer_->SetHideLayerAndSubtree(true);
        AddCopyRequest(copy_layer_.get());
        break;
      case 6:
        EndTest();
        break;
    }
  }

  virtual void AfterTest() OVERRIDE {}

  int callback_count_;
  FakeContentLayerClient client_;
  scoped_refptr<FakeContentLayer> root_;
  scoped_refptr<FakeContentLayer> grand_parent_layer_;
  scoped_refptr<FakeContentLayer> parent_layer_;
  scoped_refptr<FakeContentLayer> copy_layer_;
};

SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_NOIMPL_TEST_F(
    LayerTreeHostCopyRequestTestInHiddenSubtree);

class LayerTreeHostTestHiddenSurfaceNotAllocatedForSubtreeCopyRequest
    : public LayerTreeHostCopyRequestTest {
 protected:
  virtual void SetupTree() OVERRIDE {
    root_ = FakeContentLayer::Create(&client_);
    root_->SetBounds(gfx::Size(20, 20));

    grand_parent_layer_ = FakeContentLayer::Create(&client_);
    grand_parent_layer_->SetBounds(gfx::Size(15, 15));
    grand_parent_layer_->SetHideLayerAndSubtree(true);
    root_->AddChild(grand_parent_layer_);

    // parent_layer_ owns a render surface.
    parent_layer_ = FakeContentLayer::Create(&client_);
    parent_layer_->SetBounds(gfx::Size(15, 15));
    parent_layer_->SetForceRenderSurface(true);
    grand_parent_layer_->AddChild(parent_layer_);

    copy_layer_ = FakeContentLayer::Create(&client_);
    copy_layer_->SetBounds(gfx::Size(10, 10));
    parent_layer_->AddChild(copy_layer_);

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostCopyRequestTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE {
    did_draw_ = false;
    PostSetNeedsCommitToMainThread();

    copy_layer_->RequestCopyOfOutput(
        CopyOutputRequest::CreateBitmapRequest(base::Bind(
            &LayerTreeHostTestHiddenSurfaceNotAllocatedForSubtreeCopyRequest::
                 CopyOutputCallback,
            base::Unretained(this))));
  }

  void CopyOutputCallback(scoped_ptr<CopyOutputResult> result) {
    EXPECT_TRUE(layer_tree_host()->proxy()->IsMainThread());
    EXPECT_EQ(copy_layer_->bounds().ToString(), result->size().ToString());
    EndTest();
  }

  virtual void DrawLayersOnThread(LayerTreeHostImpl* host_impl) OVERRIDE {
    Renderer* renderer = host_impl->renderer();

    LayerImpl* root = host_impl->active_tree()->root_layer();
    LayerImpl* grand_parent = root->children()[0];
    LayerImpl* parent = grand_parent->children()[0];
    LayerImpl* copy_layer = parent->children()[0];

    // |parent| owns a surface, but it was hidden and not part of the copy
    // request so it should not allocate any resource.
    EXPECT_FALSE(renderer->HasAllocatedResourcesForTesting(
        parent->render_surface()->RenderPassId()));

    // |copy_layer| should have been rendered to a texture since it was needed
    // for a copy request.
    EXPECT_TRUE(renderer->HasAllocatedResourcesForTesting(
        copy_layer->render_surface()->RenderPassId()));

    did_draw_ = true;
  }

  virtual void AfterTest() OVERRIDE { EXPECT_TRUE(did_draw_); }

  FakeContentLayerClient client_;
  bool did_draw_;
  scoped_refptr<FakeContentLayer> root_;
  scoped_refptr<FakeContentLayer> grand_parent_layer_;
  scoped_refptr<FakeContentLayer> parent_layer_;
  scoped_refptr<FakeContentLayer> copy_layer_;
};

// No output to copy for delegated renderers.
SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_TEST_F(
    LayerTreeHostTestHiddenSurfaceNotAllocatedForSubtreeCopyRequest);

class LayerTreeHostCopyRequestTestClippedOut
    : public LayerTreeHostCopyRequestTest {
 protected:
  virtual void SetupTree() OVERRIDE {
    root_ = FakeContentLayer::Create(&client_);
    root_->SetBounds(gfx::Size(20, 20));

    parent_layer_ = FakeContentLayer::Create(&client_);
    parent_layer_->SetBounds(gfx::Size(15, 15));
    parent_layer_->SetMasksToBounds(true);
    root_->AddChild(parent_layer_);

    copy_layer_ = FakeContentLayer::Create(&client_);
    copy_layer_->SetPosition(gfx::Point(15, 15));
    copy_layer_->SetBounds(gfx::Size(10, 10));
    parent_layer_->AddChild(copy_layer_);

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostCopyRequestTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE {
    PostSetNeedsCommitToMainThread();

    copy_layer_->RequestCopyOfOutput(CopyOutputRequest::CreateBitmapRequest(
        base::Bind(&LayerTreeHostCopyRequestTestClippedOut::CopyOutputCallback,
                   base::Unretained(this))));
  }

  void CopyOutputCallback(scoped_ptr<CopyOutputResult> result) {
    // We should still get a callback with no output if the copy requested layer
    // was completely clipped away.
    EXPECT_TRUE(layer_tree_host()->proxy()->IsMainThread());
    EXPECT_EQ(gfx::Size().ToString(), result->size().ToString());
    EndTest();
  }

  virtual void AfterTest() OVERRIDE {}

  FakeContentLayerClient client_;
  scoped_refptr<FakeContentLayer> root_;
  scoped_refptr<FakeContentLayer> parent_layer_;
  scoped_refptr<FakeContentLayer> copy_layer_;
};

SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_TEST_F(
    LayerTreeHostCopyRequestTestClippedOut);

class LayerTreeHostTestAsyncTwoReadbacksWithoutDraw
    : public LayerTreeHostCopyRequestTest {
 protected:
  virtual void SetupTree() OVERRIDE {
    root_ = FakeContentLayer::Create(&client_);
    root_->SetBounds(gfx::Size(20, 20));

    copy_layer_ = FakeContentLayer::Create(&client_);
    copy_layer_->SetBounds(gfx::Size(10, 10));
    root_->AddChild(copy_layer_);

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostCopyRequestTest::SetupTree();
  }

  void AddCopyRequest(Layer* layer) {
    layer->RequestCopyOfOutput(
        CopyOutputRequest::CreateBitmapRequest(base::Bind(
            &LayerTreeHostTestAsyncTwoReadbacksWithoutDraw::CopyOutputCallback,
            base::Unretained(this))));
  }

  virtual void BeginTest() OVERRIDE {
    saw_copy_request_ = false;
    callback_count_ = 0;
    PostSetNeedsCommitToMainThread();

    // Prevent drawing.
    layer_tree_host()->SetViewportSize(gfx::Size(0, 0));

    AddCopyRequest(copy_layer_.get());
  }

  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    if (impl->active_tree()->source_frame_number() == 0) {
      LayerImpl* root = impl->active_tree()->root_layer();
      EXPECT_TRUE(root->children()[0]->HasCopyRequest());
      saw_copy_request_ = true;
    }
  }

  virtual void DidCommit() OVERRIDE {
    if (layer_tree_host()->source_frame_number() == 1) {
      // Allow drawing.
      layer_tree_host()->SetViewportSize(gfx::Size(root_->bounds()));

      AddCopyRequest(copy_layer_.get());
    }
  }

  void CopyOutputCallback(scoped_ptr<CopyOutputResult> result) {
    EXPECT_TRUE(layer_tree_host()->proxy()->IsMainThread());
    EXPECT_EQ(copy_layer_->bounds().ToString(), result->size().ToString());
    ++callback_count_;

    if (callback_count_ == 2)
      EndTest();
  }

  virtual void AfterTest() OVERRIDE { EXPECT_TRUE(saw_copy_request_); }

  bool saw_copy_request_;
  int callback_count_;
  FakeContentLayerClient client_;
  scoped_refptr<FakeContentLayer> root_;
  scoped_refptr<FakeContentLayer> copy_layer_;
};

SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_NOIMPL_TEST_F(
    LayerTreeHostTestAsyncTwoReadbacksWithoutDraw);

class LayerTreeHostCopyRequestTestLostOutputSurface
    : public LayerTreeHostCopyRequestTest {
 protected:
  virtual scoped_ptr<FakeOutputSurface> CreateFakeOutputSurface(bool fallback)
      OVERRIDE {
    if (!first_context_provider_.get()) {
      first_context_provider_ = TestContextProvider::Create();
      return FakeOutputSurface::Create3d(first_context_provider_);
    }

    EXPECT_FALSE(second_context_provider_.get());
    second_context_provider_ = TestContextProvider::Create();
    return FakeOutputSurface::Create3d(second_context_provider_);
  }

  virtual void SetupTree() OVERRIDE {
    root_ = FakeContentLayer::Create(&client_);
    root_->SetBounds(gfx::Size(20, 20));

    copy_layer_ = FakeContentLayer::Create(&client_);
    copy_layer_->SetBounds(gfx::Size(10, 10));
    root_->AddChild(copy_layer_);

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostCopyRequestTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE { PostSetNeedsCommitToMainThread(); }

  void CopyOutputCallback(scoped_ptr<CopyOutputResult> result) {
    EXPECT_TRUE(layer_tree_host()->proxy()->IsMainThread());
    EXPECT_EQ(gfx::Size(10, 10).ToString(), result->size().ToString());
    EXPECT_TRUE(result->HasTexture());

    // Save the result for later.
    EXPECT_FALSE(result_);
    result_ = result.Pass();

    // Post a commit to lose the output surface.
    layer_tree_host()->SetNeedsCommit();
  }

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    switch (layer_tree_host()->source_frame_number()) {
      case 1:
        // The layers have been pushed to the impl side. The layer textures have
        // been allocated.

        // Request a copy of the layer. This will use another texture.
        copy_layer_->RequestCopyOfOutput(CopyOutputRequest::CreateRequest(
            base::Bind(&LayerTreeHostCopyRequestTestLostOutputSurface::
                            CopyOutputCallback,
                       base::Unretained(this))));
        break;
      case 4:
      // With SingleThreadProxy it takes two commits to finally swap after a
      // context loss.
      case 5:
        // Now destroy the CopyOutputResult, releasing the texture inside back
        // to the compositor.
        EXPECT_TRUE(result_);
        result_.reset();

        // Check that it is released.
        ImplThreadTaskRunner()->PostTask(
            FROM_HERE,
            base::Bind(&LayerTreeHostCopyRequestTestLostOutputSurface::
                            CheckNumTextures,
                       base::Unretained(this),
                       num_textures_after_loss_ - 1));
        break;
    }
  }

  virtual void SwapBuffersOnThread(LayerTreeHostImpl* impl,
                                   bool result) OVERRIDE {
    switch (impl->active_tree()->source_frame_number()) {
      case 0:
        // The layers have been drawn, so their textures have been allocated.
        EXPECT_FALSE(result_);
        num_textures_without_readback_ =
            first_context_provider_->TestContext3d()->NumTextures();
        break;
      case 1:
        // We did a readback, so there will be a readback texture around now.
        EXPECT_LT(num_textures_without_readback_,
                  first_context_provider_->TestContext3d()->NumTextures());
        break;
      case 2:
        // The readback texture is collected.
        EXPECT_TRUE(result_);

        // Lose the output surface.
        first_context_provider_->TestContext3d()->loseContextCHROMIUM(
            GL_GUILTY_CONTEXT_RESET_ARB, GL_INNOCENT_CONTEXT_RESET_ARB);
        break;
      case 3:
      // With SingleThreadProxy it takes two commits to finally swap after a
      // context loss.
      case 4:
        // The output surface has been recreated.
        EXPECT_TRUE(second_context_provider_.get());

        num_textures_after_loss_ =
            first_context_provider_->TestContext3d()->NumTextures();
        break;
    }
  }

  void CheckNumTextures(size_t expected_num_textures) {
    EXPECT_EQ(expected_num_textures,
              first_context_provider_->TestContext3d()->NumTextures());
    EndTest();
  }

  virtual void AfterTest() OVERRIDE {}

  scoped_refptr<TestContextProvider> first_context_provider_;
  scoped_refptr<TestContextProvider> second_context_provider_;
  size_t num_textures_without_readback_;
  size_t num_textures_after_loss_;
  FakeContentLayerClient client_;
  scoped_refptr<FakeContentLayer> root_;
  scoped_refptr<FakeContentLayer> copy_layer_;
  scoped_ptr<CopyOutputResult> result_;
};

SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_NOIMPL_TEST_F(
    LayerTreeHostCopyRequestTestLostOutputSurface);

class LayerTreeHostCopyRequestTestCountTextures
    : public LayerTreeHostCopyRequestTest {
 protected:
  virtual scoped_ptr<FakeOutputSurface> CreateFakeOutputSurface(bool fallback)
      OVERRIDE {
    context_provider_ = TestContextProvider::Create();
    return FakeOutputSurface::Create3d(context_provider_);
  }

  virtual void SetupTree() OVERRIDE {
    root_ = FakeContentLayer::Create(&client_);
    root_->SetBounds(gfx::Size(20, 20));

    copy_layer_ = FakeContentLayer::Create(&client_);
    copy_layer_->SetBounds(gfx::Size(10, 10));
    root_->AddChild(copy_layer_);

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostCopyRequestTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE {
    num_textures_without_readback_ = 0;
    num_textures_with_readback_ = 0;
    waited_sync_point_after_readback_ = 0;
    PostSetNeedsCommitToMainThread();
  }

  virtual void RequestCopy(Layer* layer) = 0;

  virtual void DidCommitAndDrawFrame() OVERRIDE {
    switch (layer_tree_host()->source_frame_number()) {
      case 1:
        // The layers have been pushed to the impl side. The layer textures have
        // been allocated.
        RequestCopy(copy_layer_.get());
        break;
    }
  }

  virtual void SwapBuffersOnThread(LayerTreeHostImpl* impl,
                                   bool result) OVERRIDE {
    switch (impl->active_tree()->source_frame_number()) {
      case 0:
        // The layers have been drawn, so their textures have been allocated.
        num_textures_without_readback_ =
            context_provider_->TestContext3d()->NumTextures();
        break;
      case 1:
        // We did a readback, so there will be a readback texture around now.
        num_textures_with_readback_ =
            context_provider_->TestContext3d()->NumTextures();
        waited_sync_point_after_readback_ =
            context_provider_->TestContext3d()->last_waited_sync_point();

        MainThreadTaskRunner()->PostTask(
            FROM_HERE,
            base::Bind(&LayerTreeHostCopyRequestTestCountTextures::DoEndTest,
                       base::Unretained(this)));
        break;
    }
  }

  virtual void DoEndTest() { EndTest(); }

  scoped_refptr<TestContextProvider> context_provider_;
  size_t num_textures_without_readback_;
  size_t num_textures_with_readback_;
  unsigned waited_sync_point_after_readback_;
  FakeContentLayerClient client_;
  scoped_refptr<FakeContentLayer> root_;
  scoped_refptr<FakeContentLayer> copy_layer_;
};

class LayerTreeHostCopyRequestTestCreatesTexture
    : public LayerTreeHostCopyRequestTestCountTextures {
 protected:
  virtual void RequestCopy(Layer* layer) OVERRIDE {
    // Request a normal texture copy. This should create a new texture.
    copy_layer_->RequestCopyOfOutput(
        CopyOutputRequest::CreateRequest(base::Bind(
            &LayerTreeHostCopyRequestTestCreatesTexture::CopyOutputCallback,
            base::Unretained(this))));
  }

  void CopyOutputCallback(scoped_ptr<CopyOutputResult> result) {
    EXPECT_FALSE(result->IsEmpty());
    EXPECT_TRUE(result->HasTexture());

    TextureMailbox mailbox;
    scoped_ptr<SingleReleaseCallback> release;
    result->TakeTexture(&mailbox, &release);
    EXPECT_TRUE(release);

    release->Run(0, false);
  }

  virtual void AfterTest() OVERRIDE {
    // No sync point was needed.
    EXPECT_EQ(0u, waited_sync_point_after_readback_);
    // Except the copy to have made another texture.
    EXPECT_EQ(num_textures_without_readback_ + 1, num_textures_with_readback_);
  }
};

SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_NOIMPL_TEST_F(
    LayerTreeHostCopyRequestTestCreatesTexture);

class LayerTreeHostCopyRequestTestProvideTexture
    : public LayerTreeHostCopyRequestTestCountTextures {
 protected:
  virtual void BeginTest() OVERRIDE {
    external_context_provider_ = TestContextProvider::Create();
    EXPECT_TRUE(external_context_provider_->BindToCurrentThread());
    LayerTreeHostCopyRequestTestCountTextures::BeginTest();
  }

  void CopyOutputCallback(scoped_ptr<CopyOutputResult> result) {
    EXPECT_FALSE(result->IsEmpty());
    EXPECT_TRUE(result->HasTexture());

    TextureMailbox mailbox;
    scoped_ptr<SingleReleaseCallback> release;
    result->TakeTexture(&mailbox, &release);
    EXPECT_FALSE(release);
  }

  virtual void RequestCopy(Layer* layer) OVERRIDE {
    // Request a copy to a provided texture. This should not create a new
    // texture.
    scoped_ptr<CopyOutputRequest> request =
        CopyOutputRequest::CreateRequest(base::Bind(
            &LayerTreeHostCopyRequestTestProvideTexture::CopyOutputCallback,
            base::Unretained(this)));

    gpu::gles2::GLES2Interface* gl = external_context_provider_->ContextGL();
    gpu::Mailbox mailbox;
    gl->GenMailboxCHROMIUM(mailbox.name);
    sync_point_ = gl->InsertSyncPointCHROMIUM();
    request->SetTextureMailbox(
        TextureMailbox(mailbox, GL_TEXTURE_2D, sync_point_));
    EXPECT_TRUE(request->has_texture_mailbox());

    copy_layer_->RequestCopyOfOutput(request.Pass());
  }

  virtual void AfterTest() OVERRIDE {
    // Expect the compositor to have waited for the sync point in the provided
    // TextureMailbox.
    EXPECT_EQ(sync_point_, waited_sync_point_after_readback_);
    // Except the copy to have *not* made another texture.
    EXPECT_EQ(num_textures_without_readback_, num_textures_with_readback_);
  }

  scoped_refptr<TestContextProvider> external_context_provider_;
  unsigned sync_point_;
};

SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_NOIMPL_TEST_F(
    LayerTreeHostCopyRequestTestProvideTexture);

class LayerTreeHostCopyRequestTestDestroyBeforeCopy
    : public LayerTreeHostCopyRequestTest {
 protected:
  virtual void SetupTree() OVERRIDE {
    root_ = FakeContentLayer::Create(&client_);
    root_->SetBounds(gfx::Size(20, 20));

    copy_layer_ = FakeContentLayer::Create(&client_);
    copy_layer_->SetBounds(gfx::Size(10, 10));
    root_->AddChild(copy_layer_);

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostCopyRequestTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE {
    callback_count_ = 0;
    PostSetNeedsCommitToMainThread();
  }

  void CopyOutputCallback(scoped_ptr<CopyOutputResult> result) {
    EXPECT_TRUE(result->IsEmpty());
    ++callback_count_;
  }

  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    MainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&LayerTreeHostCopyRequestTestDestroyBeforeCopy::DidActivate,
                   base::Unretained(this)));
  }

  void DidActivate() {
    switch (layer_tree_host()->source_frame_number()) {
      case 1: {
        EXPECT_EQ(0, callback_count_);
        // Put a copy request on the layer, but then don't allow any
        // drawing to take place.
        scoped_ptr<CopyOutputRequest> request =
            CopyOutputRequest::CreateRequest(
                base::Bind(&LayerTreeHostCopyRequestTestDestroyBeforeCopy::
                                CopyOutputCallback,
                           base::Unretained(this)));
        copy_layer_->RequestCopyOfOutput(request.Pass());

        layer_tree_host()->SetViewportSize(gfx::Size());
        break;
      }
      case 2:
        EXPECT_EQ(0, callback_count_);
        // Remove the copy layer before we were able to draw.
        copy_layer_->RemoveFromParent();
        break;
      case 3:
        EXPECT_EQ(1, callback_count_);
        // Allow us to draw now.
        layer_tree_host()->SetViewportSize(
            layer_tree_host()->root_layer()->bounds());
        break;
      case 4:
        EXPECT_EQ(1, callback_count_);
        // We should not have crashed.
        EndTest();
    }
  }

  virtual void AfterTest() OVERRIDE {}

  int callback_count_;
  FakeContentLayerClient client_;
  scoped_refptr<FakeContentLayer> root_;
  scoped_refptr<FakeContentLayer> copy_layer_;
};

SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_TEST_F(
    LayerTreeHostCopyRequestTestDestroyBeforeCopy);

class LayerTreeHostCopyRequestTestShutdownBeforeCopy
    : public LayerTreeHostCopyRequestTest {
 protected:
  virtual void SetupTree() OVERRIDE {
    root_ = FakeContentLayer::Create(&client_);
    root_->SetBounds(gfx::Size(20, 20));

    copy_layer_ = FakeContentLayer::Create(&client_);
    copy_layer_->SetBounds(gfx::Size(10, 10));
    root_->AddChild(copy_layer_);

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostCopyRequestTest::SetupTree();
  }

  virtual void BeginTest() OVERRIDE {
    callback_count_ = 0;
    PostSetNeedsCommitToMainThread();
  }

  void CopyOutputCallback(scoped_ptr<CopyOutputResult> result) {
    EXPECT_TRUE(result->IsEmpty());
    ++callback_count_;
  }

  virtual void DidActivateTreeOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    MainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&LayerTreeHostCopyRequestTestShutdownBeforeCopy::DidActivate,
                   base::Unretained(this)));
  }

  void DidActivate() {
    switch (layer_tree_host()->source_frame_number()) {
      case 1: {
        EXPECT_EQ(0, callback_count_);
        // Put a copy request on the layer, but then don't allow any
        // drawing to take place.
        scoped_ptr<CopyOutputRequest> request =
            CopyOutputRequest::CreateRequest(
                base::Bind(&LayerTreeHostCopyRequestTestShutdownBeforeCopy::
                                CopyOutputCallback,
                           base::Unretained(this)));
        copy_layer_->RequestCopyOfOutput(request.Pass());

        layer_tree_host()->SetViewportSize(gfx::Size());
        break;
      }
      case 2:
        DestroyLayerTreeHost();
        // End the test after the copy result has had a chance to get back to
        // the main thread.
        MainThreadTaskRunner()->PostTask(
            FROM_HERE,
            base::Bind(&LayerTreeHostCopyRequestTestShutdownBeforeCopy::EndTest,
                       base::Unretained(this)));
        break;
    }
  }

  virtual void AfterTest() OVERRIDE { EXPECT_EQ(1, callback_count_); }

  int callback_count_;
  FakeContentLayerClient client_;
  scoped_refptr<FakeContentLayer> root_;
  scoped_refptr<FakeContentLayer> copy_layer_;
};

SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_TEST_F(
    LayerTreeHostCopyRequestTestShutdownBeforeCopy);

}  // namespace
}  // namespace cc
