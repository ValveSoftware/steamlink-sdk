// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/layers/layer_iterator.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_picture_layer.h"
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
  void SetupTree() override {
    root = FakePictureLayer::Create(&client_);
    root->SetBounds(gfx::Size(20, 20));

    child = FakePictureLayer::Create(&client_);
    child->SetBounds(gfx::Size(10, 10));
    root->AddChild(child);

    grand_child = FakePictureLayer::Create(&client_);
    grand_child->SetBounds(gfx::Size(5, 5));
    child->AddChild(grand_child);

    layer_tree_host()->SetRootLayer(root);
    LayerTreeHostCopyRequestTest::SetupTree();
    client_.set_bounds(root->bounds());
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void DidCommit() override { WaitForCallback(); }

  void WaitForCallback() {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
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
                       base::Unretained(this), 0)));
        EXPECT_EQ(0u, callbacks_.size());
        break;
      case 2:
        // This commit is triggered by the copy request having been completed.
        break;
      case 3:
        if (callbacks_.size() < 1u) {
          WaitForCallback();
          return;
        }
        EXPECT_EQ(1u, callbacks_.size());
        EXPECT_EQ(gfx::Size(10, 10).ToString(), callbacks_[0].ToString());

        child->RequestCopyOfOutput(CopyOutputRequest::CreateBitmapRequest(
            base::Bind(&LayerTreeHostCopyRequestTestMultipleRequests::
                           CopyOutputCallback,
                       base::Unretained(this), 1)));
        root->RequestCopyOfOutput(CopyOutputRequest::CreateBitmapRequest(
            base::Bind(&LayerTreeHostCopyRequestTestMultipleRequests::
                           CopyOutputCallback,
                       base::Unretained(this), 2)));
        grand_child->RequestCopyOfOutput(CopyOutputRequest::CreateBitmapRequest(
            base::Bind(&LayerTreeHostCopyRequestTestMultipleRequests::
                           CopyOutputCallback,
                       base::Unretained(this), 3)));
        EXPECT_EQ(1u, callbacks_.size());
        break;
      case 4:
        // This commit is triggered by the copy request having been completed.
        break;
      case 5:
        if (callbacks_.size() < 4u) {
          WaitForCallback();
          return;
        }
        EXPECT_EQ(4u, callbacks_.size());

        // The |child| was copied to a bitmap and passed back in Case 1.
        EXPECT_EQ(gfx::Size(10, 10).ToString(), callbacks_[0].ToString());

        // The |child| was copied to a bitmap and passed back in Case 2.
        EXPECT_EQ(gfx::Size(10, 10).ToString(), callbacks_[1].ToString());
        // The |root| was copied to a bitmap and passed back also in Case 2.
        EXPECT_EQ(gfx::Size(20, 20).ToString(), callbacks_[2].ToString());
        // The |grand_child| was copied to a bitmap and passed back in Case 2.
        EXPECT_EQ(gfx::Size(5, 5).ToString(), callbacks_[3].ToString());
        EndTest();
        break;
    }
  }

  void CopyOutputCallback(size_t id, std::unique_ptr<CopyOutputResult> result) {
    EXPECT_TRUE(layer_tree_host()->task_runner_provider()->IsMainThread());
    EXPECT_TRUE(result->HasBitmap());
    std::unique_ptr<SkBitmap> bitmap = result->TakeBitmap();
    EXPECT_EQ(result->size().ToString(),
              gfx::Size(bitmap->width(), bitmap->height()).ToString());
    callbacks_[id] = result->size();
  }

  void AfterTest() override { EXPECT_EQ(4u, callbacks_.size()); }

  std::unique_ptr<FakeOutputSurface> CreateFakeOutputSurface() override {
    if (!use_gl_renderer_) {
      return FakeOutputSurface::CreateSoftware(
          base::WrapUnique(new SoftwareOutputDevice));
    }
    std::unique_ptr<FakeOutputSurface> output_surface =
        FakeOutputSurface::Create3d();
    TestContextSupport* context_support = static_cast<TestContextSupport*>(
        output_surface->context_provider()->ContextSupport());
    context_support->set_out_of_order_callbacks(out_of_order_callbacks_);
    return output_surface;
  }

  bool use_gl_renderer_;
  bool out_of_order_callbacks_ = false;
  std::map<size_t, gfx::Size> callbacks_;
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> root;
  scoped_refptr<FakePictureLayer> child;
  scoped_refptr<FakePictureLayer> grand_child;
};

// Readback can't be done with a delegating renderer.
TEST_F(LayerTreeHostCopyRequestTestMultipleRequests,
       GLRenderer_RunSingleThread) {
  use_gl_renderer_ = true;
  RunTest(CompositorMode::SINGLE_THREADED, false);
}

TEST_F(LayerTreeHostCopyRequestTestMultipleRequests,
       GLRenderer_RunMultiThread) {
  use_gl_renderer_ = true;
  RunTest(CompositorMode::THREADED, false);
}

TEST_F(LayerTreeHostCopyRequestTestMultipleRequests,
       GLRenderer_RunSingleThread_OutOfOrderCallbacks) {
  use_gl_renderer_ = true;
  out_of_order_callbacks_ = true;
  RunTest(CompositorMode::SINGLE_THREADED, false);
}

TEST_F(LayerTreeHostCopyRequestTestMultipleRequests,
       GLRenderer_RunMultiThread_OutOfOrderCallbacks) {
  use_gl_renderer_ = true;
  out_of_order_callbacks_ = true;
  RunTest(CompositorMode::THREADED, false);
}

TEST_F(LayerTreeHostCopyRequestTestMultipleRequests,
       SoftwareRenderer_RunSingleThread) {
  use_gl_renderer_ = false;
  RunTest(CompositorMode::SINGLE_THREADED, false);
}

TEST_F(LayerTreeHostCopyRequestTestMultipleRequests,
       SoftwareRenderer_RunMultiThread) {
  use_gl_renderer_ = false;
  RunTest(CompositorMode::THREADED, false);
}

// TODO(crbug.com/564832): Remove this test when the workaround it tests is no
// longer needed.
class LayerTreeHostCopyRequestCompletionCausesCommit
    : public LayerTreeHostCopyRequestTest {
 protected:
  void SetupTree() override {
    root_ = FakePictureLayer::Create(&client_);
    root_->SetBounds(gfx::Size(20, 20));

    layer_ = FakePictureLayer::Create(&client_);
    layer_->SetBounds(gfx::Size(15, 15));
    root_->AddChild(layer_);

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostCopyRequestTest::SetupTree();
    client_.set_bounds(root_->bounds());
  }

  void BeginTest() override {
    PostSetNeedsCommitToMainThread();
  }

  void DidCommit() override {
    int frame = layer_tree_host()->source_frame_number();
    switch (frame) {
      case 1:
        layer_->RequestCopyOfOutput(CopyOutputRequest::CreateBitmapRequest(
            base::Bind(&LayerTreeHostCopyRequestCompletionCausesCommit::
                           CopyOutputCallback)));
        break;
      case 2:
        // This commit is triggered by the copy request.
        break;
      case 3:
        // This commit is triggered by the completion of the copy request.
        EndTest();
        break;
    }
  }

  static void CopyOutputCallback(std::unique_ptr<CopyOutputResult> result) {
    EXPECT_FALSE(result->IsEmpty());
  }

  void AfterTest() override {}

  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> root_;
  scoped_refptr<FakePictureLayer> layer_;
};

SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_TEST_F(
    LayerTreeHostCopyRequestCompletionCausesCommit);

class LayerTreeHostCopyRequestTestLayerDestroyed
    : public LayerTreeHostCopyRequestTest {
 protected:
  void SetupTree() override {
    root_ = FakePictureLayer::Create(&client_);
    root_->SetBounds(gfx::Size(20, 20));

    main_destroyed_ = FakePictureLayer::Create(&client_);
    main_destroyed_->SetBounds(gfx::Size(15, 15));
    root_->AddChild(main_destroyed_);

    impl_destroyed_ = FakePictureLayer::Create(&client_);
    impl_destroyed_->SetBounds(gfx::Size(10, 10));
    root_->AddChild(impl_destroyed_);

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostCopyRequestTest::SetupTree();
    client_.set_bounds(root_->bounds());
  }

  void BeginTest() override {
    callback_count_ = 0;
    PostSetNeedsCommitToMainThread();
  }

  void DidCommit() override {
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

  void CopyOutputCallback(std::unique_ptr<CopyOutputResult> result) {
    EXPECT_TRUE(layer_tree_host()->task_runner_provider()->IsMainThread());
    EXPECT_TRUE(result->IsEmpty());
    ++callback_count_;
  }

  void AfterTest() override {}

  int callback_count_;
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> root_;
  scoped_refptr<FakePictureLayer> main_destroyed_;
  scoped_refptr<FakePictureLayer> impl_destroyed_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostCopyRequestTestLayerDestroyed);

class LayerTreeHostCopyRequestTestInHiddenSubtree
    : public LayerTreeHostCopyRequestTest {
 protected:
  void SetupTree() override {
    root_ = FakePictureLayer::Create(&client_);
    root_->SetBounds(gfx::Size(20, 20));

    grand_parent_layer_ = FakePictureLayer::Create(&client_);
    grand_parent_layer_->SetBounds(gfx::Size(15, 15));
    root_->AddChild(grand_parent_layer_);

    // parent_layer_ owns a render surface.
    parent_layer_ = FakePictureLayer::Create(&client_);
    parent_layer_->SetBounds(gfx::Size(15, 15));
    parent_layer_->SetForceRenderSurfaceForTesting(true);
    grand_parent_layer_->AddChild(parent_layer_);

    copy_layer_ = FakePictureLayer::Create(&client_);
    copy_layer_->SetBounds(gfx::Size(10, 10));
    parent_layer_->AddChild(copy_layer_);

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostCopyRequestTest::SetupTree();
    client_.set_bounds(root_->bounds());
  }

  void AddCopyRequest(Layer* layer) {
    layer->RequestCopyOfOutput(
        CopyOutputRequest::CreateBitmapRequest(base::Bind(
            &LayerTreeHostCopyRequestTestInHiddenSubtree::CopyOutputCallback,
            base::Unretained(this))));
  }

  void BeginTest() override {
    callback_count_ = 0;
    PostSetNeedsCommitToMainThread();

    AddCopyRequest(copy_layer_.get());
  }

  void CopyOutputCallback(std::unique_ptr<CopyOutputResult> result) {
    ++callback_count_;
    EXPECT_TRUE(layer_tree_host()->task_runner_provider()->IsMainThread());
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

  void AfterTest() override {}

  int callback_count_;
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> root_;
  scoped_refptr<FakePictureLayer> grand_parent_layer_;
  scoped_refptr<FakePictureLayer> parent_layer_;
  scoped_refptr<FakePictureLayer> copy_layer_;
};

SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_TEST_F(
    LayerTreeHostCopyRequestTestInHiddenSubtree);

class LayerTreeHostTestHiddenSurfaceNotAllocatedForSubtreeCopyRequest
    : public LayerTreeHostCopyRequestTest {
 protected:
  void SetupTree() override {
    root_ = FakePictureLayer::Create(&client_);
    root_->SetBounds(gfx::Size(20, 20));

    grand_parent_layer_ = FakePictureLayer::Create(&client_);
    grand_parent_layer_->SetBounds(gfx::Size(15, 15));
    grand_parent_layer_->SetHideLayerAndSubtree(true);
    root_->AddChild(grand_parent_layer_);

    // parent_layer_ owns a render surface.
    parent_layer_ = FakePictureLayer::Create(&client_);
    parent_layer_->SetBounds(gfx::Size(15, 15));
    parent_layer_->SetForceRenderSurfaceForTesting(true);
    grand_parent_layer_->AddChild(parent_layer_);

    copy_layer_ = FakePictureLayer::Create(&client_);
    copy_layer_->SetBounds(gfx::Size(10, 10));
    parent_layer_->AddChild(copy_layer_);

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostCopyRequestTest::SetupTree();
    client_.set_bounds(root_->bounds());
  }

  void BeginTest() override {
    did_draw_ = false;
    PostSetNeedsCommitToMainThread();

    copy_layer_->RequestCopyOfOutput(
        CopyOutputRequest::CreateBitmapRequest(base::Bind(
            &LayerTreeHostTestHiddenSurfaceNotAllocatedForSubtreeCopyRequest::
                 CopyOutputCallback,
            base::Unretained(this))));
  }

  void CopyOutputCallback(std::unique_ptr<CopyOutputResult> result) {
    EXPECT_TRUE(layer_tree_host()->task_runner_provider()->IsMainThread());
    EXPECT_EQ(copy_layer_->bounds().ToString(), result->size().ToString());
    EndTest();
  }

  void DrawLayersOnThread(LayerTreeHostImpl* host_impl) override {
    Renderer* renderer = host_impl->renderer();

    LayerImpl* parent =
        host_impl->active_tree()->LayerById(parent_layer_->id());
    LayerImpl* copy_layer =
        host_impl->active_tree()->LayerById(copy_layer_->id());

    // |parent| owns a surface, but it was hidden and not part of the copy
    // request so it should not allocate any resource.
    EXPECT_FALSE(renderer->HasAllocatedResourcesForTesting(
        parent->render_surface()->GetRenderPassId()));

    // |copy_layer| should have been rendered to a texture since it was needed
    // for a copy request.
    if (did_draw_) {
      // TODO(crbug.com/564832): Ignore the extra frame that occurs due to copy
      // completion. This can be removed when the extra commit is removed.
      EXPECT_FALSE(copy_layer->render_surface());
    } else {
      EXPECT_TRUE(renderer->HasAllocatedResourcesForTesting(
          copy_layer->render_surface()->GetRenderPassId()));
    }

    did_draw_ = true;
  }

  void AfterTest() override { EXPECT_TRUE(did_draw_); }

  FakeContentLayerClient client_;
  bool did_draw_;
  scoped_refptr<FakePictureLayer> root_;
  scoped_refptr<FakePictureLayer> grand_parent_layer_;
  scoped_refptr<FakePictureLayer> parent_layer_;
  scoped_refptr<FakePictureLayer> copy_layer_;
};

// No output to copy for delegated renderers.
SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_TEST_F(
    LayerTreeHostTestHiddenSurfaceNotAllocatedForSubtreeCopyRequest);

class LayerTreeHostCopyRequestTestClippedOut
    : public LayerTreeHostCopyRequestTest {
 protected:
  void SetupTree() override {
    root_ = FakePictureLayer::Create(&client_);
    root_->SetBounds(gfx::Size(20, 20));

    parent_layer_ = FakePictureLayer::Create(&client_);
    parent_layer_->SetBounds(gfx::Size(15, 15));
    parent_layer_->SetMasksToBounds(true);
    root_->AddChild(parent_layer_);

    copy_layer_ = FakePictureLayer::Create(&client_);
    copy_layer_->SetPosition(gfx::PointF(15.f, 15.f));
    copy_layer_->SetBounds(gfx::Size(10, 10));
    parent_layer_->AddChild(copy_layer_);

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostCopyRequestTest::SetupTree();
    client_.set_bounds(root_->bounds());
  }

  void BeginTest() override {
    PostSetNeedsCommitToMainThread();

    copy_layer_->RequestCopyOfOutput(CopyOutputRequest::CreateBitmapRequest(
        base::Bind(&LayerTreeHostCopyRequestTestClippedOut::CopyOutputCallback,
                   base::Unretained(this))));
  }

  void CopyOutputCallback(std::unique_ptr<CopyOutputResult> result) {
    // We should still get the content even if the copy requested layer was
    // completely clipped away.
    EXPECT_TRUE(layer_tree_host()->task_runner_provider()->IsMainThread());
    EXPECT_EQ(gfx::Size(10, 10).ToString(), result->size().ToString());
    EndTest();
  }

  void AfterTest() override {}

  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> root_;
  scoped_refptr<FakePictureLayer> parent_layer_;
  scoped_refptr<FakePictureLayer> copy_layer_;
};

SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_TEST_F(
    LayerTreeHostCopyRequestTestClippedOut);

class LayerTreeHostCopyRequestTestScaledLayer
    : public LayerTreeHostCopyRequestTest {
 protected:
  void SetupTree() override {
    root_ = Layer::Create();
    root_->SetBounds(gfx::Size(20, 20));

    gfx::Transform scale;
    scale.Scale(2, 2);

    copy_layer_ = Layer::Create();
    copy_layer_->SetBounds(gfx::Size(10, 10));
    copy_layer_->SetTransform(scale);
    root_->AddChild(copy_layer_);

    child_layer_ = FakePictureLayer::Create(&client_);
    child_layer_->SetBounds(gfx::Size(10, 10));
    copy_layer_->AddChild(child_layer_);

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostCopyRequestTest::SetupTree();
    client_.set_bounds(root_->bounds());
  }

  void BeginTest() override {
    PostSetNeedsCommitToMainThread();

    std::unique_ptr<CopyOutputRequest> request =
        CopyOutputRequest::CreateBitmapRequest(base::Bind(
            &LayerTreeHostCopyRequestTestScaledLayer::CopyOutputCallback,
            base::Unretained(this)));
    request->set_area(gfx::Rect(5, 5));
    copy_layer_->RequestCopyOfOutput(std::move(request));
  }

  void CopyOutputCallback(std::unique_ptr<CopyOutputResult> result) {
    // The request area is expressed in layer space, but the result's size takes
    // into account the transform from layer space to surface space.
    EXPECT_EQ(gfx::Size(10, 10), result->size());
    EndTest();
  }

  void AfterTest() override {}

  FakeContentLayerClient client_;
  scoped_refptr<Layer> root_;
  scoped_refptr<Layer> copy_layer_;
  scoped_refptr<FakePictureLayer> child_layer_;
};

SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_TEST_F(
    LayerTreeHostCopyRequestTestScaledLayer);

class LayerTreeHostTestAsyncTwoReadbacksWithoutDraw
    : public LayerTreeHostCopyRequestTest {
 protected:
  void SetupTree() override {
    root_ = FakePictureLayer::Create(&client_);
    root_->SetBounds(gfx::Size(20, 20));

    copy_layer_ = FakePictureLayer::Create(&client_);
    copy_layer_->SetBounds(gfx::Size(10, 10));
    root_->AddChild(copy_layer_);

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostCopyRequestTest::SetupTree();
    client_.set_bounds(root_->bounds());
  }

  void AddCopyRequest(Layer* layer) {
    layer->RequestCopyOfOutput(
        CopyOutputRequest::CreateBitmapRequest(base::Bind(
            &LayerTreeHostTestAsyncTwoReadbacksWithoutDraw::CopyOutputCallback,
            base::Unretained(this))));
  }

  void BeginTest() override {
    saw_copy_request_ = false;
    callback_count_ = 0;
    PostSetNeedsCommitToMainThread();

    // Prevent drawing.
    layer_tree_host()->SetViewportSize(gfx::Size(0, 0));

    AddCopyRequest(copy_layer_.get());
  }

  void DidActivateTreeOnThread(LayerTreeHostImpl* impl) override {
    if (impl->active_tree()->source_frame_number() == 0) {
      EXPECT_TRUE(impl->active_tree()->LayerById(copy_layer_->id()));
      saw_copy_request_ = true;
    }
  }

  void DidCommit() override {
    if (layer_tree_host()->source_frame_number() == 1) {
      // Allow drawing.
      layer_tree_host()->SetViewportSize(gfx::Size(root_->bounds()));

      AddCopyRequest(copy_layer_.get());
    }
  }

  void CopyOutputCallback(std::unique_ptr<CopyOutputResult> result) {
    EXPECT_TRUE(layer_tree_host()->task_runner_provider()->IsMainThread());

    // The first frame can't be drawn.
    switch (callback_count_) {
      case 0:
        EXPECT_TRUE(result->IsEmpty());
        EXPECT_EQ(gfx::Size(), result->size());
        break;
      case 1:
        EXPECT_FALSE(result->IsEmpty());
        EXPECT_EQ(copy_layer_->bounds().ToString(), result->size().ToString());
        EndTest();
        break;
      default:
        NOTREACHED();
        break;
    }

    ++callback_count_;
  }

  void AfterTest() override { EXPECT_TRUE(saw_copy_request_); }

  bool saw_copy_request_;
  int callback_count_;
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> root_;
  scoped_refptr<FakePictureLayer> copy_layer_;
};

SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_TEST_F(
    LayerTreeHostTestAsyncTwoReadbacksWithoutDraw);

class LayerTreeHostCopyRequestTestLostOutputSurface
    : public LayerTreeHostCopyRequestTest {
 protected:
  std::unique_ptr<FakeOutputSurface> CreateFakeOutputSurface() override {
    if (!first_context_provider_) {
      first_context_provider_ = TestContextProvider::Create();
      return FakeOutputSurface::Create3d(first_context_provider_);
    }

    EXPECT_FALSE(second_context_provider_);
    second_context_provider_ = TestContextProvider::Create();
    return FakeOutputSurface::Create3d(second_context_provider_);
  }

  void SetupTree() override {
    root_ = FakePictureLayer::Create(&client_);
    root_->SetBounds(gfx::Size(20, 20));

    copy_layer_ = FakePictureLayer::Create(&client_);
    copy_layer_->SetBounds(gfx::Size(10, 10));
    root_->AddChild(copy_layer_);

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostCopyRequestTest::SetupTree();
    client_.set_bounds(root_->bounds());
  }

  void BeginTest() override { PostSetNeedsCommitToMainThread(); }

  void ReceiveCopyRequestOutputAndCommit(
      std::unique_ptr<CopyOutputResult> result) {
    EXPECT_TRUE(layer_tree_host()->task_runner_provider()->IsMainThread());
    EXPECT_EQ(gfx::Size(10, 10).ToString(), result->size().ToString());
    EXPECT_TRUE(result->HasTexture());

    // Save the result for later.
    EXPECT_FALSE(result_);
    result_ = std::move(result);

    // Post a commit to lose the output surface.
    layer_tree_host()->SetNeedsCommit();
  }

  void InsertCopyRequest() {
    copy_layer_->RequestCopyOfOutput(CopyOutputRequest::CreateRequest(
        base::Bind(&LayerTreeHostCopyRequestTestLostOutputSurface::
                       ReceiveCopyRequestOutputAndCommit,
                   base::Unretained(this))));
  }

  void DestroyCopyResultAndCheckNumTextures() {
    EXPECT_TRUE(result_);
    result_ = nullptr;

    ImplThreadTaskRunner()->PostTask(
        FROM_HERE, base::Bind(&LayerTreeHostCopyRequestTestLostOutputSurface::
                                  CheckNumTexturesAfterReadbackDestroyed,
                              base::Unretained(this)));
  }

  void CheckNumTexturesAfterReadbackDestroyed() {
    // After the loss we had |num_textures_after_loss_| many textures, but
    // releasing the copy output request will cause the texture in the request
    // to be released, so we should have 1 less by now.
    EXPECT_EQ(num_textures_after_loss_ - 1,
              first_context_provider_->TestContext3d()->NumTextures());
    EndTest();
  }

  void SwapBuffersOnThread(LayerTreeHostImpl* impl, bool result) override {
    switch (impl->active_tree()->source_frame_number()) {
      case 0:
        // The layers have been drawn, so their textures have been allocated.
        EXPECT_FALSE(result_);
        num_textures_without_readback_ =
            first_context_provider_->TestContext3d()->NumTextures();

        // Request a copy of the layer. This will use another texture.
        MainThreadTaskRunner()->PostTask(
            FROM_HERE,
            base::Bind(&LayerTreeHostCopyRequestTestLostOutputSurface::
                           InsertCopyRequest,
                       base::Unretained(this)));
        break;
      case 1:
        // We did a readback, so there will be a readback texture around now.
        EXPECT_LT(num_textures_without_readback_,
                  first_context_provider_->TestContext3d()->NumTextures());

        // The copy request will be serviced and the result sent to
        // ReceiveCopyRequestOutputAndCommit, which posts a new commit causing
        // the test to advance to the next case.
        break;
      case 2:
        // The readback texture is collected.
        EXPECT_TRUE(result_);

        // Lose the output surface.
        first_context_provider_->TestContext3d()->loseContextCHROMIUM(
            GL_GUILTY_CONTEXT_RESET_ARB, GL_INNOCENT_CONTEXT_RESET_ARB);
        break;
      case 3:
        // The output surface has been recreated.
        EXPECT_TRUE(second_context_provider_);

        num_textures_after_loss_ =
            first_context_provider_->TestContext3d()->NumTextures();

        // Now destroy the CopyOutputResult, releasing the texture inside back
        // to the compositor. Then check the resulting number of allocated
        // textures.
        MainThreadTaskRunner()->PostTask(
            FROM_HERE,
            base::Bind(&LayerTreeHostCopyRequestTestLostOutputSurface::
                           DestroyCopyResultAndCheckNumTextures,
                       base::Unretained(this)));
        break;
    }
  }

  void AfterTest() override {}

  scoped_refptr<TestContextProvider> first_context_provider_;
  scoped_refptr<TestContextProvider> second_context_provider_;
  size_t num_textures_without_readback_ = 0;
  size_t num_textures_after_loss_ = 0;
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> root_;
  scoped_refptr<FakePictureLayer> copy_layer_;
  std::unique_ptr<CopyOutputResult> result_;
};

SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_TEST_F(
    LayerTreeHostCopyRequestTestLostOutputSurface);

class LayerTreeHostCopyRequestTestCountTextures
    : public LayerTreeHostCopyRequestTest {
 protected:
  std::unique_ptr<FakeOutputSurface> CreateFakeOutputSurface() override {
    context_provider_ = TestContextProvider::Create();
    return FakeOutputSurface::Create3d(context_provider_);
  }

  void SetupTree() override {
    client_.set_fill_with_nonsolid_color(true);

    root_ = FakePictureLayer::Create(&client_);
    root_->SetBounds(gfx::Size(20, 20));

    copy_layer_ = FakePictureLayer::Create(&client_);
    copy_layer_->SetBounds(gfx::Size(10, 10));
    root_->AddChild(copy_layer_);

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostCopyRequestTest::SetupTree();
    client_.set_bounds(root_->bounds());
  }

  void BeginTest() override {
    num_textures_without_readback_ = 0;
    num_textures_with_readback_ = 0;
    waited_sync_token_after_readback_.Clear();
    PostSetNeedsCommitToMainThread();
  }

  virtual void RequestCopy(Layer* layer) = 0;

  void DidCommit() override {
    switch (layer_tree_host()->source_frame_number()) {
      case 1:
        // The layers have been pushed to the impl side. The layer textures have
        // been allocated.
        RequestCopy(copy_layer_.get());
        break;
    }
  }

  void SwapBuffersOnThread(LayerTreeHostImpl* impl, bool result) override {
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
        waited_sync_token_after_readback_ =
            context_provider_->TestContext3d()->last_waited_sync_token();

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
  gpu::SyncToken waited_sync_token_after_readback_;
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> root_;
  scoped_refptr<FakePictureLayer> copy_layer_;
};

class LayerTreeHostCopyRequestTestCreatesTexture
    : public LayerTreeHostCopyRequestTestCountTextures {
 protected:
  void RequestCopy(Layer* layer) override {
    // Request a normal texture copy. This should create a new texture.
    copy_layer_->RequestCopyOfOutput(
        CopyOutputRequest::CreateRequest(base::Bind(
            &LayerTreeHostCopyRequestTestCreatesTexture::CopyOutputCallback,
            base::Unretained(this))));
  }

  void CopyOutputCallback(std::unique_ptr<CopyOutputResult> result) {
    EXPECT_FALSE(result->IsEmpty());
    EXPECT_TRUE(result->HasTexture());

    TextureMailbox mailbox;
    std::unique_ptr<SingleReleaseCallback> release;
    result->TakeTexture(&mailbox, &release);
    EXPECT_TRUE(release);

    release->Run(gpu::SyncToken(), false);
  }

  void AfterTest() override {
    // No sync point was needed.
    EXPECT_FALSE(waited_sync_token_after_readback_.HasData());
    // Except the copy to have made another texture.
    EXPECT_EQ(num_textures_without_readback_ + 1, num_textures_with_readback_);
  }
};

SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_TEST_F(
    LayerTreeHostCopyRequestTestCreatesTexture);

class LayerTreeHostCopyRequestTestProvideTexture
    : public LayerTreeHostCopyRequestTestCountTextures {
 protected:
  void BeginTest() override {
    external_context_provider_ = TestContextProvider::Create();
    EXPECT_TRUE(external_context_provider_->BindToCurrentThread());
    LayerTreeHostCopyRequestTestCountTextures::BeginTest();
  }

  void CopyOutputCallback(std::unique_ptr<CopyOutputResult> result) {
    EXPECT_FALSE(result->IsEmpty());
    EXPECT_TRUE(result->HasTexture());

    TextureMailbox mailbox;
    std::unique_ptr<SingleReleaseCallback> release;
    result->TakeTexture(&mailbox, &release);
    EXPECT_FALSE(release);
  }

  void RequestCopy(Layer* layer) override {
    // Request a copy to a provided texture. This should not create a new
    // texture.
    std::unique_ptr<CopyOutputRequest> request =
        CopyOutputRequest::CreateRequest(base::Bind(
            &LayerTreeHostCopyRequestTestProvideTexture::CopyOutputCallback,
            base::Unretained(this)));

    gpu::gles2::GLES2Interface* gl = external_context_provider_->ContextGL();
    gpu::Mailbox mailbox;
    gl->GenMailboxCHROMIUM(mailbox.name);

    const GLuint64 fence_sync = gl->InsertFenceSyncCHROMIUM();
    gl->ShallowFlushCHROMIUM();
    gl->GenSyncTokenCHROMIUM(fence_sync, sync_token_.GetData());

    request->SetTextureMailbox(
        TextureMailbox(mailbox, sync_token_, GL_TEXTURE_2D));
    EXPECT_TRUE(request->has_texture_mailbox());

    copy_layer_->RequestCopyOfOutput(std::move(request));
  }

  void AfterTest() override {
    // Expect the compositor to have waited for the sync point in the provided
    // TextureMailbox.
    EXPECT_EQ(sync_token_, waited_sync_token_after_readback_);
    // Except the copy to have *not* made another texture.
    EXPECT_EQ(num_textures_without_readback_, num_textures_with_readback_);
  }

  scoped_refptr<TestContextProvider> external_context_provider_;
  gpu::SyncToken sync_token_;
};

SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_TEST_F(
    LayerTreeHostCopyRequestTestProvideTexture);

class LayerTreeHostCopyRequestTestDestroyBeforeCopy
    : public LayerTreeHostCopyRequestTest {
 protected:
  void SetupTree() override {
    root_ = FakePictureLayer::Create(&client_);
    root_->SetBounds(gfx::Size(20, 20));

    copy_layer_ = FakePictureLayer::Create(&client_);
    copy_layer_->SetBounds(gfx::Size(10, 10));
    root_->AddChild(copy_layer_);

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostCopyRequestTest::SetupTree();
    client_.set_bounds(root_->bounds());
  }

  void BeginTest() override {
    callback_count_ = 0;
    PostSetNeedsCommitToMainThread();
  }

  void CopyOutputCallback(std::unique_ptr<CopyOutputResult> result) {
    EXPECT_TRUE(result->IsEmpty());
    ++callback_count_;
  }

  void DidActivateTreeOnThread(LayerTreeHostImpl* impl) override {
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
        std::unique_ptr<CopyOutputRequest> request =
            CopyOutputRequest::CreateRequest(
                base::Bind(&LayerTreeHostCopyRequestTestDestroyBeforeCopy::
                               CopyOutputCallback,
                           base::Unretained(this)));
        copy_layer_->RequestCopyOfOutput(std::move(request));

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

  void AfterTest() override {}

  int callback_count_;
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> root_;
  scoped_refptr<FakePictureLayer> copy_layer_;
};

SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_TEST_F(
    LayerTreeHostCopyRequestTestDestroyBeforeCopy);

class LayerTreeHostCopyRequestTestShutdownBeforeCopy
    : public LayerTreeHostCopyRequestTest {
 protected:
  void SetupTree() override {
    root_ = FakePictureLayer::Create(&client_);
    root_->SetBounds(gfx::Size(20, 20));

    copy_layer_ = FakePictureLayer::Create(&client_);
    copy_layer_->SetBounds(gfx::Size(10, 10));
    root_->AddChild(copy_layer_);

    layer_tree_host()->SetRootLayer(root_);
    LayerTreeHostCopyRequestTest::SetupTree();
    client_.set_bounds(root_->bounds());
  }

  void BeginTest() override {
    callback_count_ = 0;
    PostSetNeedsCommitToMainThread();
  }

  void CopyOutputCallback(std::unique_ptr<CopyOutputResult> result) {
    EXPECT_TRUE(result->IsEmpty());
    ++callback_count_;
  }

  void DidActivateTreeOnThread(LayerTreeHostImpl* impl) override {
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
        std::unique_ptr<CopyOutputRequest> request =
            CopyOutputRequest::CreateRequest(
                base::Bind(&LayerTreeHostCopyRequestTestShutdownBeforeCopy::
                               CopyOutputCallback,
                           base::Unretained(this)));
        copy_layer_->RequestCopyOfOutput(std::move(request));

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

  void AfterTest() override { EXPECT_EQ(1, callback_count_); }

  int callback_count_;
  FakeContentLayerClient client_;
  scoped_refptr<FakePictureLayer> root_;
  scoped_refptr<FakePictureLayer> copy_layer_;
};

SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_TEST_F(
    LayerTreeHostCopyRequestTestShutdownBeforeCopy);

class LayerTreeHostCopyRequestTestMultipleDrawsHiddenCopyRequest
    : public LayerTreeHostCopyRequestTest {
 protected:
  void SetupTree() override {
    scoped_refptr<FakePictureLayer> root = FakePictureLayer::Create(&client_);
    root->SetBounds(gfx::Size(20, 20));

    child_ = FakePictureLayer::Create(&client_);
    child_->SetBounds(gfx::Size(10, 10));
    root->AddChild(child_);
    child_->SetHideLayerAndSubtree(true);

    layer_tree_host()->SetRootLayer(root);
    LayerTreeHostCopyRequestTest::SetupTree();
    client_.set_bounds(root->bounds());
  }

  void BeginTest() override {
    num_draws_ = 0;
    copy_happened_ = false;
    draw_happened_ = false;
    PostSetNeedsCommitToMainThread();
  }

  void DidCommit() override {
    // Send a copy request after the first commit.
    if (layer_tree_host()->source_frame_number() == 1) {
      child_->RequestCopyOfOutput(
          CopyOutputRequest::CreateBitmapRequest(base::Bind(
              &LayerTreeHostCopyRequestTestMultipleDrawsHiddenCopyRequest::
                  CopyOutputCallback,
              base::Unretained(this))));
    }
  }

  DrawResult PrepareToDrawOnThread(LayerTreeHostImpl* host_impl,
                                   LayerTreeHostImpl::FrameData* frame_data,
                                   DrawResult draw_result) override {
    LayerImpl* root = host_impl->active_tree()->root_layer_for_testing();
    LayerImpl* child = host_impl->active_tree()->LayerById(child_->id());

    bool saw_root = false;
    bool saw_child = false;
    for (LayerIterator it =
             LayerIterator::Begin(frame_data->render_surface_layer_list);
         it != LayerIterator::End(frame_data->render_surface_layer_list);
         ++it) {
      if (it.represents_itself()) {
        if (*it == root)
          saw_root = true;
        else if (*it == child)
          saw_child = true;
        else
          NOTREACHED();
      }
    }

    ++num_draws_;
    // The first draw has no copy request. The 2nd draw has a copy request, the
    // 3rd should not again.
    switch (num_draws_) {
      case 1:
        // Only the root layer draws, the child is hidden.
        EXPECT_TRUE(saw_root);
        EXPECT_FALSE(saw_child);
        break;
      case 2:
        // Copy happening here, the child will draw.
        EXPECT_TRUE(saw_root);
        EXPECT_TRUE(saw_child);
        // Make another draw happen after doing the copy request.
        host_impl->SetNeedsRedrawRect(gfx::Rect(1, 1));
        break;
      case 3:
        // If LayerTreeHostImpl does the wrong thing, it will try to draw the
        // layer which had a copy request. But only the root should draw.
        EXPECT_TRUE(saw_root);
        EXPECT_FALSE(saw_child);

        // End the test! Don't race with copy request callbacks, so post the end
        // to the main thread.
        draw_happened_ = true;
        MainThreadTaskRunner()->PostTask(
            FROM_HERE,
            base::Bind(
                &LayerTreeHostCopyRequestTestMultipleDrawsHiddenCopyRequest::
                    TryEndTest,
                base::Unretained(this)));
        break;
    }
    return draw_result;
  }

  void CopyOutputCallback(std::unique_ptr<CopyOutputResult> result) {
    EXPECT_FALSE(TestEnded());
    copy_happened_ = true;
    TryEndTest();
  }

  void TryEndTest() {
    if (draw_happened_ && copy_happened_)
      EndTest();
  }

  void AfterTest() override {}

  scoped_refptr<FakePictureLayer> child_;
  FakeContentLayerClient client_;
  int num_draws_;
  bool copy_happened_;
  bool draw_happened_;
};

SINGLE_AND_MULTI_THREAD_DIRECT_RENDERER_TEST_F(
    LayerTreeHostCopyRequestTestMultipleDrawsHiddenCopyRequest);

}  // namespace
}  // namespace cc
