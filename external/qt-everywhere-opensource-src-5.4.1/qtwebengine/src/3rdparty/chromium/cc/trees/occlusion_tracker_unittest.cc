// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/occlusion_tracker.h"

#include "cc/animation/layer_animation_controller.h"
#include "cc/base/math_util.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "cc/output/filter_operation.h"
#include "cc/output/filter_operations.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/test_occlusion_tracker.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/single_thread_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/transform.h"

namespace cc {
namespace {

class TestContentLayer : public Layer {
 public:
  TestContentLayer() : Layer(), override_opaque_contents_rect_(false) {
    SetIsDrawable(true);
  }

  virtual Region VisibleContentOpaqueRegion() const OVERRIDE {
    if (override_opaque_contents_rect_)
      return gfx::IntersectRects(opaque_contents_rect_, visible_content_rect());
    return Layer::VisibleContentOpaqueRegion();
  }
  void SetOpaqueContentsRect(const gfx::Rect& opaque_contents_rect) {
    override_opaque_contents_rect_ = true;
    opaque_contents_rect_ = opaque_contents_rect;
  }

 private:
  virtual ~TestContentLayer() {}

  bool override_opaque_contents_rect_;
  gfx::Rect opaque_contents_rect_;
};

class TestContentLayerImpl : public LayerImpl {
 public:
  TestContentLayerImpl(LayerTreeImpl* tree_impl, int id)
      : LayerImpl(tree_impl, id), override_opaque_contents_rect_(false) {
    SetDrawsContent(true);
  }

  virtual Region VisibleContentOpaqueRegion() const OVERRIDE {
    if (override_opaque_contents_rect_)
      return gfx::IntersectRects(opaque_contents_rect_, visible_content_rect());
    return LayerImpl::VisibleContentOpaqueRegion();
  }
  void SetOpaqueContentsRect(const gfx::Rect& opaque_contents_rect) {
    override_opaque_contents_rect_ = true;
    opaque_contents_rect_ = opaque_contents_rect;
  }

 private:
  bool override_opaque_contents_rect_;
  gfx::Rect opaque_contents_rect_;
};

template <typename LayerType>
class TestOcclusionTrackerWithClip : public TestOcclusionTracker<LayerType> {
 public:
  explicit TestOcclusionTrackerWithClip(const gfx::Rect& viewport_rect)
      : TestOcclusionTracker<LayerType>(viewport_rect) {}

  bool OccludedLayer(const LayerType* layer,
                     const gfx::Rect& content_rect) const {
    DCHECK(layer->visible_content_rect().Contains(content_rect));
    return this->Occluded(
        layer->render_target(), content_rect, layer->draw_transform());
  }

  // Gives an unoccluded sub-rect of |content_rect| in the content space of the
  // layer. Simple wrapper around UnoccludedContentRect.
  gfx::Rect UnoccludedLayerContentRect(const LayerType* layer,
                                       const gfx::Rect& content_rect) const {
    DCHECK(layer->visible_content_rect().Contains(content_rect));
    return this->UnoccludedContentRect(content_rect, layer->draw_transform());
  }

  gfx::Rect UnoccludedSurfaceContentRect(const LayerType* layer,
                                         bool for_replica,
                                         const gfx::Rect& content_rect) const {
    typename LayerType::RenderSurfaceType* surface = layer->render_surface();
    gfx::Transform draw_transform = for_replica
                                        ? surface->replica_draw_transform()
                                        : surface->draw_transform();
    return this->UnoccludedContributingSurfaceContentRect(content_rect,
                                                          draw_transform);
  }
};

struct OcclusionTrackerTestMainThreadTypes {
  typedef Layer LayerType;
  typedef FakeLayerTreeHost HostType;
  typedef RenderSurface RenderSurfaceType;
  typedef TestContentLayer ContentLayerType;
  typedef scoped_refptr<Layer> LayerPtrType;
  typedef scoped_refptr<ContentLayerType> ContentLayerPtrType;
  typedef LayerIterator<Layer> TestLayerIterator;
  typedef OcclusionTracker<Layer> OcclusionTrackerType;

  static LayerPtrType CreateLayer(HostType*  host) { return Layer::Create(); }
  static ContentLayerPtrType CreateContentLayer(HostType* host) {
    return make_scoped_refptr(new ContentLayerType());
  }

  static LayerPtrType PassLayerPtr(ContentLayerPtrType* layer) {
    LayerPtrType ref(*layer);
    *layer = NULL;
    return ref;
  }

  static LayerPtrType PassLayerPtr(LayerPtrType* layer) {
    LayerPtrType ref(*layer);
    *layer = NULL;
    return ref;
  }

  static void DestroyLayer(LayerPtrType* layer) { *layer = NULL; }
};

struct OcclusionTrackerTestImplThreadTypes {
  typedef LayerImpl LayerType;
  typedef LayerTreeImpl HostType;
  typedef RenderSurfaceImpl RenderSurfaceType;
  typedef TestContentLayerImpl ContentLayerType;
  typedef scoped_ptr<LayerImpl> LayerPtrType;
  typedef scoped_ptr<ContentLayerType> ContentLayerPtrType;
  typedef LayerIterator<LayerImpl> TestLayerIterator;
  typedef OcclusionTracker<LayerImpl> OcclusionTrackerType;

  static LayerPtrType CreateLayer(HostType* host) {
    return LayerImpl::Create(host, next_layer_impl_id++);
  }
  static ContentLayerPtrType CreateContentLayer(HostType* host) {
    return make_scoped_ptr(new ContentLayerType(host, next_layer_impl_id++));
  }
  static int next_layer_impl_id;

  static LayerPtrType PassLayerPtr(LayerPtrType* layer) {
    return layer->Pass();
  }

  static LayerPtrType PassLayerPtr(ContentLayerPtrType* layer) {
    return layer->PassAs<LayerType>();
  }

  static void DestroyLayer(LayerPtrType* layer) { layer->reset(); }
};

int OcclusionTrackerTestImplThreadTypes::next_layer_impl_id = 1;

template <typename Types> class OcclusionTrackerTest : public testing::Test {
 protected:
  explicit OcclusionTrackerTest(bool opaque_layers)
      : opaque_layers_(opaque_layers), host_(FakeLayerTreeHost::Create()) {}

  virtual void RunMyTest() = 0;

  virtual void TearDown() {
    Types::DestroyLayer(&root_);
    render_surface_layer_list_.reset();
    render_surface_layer_list_impl_.clear();
    replica_layers_.clear();
    mask_layers_.clear();
  }

  typename Types::HostType* GetHost();

  typename Types::ContentLayerType* CreateRoot(const gfx::Transform& transform,
                                               const gfx::PointF& position,
                                               const gfx::Size& bounds) {
    typename Types::ContentLayerPtrType layer(
        Types::CreateContentLayer(GetHost()));
    typename Types::ContentLayerType* layer_ptr = layer.get();
    SetProperties(layer_ptr, transform, position, bounds);

    DCHECK(!root_.get());
    root_ = Types::PassLayerPtr(&layer);

    SetRootLayerOnMainThread(layer_ptr);

    return layer_ptr;
  }

  typename Types::LayerType* CreateLayer(typename Types::LayerType* parent,
                                         const gfx::Transform& transform,
                                         const gfx::PointF& position,
                                         const gfx::Size& bounds) {
    typename Types::LayerPtrType layer(Types::CreateLayer(GetHost()));
    typename Types::LayerType* layer_ptr = layer.get();
    SetProperties(layer_ptr, transform, position, bounds);
    parent->AddChild(Types::PassLayerPtr(&layer));
    return layer_ptr;
  }

  typename Types::LayerType* CreateSurface(typename Types::LayerType* parent,
                                           const gfx::Transform& transform,
                                           const gfx::PointF& position,
                                           const gfx::Size& bounds) {
    typename Types::LayerType* layer =
        CreateLayer(parent, transform, position, bounds);
    layer->SetForceRenderSurface(true);
    return layer;
  }

  typename Types::ContentLayerType* CreateDrawingLayer(
      typename Types::LayerType* parent,
      const gfx::Transform& transform,
      const gfx::PointF& position,
      const gfx::Size& bounds,
      bool opaque) {
    typename Types::ContentLayerPtrType layer(
        Types::CreateContentLayer(GetHost()));
    typename Types::ContentLayerType* layer_ptr = layer.get();
    SetProperties(layer_ptr, transform, position, bounds);

    if (opaque_layers_) {
      layer_ptr->SetContentsOpaque(opaque);
    } else {
      layer_ptr->SetContentsOpaque(false);
      if (opaque)
        layer_ptr->SetOpaqueContentsRect(gfx::Rect(bounds));
      else
        layer_ptr->SetOpaqueContentsRect(gfx::Rect());
    }

    parent->AddChild(Types::PassLayerPtr(&layer));
    return layer_ptr;
  }

  typename Types::LayerType* CreateReplicaLayer(
      typename Types::LayerType* owning_layer,
      const gfx::Transform& transform,
      const gfx::PointF& position,
      const gfx::Size& bounds) {
    typename Types::ContentLayerPtrType layer(
        Types::CreateContentLayer(GetHost()));
    typename Types::ContentLayerType* layer_ptr = layer.get();
    SetProperties(layer_ptr, transform, position, bounds);
    SetReplica(owning_layer, Types::PassLayerPtr(&layer));
    return layer_ptr;
  }

  typename Types::LayerType* CreateMaskLayer(
      typename Types::LayerType* owning_layer,
      const gfx::Size& bounds) {
    typename Types::ContentLayerPtrType layer(
        Types::CreateContentLayer(GetHost()));
    typename Types::ContentLayerType* layer_ptr = layer.get();
    SetProperties(layer_ptr, identity_matrix, gfx::PointF(), bounds);
    SetMask(owning_layer, Types::PassLayerPtr(&layer));
    return layer_ptr;
  }

  typename Types::ContentLayerType* CreateDrawingSurface(
      typename Types::LayerType* parent,
      const gfx::Transform& transform,
      const gfx::PointF& position,
      const gfx::Size& bounds,
      bool opaque) {
    typename Types::ContentLayerType* layer =
        CreateDrawingLayer(parent, transform, position, bounds, opaque);
    layer->SetForceRenderSurface(true);
    return layer;
  }


  void CopyOutputCallback(scoped_ptr<CopyOutputResult> result) {}

  void AddCopyRequest(Layer* layer) {
    layer->RequestCopyOfOutput(
        CopyOutputRequest::CreateBitmapRequest(base::Bind(
            &OcclusionTrackerTest<Types>::CopyOutputCallback,
            base::Unretained(this))));
  }

  void AddCopyRequest(LayerImpl* layer) {
    ScopedPtrVector<CopyOutputRequest> requests;
    requests.push_back(
        CopyOutputRequest::CreateBitmapRequest(base::Bind(
            &OcclusionTrackerTest<Types>::CopyOutputCallback,
            base::Unretained(this))));
    layer->PassCopyRequests(&requests);
  }

  void CalcDrawEtc(TestContentLayerImpl* root) {
    DCHECK(root == root_.get());
    DCHECK(!root->render_surface());

    LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
        root, root->bounds(), &render_surface_layer_list_impl_);
    inputs.can_adjust_raster_scales = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);

    layer_iterator_ = layer_iterator_begin_ =
        Types::TestLayerIterator::Begin(&render_surface_layer_list_impl_);
  }

  void CalcDrawEtc(TestContentLayer* root) {
    DCHECK(root == root_.get());
    DCHECK(!root->render_surface());

    render_surface_layer_list_.reset(new RenderSurfaceLayerList);
    LayerTreeHostCommon::CalcDrawPropsMainInputsForTesting inputs(
        root, root->bounds(), render_surface_layer_list_.get());
    inputs.can_adjust_raster_scales = true;
    LayerTreeHostCommon::CalculateDrawProperties(&inputs);

    layer_iterator_ = layer_iterator_begin_ =
        Types::TestLayerIterator::Begin(render_surface_layer_list_.get());
  }

  void EnterLayer(typename Types::LayerType* layer,
                  typename Types::OcclusionTrackerType* occlusion) {
    ASSERT_EQ(layer, *layer_iterator_);
    ASSERT_TRUE(layer_iterator_.represents_itself());
    occlusion->EnterLayer(layer_iterator_);
  }

  void LeaveLayer(typename Types::LayerType* layer,
                  typename Types::OcclusionTrackerType* occlusion) {
    ASSERT_EQ(layer, *layer_iterator_);
    ASSERT_TRUE(layer_iterator_.represents_itself());
    occlusion->LeaveLayer(layer_iterator_);
    ++layer_iterator_;
  }

  void VisitLayer(typename Types::LayerType* layer,
                  typename Types::OcclusionTrackerType* occlusion) {
    EnterLayer(layer, occlusion);
    LeaveLayer(layer, occlusion);
  }

  void EnterContributingSurface(
      typename Types::LayerType* layer,
      typename Types::OcclusionTrackerType* occlusion) {
    ASSERT_EQ(layer, *layer_iterator_);
    ASSERT_TRUE(layer_iterator_.represents_target_render_surface());
    occlusion->EnterLayer(layer_iterator_);
    occlusion->LeaveLayer(layer_iterator_);
    ++layer_iterator_;
    ASSERT_TRUE(layer_iterator_.represents_contributing_render_surface());
    occlusion->EnterLayer(layer_iterator_);
  }

  void LeaveContributingSurface(
      typename Types::LayerType* layer,
      typename Types::OcclusionTrackerType* occlusion) {
    ASSERT_EQ(layer, *layer_iterator_);
    ASSERT_TRUE(layer_iterator_.represents_contributing_render_surface());
    occlusion->LeaveLayer(layer_iterator_);
    ++layer_iterator_;
  }

  void VisitContributingSurface(
      typename Types::LayerType* layer,
      typename Types::OcclusionTrackerType* occlusion) {
    EnterContributingSurface(layer, occlusion);
    LeaveContributingSurface(layer, occlusion);
  }

  void ResetLayerIterator() { layer_iterator_ = layer_iterator_begin_; }

  const gfx::Transform identity_matrix;

 private:
  void SetRootLayerOnMainThread(Layer* root) {
    host_->SetRootLayer(scoped_refptr<Layer>(root));
  }

  void SetRootLayerOnMainThread(LayerImpl* root) {}

  void SetBaseProperties(typename Types::LayerType* layer,
                         const gfx::Transform& transform,
                         const gfx::PointF& position,
                         const gfx::Size& bounds) {
    layer->SetTransform(transform);
    layer->SetPosition(position);
    layer->SetBounds(bounds);
  }

  void SetProperties(Layer* layer,
                     const gfx::Transform& transform,
                     const gfx::PointF& position,
                     const gfx::Size& bounds) {
    SetBaseProperties(layer, transform, position, bounds);
  }

  void SetProperties(LayerImpl* layer,
                     const gfx::Transform& transform,
                     const gfx::PointF& position,
                     const gfx::Size& bounds) {
    SetBaseProperties(layer, transform, position, bounds);

    layer->SetContentBounds(layer->bounds());
  }

  void SetReplica(Layer* owning_layer, scoped_refptr<Layer> layer) {
    owning_layer->SetReplicaLayer(layer.get());
    replica_layers_.push_back(layer);
  }

  void SetReplica(LayerImpl* owning_layer, scoped_ptr<LayerImpl> layer) {
    owning_layer->SetReplicaLayer(layer.Pass());
  }

  void SetMask(Layer* owning_layer, scoped_refptr<Layer> layer) {
    owning_layer->SetMaskLayer(layer.get());
    mask_layers_.push_back(layer);
  }

  void SetMask(LayerImpl* owning_layer, scoped_ptr<LayerImpl> layer) {
    owning_layer->SetMaskLayer(layer.Pass());
  }

  bool opaque_layers_;
  scoped_ptr<FakeLayerTreeHost> host_;
  // These hold ownership of the layers for the duration of the test.
  typename Types::LayerPtrType root_;
  scoped_ptr<RenderSurfaceLayerList> render_surface_layer_list_;
  LayerImplList render_surface_layer_list_impl_;
  typename Types::TestLayerIterator layer_iterator_begin_;
  typename Types::TestLayerIterator layer_iterator_;
  typename Types::LayerType* last_layer_visited_;
  LayerList replica_layers_;
  LayerList mask_layers_;
};

template <>
FakeLayerTreeHost*
OcclusionTrackerTest<OcclusionTrackerTestMainThreadTypes>::GetHost() {
  return host_.get();
}

template <>
LayerTreeImpl*
OcclusionTrackerTest<OcclusionTrackerTestImplThreadTypes>::GetHost() {
  return host_->host_impl()->active_tree();
}

#define RUN_TEST_MAIN_THREAD_OPAQUE_LAYERS(ClassName)                          \
  class ClassName##MainThreadOpaqueLayers                                      \
      : public ClassName<OcclusionTrackerTestMainThreadTypes> {                \
   public: /* NOLINT(whitespace/indent) */                                     \
    ClassName##MainThreadOpaqueLayers()                                        \
        : ClassName<OcclusionTrackerTestMainThreadTypes>(true) {}              \
  };                                                                           \
  TEST_F(ClassName##MainThreadOpaqueLayers, RunTest) { RunMyTest(); }
#define RUN_TEST_MAIN_THREAD_OPAQUE_PAINTS(ClassName)                          \
  class ClassName##MainThreadOpaquePaints                                      \
      : public ClassName<OcclusionTrackerTestMainThreadTypes> {                \
   public: /* NOLINT(whitespace/indent) */                                     \
    ClassName##MainThreadOpaquePaints()                                        \
        : ClassName<OcclusionTrackerTestMainThreadTypes>(false) {}             \
  };                                                                           \
  TEST_F(ClassName##MainThreadOpaquePaints, RunTest) { RunMyTest(); }

#define RUN_TEST_IMPL_THREAD_OPAQUE_LAYERS(ClassName)                          \
  class ClassName##ImplThreadOpaqueLayers                                      \
      : public ClassName<OcclusionTrackerTestImplThreadTypes> {                \
   public: /* NOLINT(whitespace/indent) */                                     \
    ClassName##ImplThreadOpaqueLayers()                                        \
        : ClassName<OcclusionTrackerTestImplThreadTypes>(true) {}              \
  };                                                                           \
  TEST_F(ClassName##ImplThreadOpaqueLayers, RunTest) { RunMyTest(); }
#define RUN_TEST_IMPL_THREAD_OPAQUE_PAINTS(ClassName)                          \
  class ClassName##ImplThreadOpaquePaints                                      \
      : public ClassName<OcclusionTrackerTestImplThreadTypes> {                \
   public: /* NOLINT(whitespace/indent) */                                     \
    ClassName##ImplThreadOpaquePaints()                                        \
        : ClassName<OcclusionTrackerTestImplThreadTypes>(false) {}             \
  };                                                                           \
  TEST_F(ClassName##ImplThreadOpaquePaints, RunTest) { RunMyTest(); }

#define ALL_OCCLUSIONTRACKER_TEST(ClassName)                                   \
  RUN_TEST_MAIN_THREAD_OPAQUE_LAYERS(ClassName)                                \
      RUN_TEST_MAIN_THREAD_OPAQUE_PAINTS(ClassName)                            \
      RUN_TEST_IMPL_THREAD_OPAQUE_LAYERS(ClassName)                            \
      RUN_TEST_IMPL_THREAD_OPAQUE_PAINTS(ClassName)

#define MAIN_THREAD_TEST(ClassName)                                            \
  RUN_TEST_MAIN_THREAD_OPAQUE_LAYERS(ClassName)

#define IMPL_THREAD_TEST(ClassName)                                            \
  RUN_TEST_IMPL_THREAD_OPAQUE_LAYERS(ClassName)

#define MAIN_AND_IMPL_THREAD_TEST(ClassName)                                   \
  RUN_TEST_MAIN_THREAD_OPAQUE_LAYERS(ClassName)                                \
      RUN_TEST_IMPL_THREAD_OPAQUE_LAYERS(ClassName)

template <class Types>
class OcclusionTrackerTestIdentityTransforms
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestIdentityTransforms(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}

  void RunMyTest() {
    typename Types::ContentLayerType* root = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(200, 200));
    typename Types::ContentLayerType* parent = this->CreateDrawingLayer(
        root, this->identity_matrix, gfx::PointF(), gfx::Size(100, 100), true);
    typename Types::ContentLayerType* layer =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(30.f, 30.f),
                                 gfx::Size(500, 500),
                                 true);
    parent->SetMasksToBounds(true);
    this->CalcDrawEtc(root);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(layer, &occlusion);
    this->EnterLayer(parent, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(30, 30, 70, 70).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    EXPECT_TRUE(occlusion.OccludedLayer(parent, gfx::Rect(30, 30, 70, 70)));
    EXPECT_FALSE(occlusion.OccludedLayer(parent, gfx::Rect(29, 30, 70, 70)));
    EXPECT_FALSE(occlusion.OccludedLayer(parent, gfx::Rect(30, 29, 70, 70)));
    EXPECT_TRUE(occlusion.OccludedLayer(parent, gfx::Rect(31, 30, 69, 70)));
    EXPECT_TRUE(occlusion.OccludedLayer(parent, gfx::Rect(30, 31, 70, 69)));

    EXPECT_TRUE(occlusion.UnoccludedLayerContentRect(
        parent, gfx::Rect(30, 30, 70, 70)).IsEmpty());
    EXPECT_RECT_EQ(gfx::Rect(29, 30, 1, 70),
                   occlusion.UnoccludedLayerContentRect(
                       parent, gfx::Rect(29, 30, 70, 70)));
    EXPECT_RECT_EQ(gfx::Rect(29, 29, 70, 70),
                   occlusion.UnoccludedLayerContentRect(
                       parent, gfx::Rect(29, 29, 70, 70)));
    EXPECT_RECT_EQ(gfx::Rect(30, 29, 70, 1),
                   occlusion.UnoccludedLayerContentRect(
                       parent, gfx::Rect(30, 29, 70, 70)));
    EXPECT_RECT_EQ(gfx::Rect(31, 29, 69, 1),
                   occlusion.UnoccludedLayerContentRect(
                       parent, gfx::Rect(31, 29, 69, 70)));
    EXPECT_RECT_EQ(gfx::Rect(),
                   occlusion.UnoccludedLayerContentRect(
                       parent, gfx::Rect(31, 30, 69, 70)));
    EXPECT_RECT_EQ(gfx::Rect(),
                   occlusion.UnoccludedLayerContentRect(
                       parent, gfx::Rect(31, 31, 69, 69)));
    EXPECT_RECT_EQ(gfx::Rect(),
                   occlusion.UnoccludedLayerContentRect(
                       parent, gfx::Rect(30, 31, 70, 69)));
    EXPECT_RECT_EQ(gfx::Rect(29, 31, 1, 69),
                   occlusion.UnoccludedLayerContentRect(
                       parent, gfx::Rect(29, 31, 70, 69)));
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestIdentityTransforms);

template <class Types>
class OcclusionTrackerTestQuadsMismatchLayer
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestQuadsMismatchLayer(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() {
    gfx::Transform layer_transform;
    layer_transform.Translate(10.0, 10.0);

    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::Point(0, 0), gfx::Size(100, 100));
    typename Types::ContentLayerType* layer1 = this->CreateDrawingLayer(
        parent, layer_transform, gfx::PointF(), gfx::Size(90, 90), true);
    typename Types::ContentLayerType* layer2 = this->CreateDrawingLayer(
        layer1, layer_transform, gfx::PointF(), gfx::Size(50, 50), true);
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(layer2, &occlusion);
    this->EnterLayer(layer1, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(20, 20, 50, 50).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    // This checks cases where the quads don't match their "containing"
    // layers, e.g. in terms of transforms or clip rect. This is typical for
    // DelegatedRendererLayer.

    gfx::Transform quad_transform;
    quad_transform.Translate(30.0, 30.0);

    EXPECT_TRUE(occlusion.UnoccludedContentRect(gfx::Rect(0, 0, 10, 10),
                                                quad_transform).IsEmpty());
    EXPECT_RECT_EQ(gfx::Rect(40, 40, 10, 10),
                   occlusion.UnoccludedContentRect(gfx::Rect(40, 40, 10, 10),
                                                   quad_transform));
    EXPECT_RECT_EQ(gfx::Rect(40, 30, 5, 10),
                   occlusion.UnoccludedContentRect(gfx::Rect(35, 30, 10, 10),
                                                   quad_transform));
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestQuadsMismatchLayer);

template <class Types>
class OcclusionTrackerTestRotatedChild : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestRotatedChild(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() {
    gfx::Transform layer_transform;
    layer_transform.Translate(250.0, 250.0);
    layer_transform.Rotate(90.0);
    layer_transform.Translate(-250.0, -250.0);

    typename Types::ContentLayerType* root = this->CreateRoot(
        this->identity_matrix, gfx::Point(0, 0), gfx::Size(200, 200));
    typename Types::ContentLayerType* parent = this->CreateDrawingLayer(
        root, this->identity_matrix, gfx::PointF(), gfx::Size(100, 100), true);
    typename Types::ContentLayerType* layer =
        this->CreateDrawingLayer(parent,
                                 layer_transform,
                                 gfx::PointF(30.f, 30.f),
                                 gfx::Size(500, 500),
                                 true);
    parent->SetMasksToBounds(true);
    this->CalcDrawEtc(root);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(layer, &occlusion);
    this->EnterLayer(parent, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(30, 30, 70, 70).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    EXPECT_TRUE(occlusion.OccludedLayer(parent, gfx::Rect(30, 30, 70, 70)));
    EXPECT_FALSE(occlusion.OccludedLayer(parent, gfx::Rect(29, 30, 70, 70)));
    EXPECT_FALSE(occlusion.OccludedLayer(parent, gfx::Rect(30, 29, 70, 70)));
    EXPECT_TRUE(occlusion.OccludedLayer(parent, gfx::Rect(31, 30, 69, 70)));
    EXPECT_TRUE(occlusion.OccludedLayer(parent, gfx::Rect(30, 31, 70, 69)));

    EXPECT_TRUE(occlusion.UnoccludedLayerContentRect(
        parent, gfx::Rect(30, 30, 70, 70)).IsEmpty());
    EXPECT_RECT_EQ(gfx::Rect(29, 30, 1, 70),
                   occlusion.UnoccludedLayerContentRect(
                       parent, gfx::Rect(29, 30, 69, 70)));
    EXPECT_RECT_EQ(gfx::Rect(29, 29, 70, 70),
                   occlusion.UnoccludedLayerContentRect(
                       parent, gfx::Rect(29, 29, 70, 70)));
    EXPECT_RECT_EQ(gfx::Rect(30, 29, 70, 1),
                   occlusion.UnoccludedLayerContentRect(
                       parent, gfx::Rect(30, 29, 70, 70)));
    EXPECT_RECT_EQ(gfx::Rect(31, 29, 69, 1),
                   occlusion.UnoccludedLayerContentRect(
                       parent, gfx::Rect(31, 29, 69, 70)));
    EXPECT_RECT_EQ(gfx::Rect(),
                   occlusion.UnoccludedLayerContentRect(
                       parent, gfx::Rect(31, 30, 69, 70)));
    EXPECT_RECT_EQ(gfx::Rect(),
                   occlusion.UnoccludedLayerContentRect(
                       parent, gfx::Rect(31, 31, 69, 69)));
    EXPECT_RECT_EQ(gfx::Rect(),
                   occlusion.UnoccludedLayerContentRect(
                       parent, gfx::Rect(30, 31, 70, 69)));
    EXPECT_RECT_EQ(gfx::Rect(29, 31, 1, 69),
                   occlusion.UnoccludedLayerContentRect(
                       parent, gfx::Rect(29, 31, 70, 69)));
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestRotatedChild);

template <class Types>
class OcclusionTrackerTestTranslatedChild : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestTranslatedChild(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() {
    gfx::Transform layer_transform;
    layer_transform.Translate(20.0, 20.0);

    typename Types::ContentLayerType* root = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(200, 200));
    typename Types::ContentLayerType* parent = this->CreateDrawingLayer(
        root, this->identity_matrix, gfx::PointF(), gfx::Size(100, 100), true);
    typename Types::ContentLayerType* layer =
        this->CreateDrawingLayer(parent,
                                 layer_transform,
                                 gfx::PointF(30.f, 30.f),
                                 gfx::Size(500, 500),
                                 true);
    parent->SetMasksToBounds(true);
    this->CalcDrawEtc(root);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(layer, &occlusion);
    this->EnterLayer(parent, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(50, 50, 50, 50).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    EXPECT_TRUE(occlusion.OccludedLayer(parent, gfx::Rect(50, 50, 50, 50)));
    EXPECT_FALSE(occlusion.OccludedLayer(parent, gfx::Rect(49, 50, 50, 50)));
    EXPECT_FALSE(occlusion.OccludedLayer(parent, gfx::Rect(50, 49, 50, 50)));
    EXPECT_TRUE(occlusion.OccludedLayer(parent, gfx::Rect(51, 50, 49, 50)));
    EXPECT_TRUE(occlusion.OccludedLayer(parent, gfx::Rect(50, 51, 50, 49)));

    EXPECT_TRUE(occlusion.UnoccludedLayerContentRect(
        parent, gfx::Rect(50, 50, 50, 50)).IsEmpty());
    EXPECT_RECT_EQ(gfx::Rect(49, 50, 1, 50),
                   occlusion.UnoccludedLayerContentRect(
                       parent, gfx::Rect(49, 50, 50, 50)));
    EXPECT_RECT_EQ(gfx::Rect(49, 49, 50, 50),
                   occlusion.UnoccludedLayerContentRect(
                       parent, gfx::Rect(49, 49, 50, 50)));
    EXPECT_RECT_EQ(gfx::Rect(50, 49, 50, 1),
                   occlusion.UnoccludedLayerContentRect(
                       parent, gfx::Rect(50, 49, 50, 50)));
    EXPECT_RECT_EQ(gfx::Rect(51, 49, 49, 1),
                   occlusion.UnoccludedLayerContentRect(
                       parent, gfx::Rect(51, 49, 49, 50)));
    EXPECT_TRUE(occlusion.UnoccludedLayerContentRect(
        parent, gfx::Rect(51, 50, 49, 50)).IsEmpty());
    EXPECT_TRUE(occlusion.UnoccludedLayerContentRect(
        parent, gfx::Rect(51, 51, 49, 49)).IsEmpty());
    EXPECT_TRUE(occlusion.UnoccludedLayerContentRect(
        parent, gfx::Rect(50, 51, 50, 49)).IsEmpty());
    EXPECT_RECT_EQ(gfx::Rect(49, 51, 1, 49),
                   occlusion.UnoccludedLayerContentRect(
                       parent, gfx::Rect(49, 51, 50, 49)));
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestTranslatedChild);

template <class Types>
class OcclusionTrackerTestChildInRotatedChild
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestChildInRotatedChild(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() {
    gfx::Transform child_transform;
    child_transform.Translate(250.0, 250.0);
    child_transform.Rotate(90.0);
    child_transform.Translate(-250.0, -250.0);

    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(100, 100));
    parent->SetMasksToBounds(true);
    typename Types::LayerType* child = this->CreateSurface(
        parent, child_transform, gfx::PointF(30.f, 30.f), gfx::Size(500, 500));
    child->SetMasksToBounds(true);
    typename Types::ContentLayerType* layer =
        this->CreateDrawingLayer(child,
                                 this->identity_matrix,
                                 gfx::PointF(10.f, 10.f),
                                 gfx::Size(500, 500),
                                 true);
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(layer, &occlusion);
    this->EnterContributingSurface(child, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(10, 430, 60, 70).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->LeaveContributingSurface(child, &occlusion);
    this->EnterLayer(parent, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(30, 40, 70, 60).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    EXPECT_TRUE(occlusion.OccludedLayer(parent, gfx::Rect(30, 40, 70, 60)));
    EXPECT_FALSE(occlusion.OccludedLayer(parent, gfx::Rect(29, 40, 70, 60)));
    EXPECT_FALSE(occlusion.OccludedLayer(parent, gfx::Rect(30, 39, 70, 60)));
    EXPECT_TRUE(occlusion.OccludedLayer(parent, gfx::Rect(31, 40, 69, 60)));
    EXPECT_TRUE(occlusion.OccludedLayer(parent, gfx::Rect(30, 41, 70, 59)));

    /* Justification for the above occlusion from |layer|:
                  100
         +---------------------+
         |                     |
         |    30               |           rotate(90)
         | 30 + ---------------------------------+
     100 |    |  10            |                 |            ==>
         |    |10+---------------------------------+
         |    |  |             |                 | |
         |    |  |             |                 | |
         |    |  |             |                 | |
         +----|--|-------------+                 | |
              |  |                               | |
              |  |                               | |
              |  |                               | |500
              |  |                               | |
              |  |                               | |
              |  |                               | |
              |  |                               | |
              +--|-------------------------------+ |
                 |                                 |
                 +---------------------------------+
                                500

        +---------------------+
        |                     |30  Visible region of |layer|: /////
        |                     |
        |     +---------------------------------+
     100|     |               |10               |
        |  +---------------------------------+  |
        |  |  |///////////////|     420      |  |
        |  |  |///////////////|60            |  |
        |  |  |///////////////|              |  |
        +--|--|---------------+              |  |
         20|10|     70                       |  |
           |  |                              |  |
           |  |                              |  |
           |  |                              |  |
           |  |                              |  |
           |  |                              |  |
           |  |                              |10|
           |  +------------------------------|--+
           |                 490             |
           +---------------------------------+
                          500

     */
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestChildInRotatedChild);

template <class Types>
class OcclusionTrackerTestScaledRenderSurface
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestScaledRenderSurface(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}

  void RunMyTest() {
    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(200, 200));

    gfx::Transform layer1_matrix;
    layer1_matrix.Scale(2.0, 2.0);
    typename Types::ContentLayerType* layer1 = this->CreateDrawingLayer(
        parent, layer1_matrix, gfx::PointF(), gfx::Size(100, 100), true);
    layer1->SetForceRenderSurface(true);

    gfx::Transform layer2_matrix;
    layer2_matrix.Translate(25.0, 25.0);
    typename Types::ContentLayerType* layer2 = this->CreateDrawingLayer(
        layer1, layer2_matrix, gfx::PointF(), gfx::Size(50, 50), true);
    typename Types::ContentLayerType* occluder =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(100.f, 100.f),
                                 gfx::Size(500, 500),
                                 true);
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(occluder, &occlusion);
    this->EnterLayer(layer2, &occlusion);

    EXPECT_EQ(gfx::Rect(100, 100, 100, 100).ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    EXPECT_RECT_EQ(
        gfx::Rect(0, 0, 25, 25),
        occlusion.UnoccludedLayerContentRect(layer2, gfx::Rect(0, 0, 25, 25)));
    EXPECT_RECT_EQ(gfx::Rect(10, 25, 15, 25),
                   occlusion.UnoccludedLayerContentRect(
                       layer2, gfx::Rect(10, 25, 25, 25)));
    EXPECT_RECT_EQ(gfx::Rect(25, 10, 25, 15),
                   occlusion.UnoccludedLayerContentRect(
                       layer2, gfx::Rect(25, 10, 25, 25)));
    EXPECT_TRUE(occlusion.UnoccludedLayerContentRect(
        layer2, gfx::Rect(25, 25, 25, 25)).IsEmpty());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestScaledRenderSurface);

template <class Types>
class OcclusionTrackerTestVisitTargetTwoTimes
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestVisitTargetTwoTimes(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() {
    gfx::Transform child_transform;
    child_transform.Translate(250.0, 250.0);
    child_transform.Rotate(90.0);
    child_transform.Translate(-250.0, -250.0);

    typename Types::ContentLayerType* root = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(200, 200));
    typename Types::ContentLayerType* parent = this->CreateDrawingLayer(
        root, this->identity_matrix, gfx::PointF(), gfx::Size(100, 100), true);
    parent->SetMasksToBounds(true);
    typename Types::LayerType* child = this->CreateSurface(
        parent, child_transform, gfx::PointF(30.f, 30.f), gfx::Size(500, 500));
    child->SetMasksToBounds(true);
    typename Types::ContentLayerType* layer =
        this->CreateDrawingLayer(child,
                                 this->identity_matrix,
                                 gfx::PointF(10.f, 10.f),
                                 gfx::Size(500, 500),
                                 true);
    // |child2| makes |parent|'s surface get considered by OcclusionTracker
    // first, instead of |child|'s. This exercises different code in
    // LeaveToRenderTarget, as the target surface has already been seen.
    typename Types::ContentLayerType* child2 =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(30.f, 30.f),
                                 gfx::Size(60, 20),
                                 true);
    this->CalcDrawEtc(root);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(child2, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(30, 30, 60, 20).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->VisitLayer(layer, &occlusion);

    EXPECT_EQ(gfx::Rect(0, 440, 20, 60).ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(10, 430, 60, 70).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->EnterContributingSurface(child, &occlusion);

    EXPECT_EQ(gfx::Rect(0, 440, 20, 60).ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(10, 430, 60, 70).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    // Occlusion in |child2| should get merged with the |child| surface we are
    // leaving now.
    this->LeaveContributingSurface(child, &occlusion);
    this->EnterLayer(parent, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(UnionRegions(gfx::Rect(30, 30, 60, 10), gfx::Rect(30, 40, 70, 60))
                  .ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    EXPECT_FALSE(occlusion.OccludedLayer(parent, gfx::Rect(30, 30, 70, 70)));
    EXPECT_RECT_EQ(gfx::Rect(90, 30, 10, 10),
                   occlusion.UnoccludedLayerContentRect(
                       parent, gfx::Rect(30, 30, 70, 70)));

    EXPECT_TRUE(occlusion.OccludedLayer(parent, gfx::Rect(30, 30, 60, 10)));
    EXPECT_FALSE(occlusion.OccludedLayer(parent, gfx::Rect(29, 30, 60, 10)));
    EXPECT_FALSE(occlusion.OccludedLayer(parent, gfx::Rect(30, 29, 60, 10)));
    EXPECT_FALSE(occlusion.OccludedLayer(parent, gfx::Rect(31, 30, 60, 10)));
    EXPECT_TRUE(occlusion.OccludedLayer(parent, gfx::Rect(30, 31, 60, 10)));

    EXPECT_TRUE(occlusion.OccludedLayer(parent, gfx::Rect(30, 40, 70, 60)));
    EXPECT_FALSE(occlusion.OccludedLayer(parent, gfx::Rect(29, 40, 70, 60)));
    EXPECT_FALSE(occlusion.OccludedLayer(parent, gfx::Rect(30, 39, 70, 60)));

    EXPECT_TRUE(occlusion.UnoccludedLayerContentRect(
        parent, gfx::Rect(30, 30, 60, 10)).IsEmpty());
    EXPECT_RECT_EQ(gfx::Rect(29, 30, 1, 10),
                   occlusion.UnoccludedLayerContentRect(
                       parent, gfx::Rect(29, 30, 60, 10)));
    EXPECT_RECT_EQ(gfx::Rect(30, 29, 60, 1),
                   occlusion.UnoccludedLayerContentRect(
                       parent, gfx::Rect(30, 29, 60, 10)));
    EXPECT_RECT_EQ(gfx::Rect(90, 30, 1, 10),
                   occlusion.UnoccludedLayerContentRect(
                       parent, gfx::Rect(31, 30, 60, 10)));
    EXPECT_TRUE(occlusion.UnoccludedLayerContentRect(
        parent, gfx::Rect(30, 31, 60, 10)).IsEmpty());

    EXPECT_TRUE(occlusion.UnoccludedLayerContentRect(
        parent, gfx::Rect(30, 40, 70, 60)).IsEmpty());
    EXPECT_RECT_EQ(gfx::Rect(29, 40, 1, 60),
                   occlusion.UnoccludedLayerContentRect(
                       parent, gfx::Rect(29, 40, 70, 60)));
    // This rect is mostly occluded by |child2|.
    EXPECT_RECT_EQ(gfx::Rect(90, 39, 10, 1),
                   occlusion.UnoccludedLayerContentRect(
                       parent, gfx::Rect(30, 39, 70, 60)));
    // This rect extends past top/right ends of |child2|.
    EXPECT_RECT_EQ(gfx::Rect(30, 29, 70, 11),
                   occlusion.UnoccludedLayerContentRect(
                       parent, gfx::Rect(30, 29, 70, 70)));
    // This rect extends past left/right ends of |child2|.
    EXPECT_RECT_EQ(gfx::Rect(20, 39, 80, 60),
                   occlusion.UnoccludedLayerContentRect(
                       parent, gfx::Rect(20, 39, 80, 60)));
    EXPECT_RECT_EQ(gfx::Rect(),
                   occlusion.UnoccludedLayerContentRect(
                       parent, gfx::Rect(31, 40, 69, 60)));
    EXPECT_RECT_EQ(gfx::Rect(),
                   occlusion.UnoccludedLayerContentRect(
                       parent, gfx::Rect(30, 41, 70, 59)));

    /* Justification for the above occlusion from |layer|:
               100
      +---------------------+
      |                     |
      |    30               |           rotate(90)
      | 30 + ------------+--------------------+
  100 |    |  10         |  |                 |            ==>
      |    |10+----------|----------------------+
      |    + ------------+  |                 | |
      |    |  |             |                 | |
      |    |  |             |                 | |
      +----|--|-------------+                 | |
           |  |                               | |
           |  |                               | |
           |  |                               | |500
           |  |                               | |
           |  |                               | |
           |  |                               | |
           |  |                               | |
           +--|-------------------------------+ |
              |                                 |
              +---------------------------------+
                             500


       +---------------------+
       |                     |30  Visible region of |layer|: /////
       |     30   60         |    |child2|: \\\\\
       |  30 +------------+--------------------+
       |     |\\\\\\\\\\\\|  |10               |
       |  +--|\\\\\\\\\\\\|-----------------+  |
       |  |  +------------+//|     420      |  |
       |  |  |///////////////|60            |  |
       |  |  |///////////////|              |  |
       +--|--|---------------+              |  |
        20|10|     70                       |  |
          |  |                              |  |
          |  |                              |  |
          |  |                              |  |
          |  |                              |  |
          |  |                              |  |
          |  |                              |10|
          |  +------------------------------|--+
          |                 490             |
          +---------------------------------+
                         500
     */
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestVisitTargetTwoTimes);

template <class Types>
class OcclusionTrackerTestSurfaceRotatedOffAxis
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestSurfaceRotatedOffAxis(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() {
    gfx::Transform child_transform;
    child_transform.Translate(250.0, 250.0);
    child_transform.Rotate(95.0);
    child_transform.Translate(-250.0, -250.0);

    gfx::Transform layer_transform;
    layer_transform.Translate(10.0, 10.0);

    typename Types::ContentLayerType* root = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(1000, 1000));
    typename Types::ContentLayerType* parent = this->CreateDrawingLayer(
        root, this->identity_matrix, gfx::PointF(), gfx::Size(100, 100), true);
    typename Types::LayerType* child = this->CreateLayer(
        parent, child_transform, gfx::PointF(30.f, 30.f), gfx::Size(500, 500));
    child->SetMasksToBounds(true);
    typename Types::ContentLayerType* layer = this->CreateDrawingLayer(
        child, layer_transform, gfx::PointF(), gfx::Size(500, 500), true);
    this->CalcDrawEtc(root);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    gfx::Rect clipped_layer_in_child = MathUtil::MapEnclosingClippedRect(
        layer_transform, layer->visible_content_rect());

    this->VisitLayer(layer, &occlusion);
    this->EnterContributingSurface(child, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(clipped_layer_in_child.ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->LeaveContributingSurface(child, &occlusion);
    this->EnterLayer(parent, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    EXPECT_FALSE(occlusion.OccludedLayer(parent, gfx::Rect(75, 55, 1, 1)));
    EXPECT_RECT_EQ(
        gfx::Rect(75, 55, 1, 1),
        occlusion.UnoccludedLayerContentRect(parent, gfx::Rect(75, 55, 1, 1)));
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestSurfaceRotatedOffAxis);

template <class Types>
class OcclusionTrackerTestSurfaceWithTwoOpaqueChildren
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestSurfaceWithTwoOpaqueChildren(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() {
    gfx::Transform child_transform;
    child_transform.Translate(250.0, 250.0);
    child_transform.Rotate(90.0);
    child_transform.Translate(-250.0, -250.0);

    typename Types::ContentLayerType* root = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(1000, 1000));
    typename Types::ContentLayerType* parent = this->CreateDrawingLayer(
        root, this->identity_matrix, gfx::PointF(), gfx::Size(100, 100), true);
    parent->SetMasksToBounds(true);
    typename Types::ContentLayerType* child =
        this->CreateDrawingSurface(parent,
                                 child_transform,
                                 gfx::PointF(30.f, 30.f),
                                 gfx::Size(500, 500),
                                 false);
    child->SetMasksToBounds(true);
    typename Types::ContentLayerType* layer1 =
        this->CreateDrawingLayer(child,
                                 this->identity_matrix,
                                 gfx::PointF(10.f, 10.f),
                                 gfx::Size(500, 500),
                                 true);
    typename Types::ContentLayerType* layer2 =
        this->CreateDrawingLayer(child,
                                 this->identity_matrix,
                                 gfx::PointF(10.f, 450.f),
                                 gfx::Size(500, 60),
                                 true);
    this->CalcDrawEtc(root);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(layer2, &occlusion);
    this->VisitLayer(layer1, &occlusion);
    this->VisitLayer(child, &occlusion);
    this->EnterContributingSurface(child, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(10, 430, 60, 70).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    EXPECT_TRUE(occlusion.OccludedLayer(child, gfx::Rect(10, 430, 60, 70)));
    EXPECT_FALSE(occlusion.OccludedLayer(child, gfx::Rect(9, 430, 60, 70)));
    EXPECT_TRUE(occlusion.OccludedLayer(child, gfx::Rect(11, 430, 59, 70)));
    EXPECT_TRUE(occlusion.OccludedLayer(child, gfx::Rect(10, 431, 60, 69)));

    EXPECT_TRUE(occlusion.UnoccludedLayerContentRect(
        child, gfx::Rect(10, 430, 60, 70)).IsEmpty());
    EXPECT_RECT_EQ(
        gfx::Rect(9, 430, 1, 70),
        occlusion.UnoccludedLayerContentRect(child, gfx::Rect(9, 430, 60, 70)));
    EXPECT_RECT_EQ(gfx::Rect(),
                   occlusion.UnoccludedLayerContentRect(
                       child, gfx::Rect(11, 430, 59, 70)));
    EXPECT_RECT_EQ(gfx::Rect(),
                   occlusion.UnoccludedLayerContentRect(
                       child, gfx::Rect(10, 431, 60, 69)));

    this->LeaveContributingSurface(child, &occlusion);
    this->EnterLayer(parent, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(30, 40, 70, 60).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    EXPECT_TRUE(occlusion.OccludedLayer(parent, gfx::Rect(30, 40, 70, 60)));
    EXPECT_FALSE(occlusion.OccludedLayer(parent, gfx::Rect(29, 40, 70, 60)));
    EXPECT_FALSE(occlusion.OccludedLayer(parent, gfx::Rect(30, 39, 70, 60)));

    EXPECT_TRUE(occlusion.UnoccludedLayerContentRect(
        parent, gfx::Rect(30, 40, 70, 60)).IsEmpty());
    EXPECT_RECT_EQ(gfx::Rect(29, 40, 1, 60),
                   occlusion.UnoccludedLayerContentRect(
                       parent, gfx::Rect(29, 40, 70, 60)));
    EXPECT_RECT_EQ(gfx::Rect(30, 39, 70, 1),
                   occlusion.UnoccludedLayerContentRect(
                       parent, gfx::Rect(30, 39, 70, 60)));
    EXPECT_RECT_EQ(gfx::Rect(),
                   occlusion.UnoccludedLayerContentRect(
                       parent, gfx::Rect(31, 40, 69, 60)));
    EXPECT_RECT_EQ(gfx::Rect(),
                   occlusion.UnoccludedLayerContentRect(
                       parent, gfx::Rect(30, 41, 70, 59)));

    /* Justification for the above occlusion from |layer1| and |layer2|:

           +---------------------+
           |                     |30  Visible region of |layer1|: /////
           |                     |    Visible region of |layer2|: \\\\\
           |     +---------------------------------+
           |     |               |10               |
           |  +---------------+-----------------+  |
           |  |  |\\\\\\\\\\\\|//|     420      |  |
           |  |  |\\\\\\\\\\\\|//|60            |  |
           |  |  |\\\\\\\\\\\\|//|              |  |
           +--|--|------------|--+              |  |
            20|10|     70     |                 |  |
              |  |            |                 |  |
              |  |            |                 |  |
              |  |            |                 |  |
              |  |            |                 |  |
              |  |            |                 |  |
              |  |            |                 |10|
              |  +------------|-----------------|--+
              |               | 490             |
              +---------------+-----------------+
                     60               440
         */
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestSurfaceWithTwoOpaqueChildren);

template <class Types>
class OcclusionTrackerTestOverlappingSurfaceSiblings
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestOverlappingSurfaceSiblings(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() {
    gfx::Transform child_transform;
    child_transform.Translate(250.0, 250.0);
    child_transform.Rotate(90.0);
    child_transform.Translate(-250.0, -250.0);

    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(100, 100));
    parent->SetMasksToBounds(true);
    typename Types::LayerType* child1 = this->CreateSurface(
        parent, child_transform, gfx::PointF(30.f, 30.f), gfx::Size(10, 10));
    typename Types::LayerType* child2 = this->CreateSurface(
        parent, child_transform, gfx::PointF(20.f, 40.f), gfx::Size(10, 10));
    typename Types::ContentLayerType* layer1 =
        this->CreateDrawingLayer(child1,
                                 this->identity_matrix,
                                 gfx::PointF(-10.f, -10.f),
                                 gfx::Size(510, 510),
                                 true);
    typename Types::ContentLayerType* layer2 =
        this->CreateDrawingLayer(child2,
                                 this->identity_matrix,
                                 gfx::PointF(-10.f, -10.f),
                                 gfx::Size(510, 510),
                                 true);
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(layer2, &occlusion);
    this->EnterContributingSurface(child2, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(-10, 420, 70, 80).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    // There is nothing above child2's surface in the z-order.
    EXPECT_RECT_EQ(gfx::Rect(-10, 420, 70, 80),
                   occlusion.UnoccludedSurfaceContentRect(
                       child2, false, gfx::Rect(-10, 420, 70, 80)));

    this->LeaveContributingSurface(child2, &occlusion);
    this->VisitLayer(layer1, &occlusion);
    this->EnterContributingSurface(child1, &occlusion);

    EXPECT_EQ(gfx::Rect(0, 430, 70, 80).ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(-10, 430, 80, 70).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    // child2's contents will occlude child1 below it.
    EXPECT_RECT_EQ(gfx::Rect(-10, 430, 10, 70),
                   occlusion.UnoccludedSurfaceContentRect(
                       child1, false, gfx::Rect(-10, 430, 80, 70)));

    this->LeaveContributingSurface(child1, &occlusion);
    this->EnterLayer(parent, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(UnionRegions(gfx::Rect(30, 20, 70, 10), gfx::Rect(20, 30, 80, 70))
                  .ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    EXPECT_FALSE(occlusion.OccludedLayer(parent, gfx::Rect(20, 20, 80, 80)));

    EXPECT_TRUE(occlusion.OccludedLayer(parent, gfx::Rect(30, 20, 70, 80)));
    EXPECT_FALSE(occlusion.OccludedLayer(parent, gfx::Rect(29, 20, 70, 80)));
    EXPECT_FALSE(occlusion.OccludedLayer(parent, gfx::Rect(30, 19, 70, 80)));

    EXPECT_TRUE(occlusion.OccludedLayer(parent, gfx::Rect(20, 30, 80, 70)));
    EXPECT_FALSE(occlusion.OccludedLayer(parent, gfx::Rect(19, 30, 80, 70)));
    EXPECT_FALSE(occlusion.OccludedLayer(parent, gfx::Rect(20, 29, 80, 70)));

    /* Justification for the above occlusion:
               100
      +---------------------+
      |    20               |       layer1
      |  30+ ---------------------------------+
  100 |  30|                |     layer2      |
      |20+----------------------------------+ |
      |  | |                |               | |
      |  | |                |               | |
      |  | |                |               | |
      +--|-|----------------+               | |
         | |                                | | 510
         | |                                | |
         | |                                | |
         | |                                | |
         | |                                | |
         | |                                | |
         | |                                | |
         | +--------------------------------|-+
         |                                  |
         +----------------------------------+
                         510
     */
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestOverlappingSurfaceSiblings);

template <class Types>
class OcclusionTrackerTestOverlappingSurfaceSiblingsWithTwoTransforms
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestOverlappingSurfaceSiblingsWithTwoTransforms(
      bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() {
    gfx::Transform child1_transform;
    child1_transform.Translate(250.0, 250.0);
    child1_transform.Rotate(-90.0);
    child1_transform.Translate(-250.0, -250.0);

    gfx::Transform child2_transform;
    child2_transform.Translate(250.0, 250.0);
    child2_transform.Rotate(90.0);
    child2_transform.Translate(-250.0, -250.0);

    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(100, 100));
    parent->SetMasksToBounds(true);
    typename Types::LayerType* child1 = this->CreateSurface(
        parent, child1_transform, gfx::PointF(30.f, 20.f), gfx::Size(10, 10));
    typename Types::LayerType* child2 =
        this->CreateDrawingSurface(parent,
                                   child2_transform,
                                   gfx::PointF(20.f, 40.f),
                                   gfx::Size(10, 10),
                                   false);
    typename Types::ContentLayerType* layer1 =
        this->CreateDrawingLayer(child1,
                                 this->identity_matrix,
                                 gfx::PointF(-10.f, -20.f),
                                 gfx::Size(510, 510),
                                 true);
    typename Types::ContentLayerType* layer2 =
        this->CreateDrawingLayer(child2,
                                 this->identity_matrix,
                                 gfx::PointF(-10.f, -10.f),
                                 gfx::Size(510, 510),
                                 true);
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(layer2, &occlusion);
    this->EnterLayer(child2, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(-10, 420, 70, 80).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->LeaveLayer(child2, &occlusion);
    this->EnterContributingSurface(child2, &occlusion);

    // There is nothing above child2's surface in the z-order.
    EXPECT_RECT_EQ(gfx::Rect(-10, 420, 70, 80),
                   occlusion.UnoccludedSurfaceContentRect(
                       child2, false, gfx::Rect(-10, 420, 70, 80)));

    this->LeaveContributingSurface(child2, &occlusion);
    this->VisitLayer(layer1, &occlusion);
    this->EnterContributingSurface(child1, &occlusion);

    EXPECT_EQ(gfx::Rect(420, -10, 70, 80).ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(420, -20, 80, 90).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    // child2's contents will occlude child1 below it.
    EXPECT_EQ(gfx::Rect(20, 30, 80, 70).ToString(),
              occlusion.occlusion_on_contributing_surface_from_inside_target()
                  .ToString());
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_on_contributing_surface_from_outside_target()
                  .ToString());

    this->LeaveContributingSurface(child1, &occlusion);
    this->EnterLayer(parent, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(10, 20, 90, 80).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    /* Justification for the above occlusion:
                  100
        +---------------------+
        |20                   |       layer1
       10+----------------------------------+
    100 || 30                 |     layer2  |
        |20+----------------------------------+
        || |                  |             | |
        || |                  |             | |
        || |                  |             | |
        +|-|------------------+             | |
         | |                                | | 510
         | |                            510 | |
         | |                                | |
         | |                                | |
         | |                                | |
         | |                                | |
         | |                520             | |
         +----------------------------------+ |
           |                                  |
           +----------------------------------+
                           510
     */
  }
};

ALL_OCCLUSIONTRACKER_TEST(
    OcclusionTrackerTestOverlappingSurfaceSiblingsWithTwoTransforms);

template <class Types>
class OcclusionTrackerTestFilters : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestFilters(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() {
    gfx::Transform layer_transform;
    layer_transform.Translate(250.0, 250.0);
    layer_transform.Rotate(90.0);
    layer_transform.Translate(-250.0, -250.0);

    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(100, 100));
    parent->SetMasksToBounds(true);
    typename Types::ContentLayerType* blur_layer =
        this->CreateDrawingLayer(parent,
                                 layer_transform,
                                 gfx::PointF(30.f, 30.f),
                                 gfx::Size(500, 500),
                                 true);
    typename Types::ContentLayerType* opaque_layer =
        this->CreateDrawingLayer(parent,
                                 layer_transform,
                                 gfx::PointF(30.f, 30.f),
                                 gfx::Size(500, 500),
                                 true);
    typename Types::ContentLayerType* opacity_layer =
        this->CreateDrawingLayer(parent,
                                 layer_transform,
                                 gfx::PointF(30.f, 30.f),
                                 gfx::Size(500, 500),
                                 true);

    FilterOperations filters;
    filters.Append(FilterOperation::CreateBlurFilter(10.f));
    blur_layer->SetFilters(filters);

    filters.Clear();
    filters.Append(FilterOperation::CreateGrayscaleFilter(0.5f));
    opaque_layer->SetFilters(filters);

    filters.Clear();
    filters.Append(FilterOperation::CreateOpacityFilter(0.5f));
    opacity_layer->SetFilters(filters);

    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    // Opacity layer won't contribute to occlusion.
    this->VisitLayer(opacity_layer, &occlusion);
    this->EnterContributingSurface(opacity_layer, &occlusion);

    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
    EXPECT_TRUE(occlusion.occlusion_from_inside_target().IsEmpty());

    // And has nothing to contribute to its parent surface.
    this->LeaveContributingSurface(opacity_layer, &occlusion);
    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
    EXPECT_TRUE(occlusion.occlusion_from_inside_target().IsEmpty());

    // Opaque layer will contribute to occlusion.
    this->VisitLayer(opaque_layer, &occlusion);
    this->EnterContributingSurface(opaque_layer, &occlusion);

    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
    EXPECT_EQ(gfx::Rect(0, 430, 70, 70).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    // And it gets translated to the parent surface.
    this->LeaveContributingSurface(opaque_layer, &occlusion);
    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
    EXPECT_EQ(gfx::Rect(30, 30, 70, 70).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    // The blur layer needs to throw away any occlusion from outside its
    // subtree.
    this->EnterLayer(blur_layer, &occlusion);
    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
    EXPECT_TRUE(occlusion.occlusion_from_inside_target().IsEmpty());

    // And it won't contribute to occlusion.
    this->LeaveLayer(blur_layer, &occlusion);
    this->EnterContributingSurface(blur_layer, &occlusion);
    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
    EXPECT_TRUE(occlusion.occlusion_from_inside_target().IsEmpty());

    // But the opaque layer's occlusion is preserved on the parent.
    this->LeaveContributingSurface(blur_layer, &occlusion);
    this->EnterLayer(parent, &occlusion);
    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
    EXPECT_EQ(gfx::Rect(30, 30, 70, 70).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestFilters);

template <class Types>
class OcclusionTrackerTestReplicaDoesOcclude
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestReplicaDoesOcclude(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() {
    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(100, 200));
    typename Types::LayerType* surface =
        this->CreateDrawingSurface(parent,
                                   this->identity_matrix,
                                   gfx::PointF(0.f, 100.f),
                                   gfx::Size(50, 50),
                                   true);
    this->CreateReplicaLayer(
        surface, this->identity_matrix, gfx::PointF(50.f, 50.f), gfx::Size());
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(surface, &occlusion);

    EXPECT_EQ(gfx::Rect(0, 0, 50, 50).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->VisitContributingSurface(surface, &occlusion);
    this->EnterLayer(parent, &occlusion);

    // The surface and replica should both be occluding the parent.
    EXPECT_EQ(
        UnionRegions(gfx::Rect(0, 100, 50, 50),
                     gfx::Rect(50, 150, 50, 50)).ToString(),
        occlusion.occlusion_from_inside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestReplicaDoesOcclude);

template <class Types>
class OcclusionTrackerTestReplicaWithClipping
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestReplicaWithClipping(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() {
    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(100, 170));
    parent->SetMasksToBounds(true);
    typename Types::LayerType* surface =
        this->CreateDrawingSurface(parent,
                                   this->identity_matrix,
                                   gfx::PointF(0.f, 100.f),
                                   gfx::Size(50, 50),
                                   true);
    this->CreateReplicaLayer(
        surface, this->identity_matrix, gfx::PointF(50.f, 50.f), gfx::Size());
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(surface, &occlusion);

    EXPECT_EQ(gfx::Rect(0, 0, 50, 50).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->VisitContributingSurface(surface, &occlusion);
    this->EnterLayer(parent, &occlusion);

    // The surface and replica should both be occluding the parent.
    EXPECT_EQ(
        UnionRegions(gfx::Rect(0, 100, 50, 50),
                     gfx::Rect(50, 150, 50, 20)).ToString(),
        occlusion.occlusion_from_inside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestReplicaWithClipping);

template <class Types>
class OcclusionTrackerTestReplicaWithMask : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestReplicaWithMask(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() {
    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(100, 200));
    typename Types::LayerType* surface =
        this->CreateDrawingSurface(parent,
                                   this->identity_matrix,
                                   gfx::PointF(0.f, 100.f),
                                   gfx::Size(50, 50),
                                   true);
    typename Types::LayerType* replica = this->CreateReplicaLayer(
        surface, this->identity_matrix, gfx::PointF(50.f, 50.f), gfx::Size());
    this->CreateMaskLayer(replica, gfx::Size(10, 10));
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(surface, &occlusion);

    EXPECT_EQ(gfx::Rect(0, 0, 50, 50).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->VisitContributingSurface(surface, &occlusion);
    this->EnterLayer(parent, &occlusion);

    // The replica should not be occluding the parent, since it has a mask
    // applied to it.
    EXPECT_EQ(gfx::Rect(0, 100, 50, 50).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestReplicaWithMask);

template <class Types>
class OcclusionTrackerTestOpaqueContentsRegionEmpty
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestOpaqueContentsRegionEmpty(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() {
    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(300, 300));
    typename Types::ContentLayerType* layer =
        this->CreateDrawingSurface(parent,
                                   this->identity_matrix,
                                   gfx::PointF(),
                                   gfx::Size(200, 200),
                                   false);
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));
    this->EnterLayer(layer, &occlusion);

    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(0, 0, 100, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(100, 0, 100, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(0, 100, 100, 100)));
    EXPECT_FALSE(occlusion.OccludedLayer(layer, gfx::Rect(100, 100, 100, 100)));

    this->LeaveLayer(layer, &occlusion);
    this->VisitContributingSurface(layer, &occlusion);
    this->EnterLayer(parent, &occlusion);

    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
  }
};

MAIN_AND_IMPL_THREAD_TEST(OcclusionTrackerTestOpaqueContentsRegionEmpty);

template <class Types>
class OcclusionTrackerTestOpaqueContentsRegionNonEmpty
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestOpaqueContentsRegionNonEmpty(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() {
    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(300, 300));
    typename Types::ContentLayerType* layer =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(100.f, 100.f),
                                 gfx::Size(200, 200),
                                 false);
    this->CalcDrawEtc(parent);
    {
      TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
          gfx::Rect(0, 0, 1000, 1000));
      layer->SetOpaqueContentsRect(gfx::Rect(0, 0, 100, 100));

      this->ResetLayerIterator();
      this->VisitLayer(layer, &occlusion);
      this->EnterLayer(parent, &occlusion);

      EXPECT_EQ(gfx::Rect(100, 100, 100, 100).ToString(),
                occlusion.occlusion_from_inside_target().ToString());

      EXPECT_FALSE(
          occlusion.OccludedLayer(parent, gfx::Rect(0, 100, 100, 100)));
      EXPECT_TRUE(
          occlusion.OccludedLayer(parent, gfx::Rect(100, 100, 100, 100)));
      EXPECT_FALSE(
          occlusion.OccludedLayer(parent, gfx::Rect(200, 200, 100, 100)));
    }
    {
      TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
          gfx::Rect(0, 0, 1000, 1000));
      layer->SetOpaqueContentsRect(gfx::Rect(20, 20, 180, 180));

      this->ResetLayerIterator();
      this->VisitLayer(layer, &occlusion);
      this->EnterLayer(parent, &occlusion);

      EXPECT_EQ(gfx::Rect(120, 120, 180, 180).ToString(),
                occlusion.occlusion_from_inside_target().ToString());

      EXPECT_FALSE(
          occlusion.OccludedLayer(parent, gfx::Rect(0, 100, 100, 100)));
      EXPECT_FALSE(
          occlusion.OccludedLayer(parent, gfx::Rect(100, 100, 100, 100)));
      EXPECT_TRUE(
          occlusion.OccludedLayer(parent, gfx::Rect(200, 200, 100, 100)));
    }
    {
      TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
          gfx::Rect(0, 0, 1000, 1000));
      layer->SetOpaqueContentsRect(gfx::Rect(150, 150, 100, 100));

      this->ResetLayerIterator();
      this->VisitLayer(layer, &occlusion);
      this->EnterLayer(parent, &occlusion);

      EXPECT_EQ(gfx::Rect(250, 250, 50, 50).ToString(),
                occlusion.occlusion_from_inside_target().ToString());

      EXPECT_FALSE(
          occlusion.OccludedLayer(parent, gfx::Rect(0, 100, 100, 100)));
      EXPECT_FALSE(
          occlusion.OccludedLayer(parent, gfx::Rect(100, 100, 100, 100)));
      EXPECT_FALSE(
          occlusion.OccludedLayer(parent, gfx::Rect(200, 200, 100, 100)));
    }
  }
};

MAIN_AND_IMPL_THREAD_TEST(OcclusionTrackerTestOpaqueContentsRegionNonEmpty);

template <class Types>
class OcclusionTrackerTest3dTransform : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTest3dTransform(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() {
    gfx::Transform transform;
    transform.RotateAboutYAxis(30.0);

    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(300, 300));
    typename Types::LayerType* container = this->CreateLayer(
        parent, this->identity_matrix, gfx::PointF(), gfx::Size(300, 300));
    typename Types::ContentLayerType* layer =
        this->CreateDrawingLayer(container,
                                 transform,
                                 gfx::PointF(100.f, 100.f),
                                 gfx::Size(200, 200),
                                 true);
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));
    this->EnterLayer(layer, &occlusion);

    // The layer is rotated in 3d but without preserving 3d, so it only gets
    // resized.
    EXPECT_RECT_EQ(
        gfx::Rect(0, 0, 200, 200),
        occlusion.UnoccludedLayerContentRect(layer, gfx::Rect(0, 0, 200, 200)));
  }
};

MAIN_AND_IMPL_THREAD_TEST(OcclusionTrackerTest3dTransform);

template <class Types>
class OcclusionTrackerTestUnsorted3dLayers
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestUnsorted3dLayers(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() {
    // Currently, The main thread layer iterator does not iterate over 3d items
    // in sorted order, because layer sorting is not performed on the main
    // thread.  Because of this, the occlusion tracker cannot assume that a 3d
    // layer occludes other layers that have not yet been iterated over. For
    // now, the expected behavior is that a 3d layer simply does not add any
    // occlusion to the occlusion tracker.

    gfx::Transform translation_to_front;
    translation_to_front.Translate3d(0.0, 0.0, -10.0);
    gfx::Transform translation_to_back;
    translation_to_front.Translate3d(0.0, 0.0, -100.0);

    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(300, 300));
    typename Types::ContentLayerType* child1 = this->CreateDrawingLayer(
        parent, translation_to_back, gfx::PointF(), gfx::Size(100, 100), true);
    typename Types::ContentLayerType* child2 =
        this->CreateDrawingLayer(parent,
                                 translation_to_front,
                                 gfx::PointF(50.f, 50.f),
                                 gfx::Size(100, 100),
                                 true);
    parent->SetShouldFlattenTransform(false);
    parent->Set3dSortingContextId(1);
    child1->Set3dSortingContextId(1);
    child2->Set3dSortingContextId(1);

    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));
    this->VisitLayer(child2, &occlusion);
    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
    EXPECT_TRUE(occlusion.occlusion_from_inside_target().IsEmpty());

    this->VisitLayer(child1, &occlusion);
    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
    EXPECT_TRUE(occlusion.occlusion_from_inside_target().IsEmpty());
  }
};

// This test will have different layer ordering on the impl thread; the test
// will only work on the main thread.
MAIN_THREAD_TEST(OcclusionTrackerTestUnsorted3dLayers);

template <class Types>
class OcclusionTrackerTestPerspectiveTransform
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestPerspectiveTransform(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() {
    gfx::Transform transform;
    transform.Translate(150.0, 150.0);
    transform.ApplyPerspectiveDepth(400.0);
    transform.RotateAboutXAxis(-30.0);
    transform.Translate(-150.0, -150.0);

    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(300, 300));
    typename Types::LayerType* container = this->CreateLayer(
        parent, this->identity_matrix, gfx::PointF(), gfx::Size(300, 300));
    typename Types::ContentLayerType* layer =
        this->CreateDrawingLayer(container,
                                 transform,
                                 gfx::PointF(100.f, 100.f),
                                 gfx::Size(200, 200),
                                 true);
    container->SetShouldFlattenTransform(false);
    container->Set3dSortingContextId(1);
    layer->Set3dSortingContextId(1);
    layer->SetShouldFlattenTransform(false);

    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));
    this->EnterLayer(layer, &occlusion);

    EXPECT_RECT_EQ(
        gfx::Rect(0, 0, 200, 200),
        occlusion.UnoccludedLayerContentRect(layer, gfx::Rect(0, 0, 200, 200)));
  }
};

// This test requires accumulating occlusion of 3d layers, which are skipped by
// the occlusion tracker on the main thread. So this test should run on the impl
// thread.
IMPL_THREAD_TEST(OcclusionTrackerTestPerspectiveTransform);
template <class Types>
class OcclusionTrackerTestPerspectiveTransformBehindCamera
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestPerspectiveTransformBehindCamera(
      bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() {
    // This test is based on the platform/chromium/compositing/3d-corners.html
    // layout test.
    gfx::Transform transform;
    transform.Translate(250.0, 50.0);
    transform.ApplyPerspectiveDepth(10.0);
    transform.Translate(-250.0, -50.0);
    transform.Translate(250.0, 50.0);
    transform.RotateAboutXAxis(-167.0);
    transform.Translate(-250.0, -50.0);

    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(500, 100));
    typename Types::LayerType* container = this->CreateLayer(
        parent, this->identity_matrix, gfx::PointF(), gfx::Size(500, 500));
    typename Types::ContentLayerType* layer = this->CreateDrawingLayer(
        container, transform, gfx::PointF(), gfx::Size(500, 500), true);
    container->SetShouldFlattenTransform(false);
    container->Set3dSortingContextId(1);
    layer->SetShouldFlattenTransform(false);
    layer->Set3dSortingContextId(1);
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));
    this->EnterLayer(layer, &occlusion);

    // The bottom 11 pixel rows of this layer remain visible inside the
    // container, after translation to the target surface. When translated back,
    // this will include many more pixels but must include at least the bottom
    // 11 rows.
    EXPECT_TRUE(occlusion.UnoccludedLayerContentRect(
        layer, gfx::Rect(0, 26, 500, 474)).
            Contains(gfx::Rect(0, 489, 500, 11)));
  }
};

// This test requires accumulating occlusion of 3d layers, which are skipped by
// the occlusion tracker on the main thread. So this test should run on the impl
// thread.
IMPL_THREAD_TEST(OcclusionTrackerTestPerspectiveTransformBehindCamera);

template <class Types>
class OcclusionTrackerTestLayerBehindCameraDoesNotOcclude
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestLayerBehindCameraDoesNotOcclude(
      bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() {
    gfx::Transform transform;
    transform.Translate(50.0, 50.0);
    transform.ApplyPerspectiveDepth(100.0);
    transform.Translate3d(0.0, 0.0, 110.0);
    transform.Translate(-50.0, -50.0);

    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(100, 100));
    typename Types::ContentLayerType* layer = this->CreateDrawingLayer(
        parent, transform, gfx::PointF(), gfx::Size(100, 100), true);
    parent->SetShouldFlattenTransform(false);
    parent->Set3dSortingContextId(1);
    layer->SetShouldFlattenTransform(false);
    layer->Set3dSortingContextId(1);
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    // The |layer| is entirely behind the camera and should not occlude.
    this->VisitLayer(layer, &occlusion);
    this->EnterLayer(parent, &occlusion);
    EXPECT_TRUE(occlusion.occlusion_from_inside_target().IsEmpty());
    EXPECT_TRUE(occlusion.occlusion_from_outside_target().IsEmpty());
  }
};

// This test requires accumulating occlusion of 3d layers, which are skipped by
// the occlusion tracker on the main thread. So this test should run on the impl
// thread.
IMPL_THREAD_TEST(OcclusionTrackerTestLayerBehindCameraDoesNotOcclude);

template <class Types>
class OcclusionTrackerTestLargePixelsOccludeInsideClipRect
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestLargePixelsOccludeInsideClipRect(
      bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() {
    gfx::Transform transform;
    transform.Translate(50.0, 50.0);
    transform.ApplyPerspectiveDepth(100.0);
    transform.Translate3d(0.0, 0.0, 99.0);
    transform.Translate(-50.0, -50.0);

    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(100, 100));
    parent->SetMasksToBounds(true);
    typename Types::ContentLayerType* layer = this->CreateDrawingLayer(
        parent, transform, gfx::PointF(), gfx::Size(100, 100), true);
    parent->SetShouldFlattenTransform(false);
    parent->Set3dSortingContextId(1);
    layer->SetShouldFlattenTransform(false);
    layer->Set3dSortingContextId(1);
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    // This is very close to the camera, so pixels in its visible_content_rect()
    // will actually go outside of the layer's clip rect.  Ensure that those
    // pixels don't occlude things outside the clip rect.
    this->VisitLayer(layer, &occlusion);
    this->EnterLayer(parent, &occlusion);
    EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
  }
};

// This test requires accumulating occlusion of 3d layers, which are skipped by
// the occlusion tracker on the main thread. So this test should run on the impl
// thread.
IMPL_THREAD_TEST(OcclusionTrackerTestLargePixelsOccludeInsideClipRect);

template <class Types>
class OcclusionTrackerTestAnimationOpacity1OnMainThread
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestAnimationOpacity1OnMainThread(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() {
    // parent
    // +--layer
    // +--surface
    // |  +--surface_child
    // |  +--surface_child2
    // +--parent2
    // +--topmost

    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(300, 300));
    typename Types::ContentLayerType* layer =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(),
                                 gfx::Size(300, 300),
                                 true);
    typename Types::ContentLayerType* surface =
        this->CreateDrawingSurface(parent,
                                   this->identity_matrix,
                                   gfx::PointF(),
                                   gfx::Size(300, 300),
                                   true);
    typename Types::ContentLayerType* surface_child =
        this->CreateDrawingLayer(surface,
                                 this->identity_matrix,
                                 gfx::PointF(),
                                 gfx::Size(200, 300),
                                 true);
    typename Types::ContentLayerType* surface_child2 =
        this->CreateDrawingLayer(surface,
                                 this->identity_matrix,
                                 gfx::PointF(),
                                 gfx::Size(100, 300),
                                 true);
    typename Types::ContentLayerType* parent2 =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(),
                                 gfx::Size(300, 300),
                                 false);
    typename Types::ContentLayerType* topmost =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(250.f, 0.f),
                                 gfx::Size(50, 300),
                                 true);

    AddOpacityTransitionToController(
        layer->layer_animation_controller(), 10.0, 0.f, 1.f, false);
    AddOpacityTransitionToController(
        surface->layer_animation_controller(), 10.0, 0.f, 1.f, false);
    this->CalcDrawEtc(parent);

    EXPECT_TRUE(layer->draw_opacity_is_animating());
    EXPECT_FALSE(surface->draw_opacity_is_animating());
    EXPECT_TRUE(surface->render_surface()->draw_opacity_is_animating());

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(topmost, &occlusion);
    this->EnterLayer(parent2, &occlusion);
    // This occlusion will affect all surfaces.
    EXPECT_EQ(gfx::Rect(250, 0, 50, 300).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(0, 0, 250, 300).ToString(),
              occlusion.UnoccludedLayerContentRect(
                  parent2, gfx::Rect(0, 0, 300, 300)).ToString());
    this->LeaveLayer(parent2, &occlusion);

    this->VisitLayer(surface_child2, &occlusion);
    this->EnterLayer(surface_child, &occlusion);
    EXPECT_EQ(gfx::Rect(0, 0, 100, 300).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
    EXPECT_EQ(gfx::Rect(250, 0, 50, 300).ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_RECT_EQ(gfx::Rect(100, 0, 100, 300),
                   occlusion.UnoccludedLayerContentRect(
                       surface_child, gfx::Rect(0, 0, 200, 300)));
    this->LeaveLayer(surface_child, &occlusion);
    this->EnterLayer(surface, &occlusion);
    EXPECT_EQ(gfx::Rect(0, 0, 200, 300).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
    EXPECT_EQ(gfx::Rect(250, 0, 50, 300).ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_RECT_EQ(gfx::Rect(200, 0, 50, 300),
                   occlusion.UnoccludedLayerContentRect(
                       surface, gfx::Rect(0, 0, 300, 300)));
    this->LeaveLayer(surface, &occlusion);

    this->EnterContributingSurface(surface, &occlusion);
    // Occlusion within the surface is lost when leaving the animating surface.
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_inside_target().ToString());
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_RECT_EQ(gfx::Rect(0, 0, 250, 300),
                   occlusion.UnoccludedSurfaceContentRect(
                       surface, false, gfx::Rect(0, 0, 300, 300)));
    this->LeaveContributingSurface(surface, &occlusion);

    // Occlusion from outside the animating surface still exists.
    EXPECT_EQ(gfx::Rect(250, 0, 50, 300).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());

    this->VisitLayer(layer, &occlusion);
    this->EnterLayer(parent, &occlusion);

    // Occlusion is not added for the animating |layer|.
    EXPECT_RECT_EQ(gfx::Rect(0, 0, 250, 300),
                   occlusion.UnoccludedLayerContentRect(
                       parent, gfx::Rect(0, 0, 300, 300)));
  }
};

MAIN_THREAD_TEST(OcclusionTrackerTestAnimationOpacity1OnMainThread);

template <class Types>
class OcclusionTrackerTestAnimationOpacity0OnMainThread
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestAnimationOpacity0OnMainThread(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() {
    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(300, 300));
    typename Types::ContentLayerType* layer =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(),
                                 gfx::Size(300, 300),
                                 true);
    typename Types::ContentLayerType* surface =
        this->CreateDrawingSurface(parent,
                                   this->identity_matrix,
                                   gfx::PointF(),
                                   gfx::Size(300, 300),
                                   true);
    typename Types::ContentLayerType* surface_child =
        this->CreateDrawingLayer(surface,
                                 this->identity_matrix,
                                 gfx::PointF(),
                                 gfx::Size(200, 300),
                                 true);
    typename Types::ContentLayerType* surface_child2 =
        this->CreateDrawingLayer(surface,
                                 this->identity_matrix,
                                 gfx::PointF(),
                                 gfx::Size(100, 300),
                                 true);
    typename Types::ContentLayerType* parent2 =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(),
                                 gfx::Size(300, 300),
                                 false);
    typename Types::ContentLayerType* topmost =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(250.f, 0.f),
                                 gfx::Size(50, 300),
                                 true);

    AddOpacityTransitionToController(
        layer->layer_animation_controller(), 10.0, 1.f, 0.f, false);
    AddOpacityTransitionToController(
        surface->layer_animation_controller(), 10.0, 1.f, 0.f, false);
    this->CalcDrawEtc(parent);

    EXPECT_TRUE(layer->draw_opacity_is_animating());
    EXPECT_FALSE(surface->draw_opacity_is_animating());
    EXPECT_TRUE(surface->render_surface()->draw_opacity_is_animating());

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(topmost, &occlusion);
    this->EnterLayer(parent2, &occlusion);
    // This occlusion will affect all surfaces.
    EXPECT_EQ(gfx::Rect(250, 0, 50, 300).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_RECT_EQ(gfx::Rect(0, 0, 250, 300),
                   occlusion.UnoccludedLayerContentRect(
                       parent, gfx::Rect(0, 0, 300, 300)));
    this->LeaveLayer(parent2, &occlusion);

    this->VisitLayer(surface_child2, &occlusion);
    this->EnterLayer(surface_child, &occlusion);
    EXPECT_EQ(gfx::Rect(0, 0, 100, 300).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
    EXPECT_EQ(gfx::Rect(250, 0, 50, 300).ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_RECT_EQ(gfx::Rect(100, 0, 100, 300),
                   occlusion.UnoccludedLayerContentRect(
                       surface_child, gfx::Rect(0, 0, 200, 300)));
    this->LeaveLayer(surface_child, &occlusion);
    this->EnterLayer(surface, &occlusion);
    EXPECT_EQ(gfx::Rect(0, 0, 200, 300).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
    EXPECT_EQ(gfx::Rect(250, 0, 50, 300).ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_RECT_EQ(gfx::Rect(200, 0, 50, 300),
                   occlusion.UnoccludedLayerContentRect(
                       surface, gfx::Rect(0, 0, 300, 300)));
    this->LeaveLayer(surface, &occlusion);

    this->EnterContributingSurface(surface, &occlusion);
    // Occlusion within the surface is lost when leaving the animating surface.
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_inside_target().ToString());
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_RECT_EQ(gfx::Rect(0, 0, 250, 300),
                   occlusion.UnoccludedSurfaceContentRect(
                       surface, false, gfx::Rect(0, 0, 300, 300)));
    this->LeaveContributingSurface(surface, &occlusion);

    // Occlusion from outside the animating surface still exists.
    EXPECT_EQ(gfx::Rect(250, 0, 50, 300).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());

    this->VisitLayer(layer, &occlusion);
    this->EnterLayer(parent, &occlusion);

    // Occlusion is not added for the animating |layer|.
    EXPECT_RECT_EQ(gfx::Rect(0, 0, 250, 300),
                   occlusion.UnoccludedLayerContentRect(
                       parent, gfx::Rect(0, 0, 300, 300)));
  }
};

MAIN_THREAD_TEST(OcclusionTrackerTestAnimationOpacity0OnMainThread);

template <class Types>
class OcclusionTrackerTestAnimationTranslateOnMainThread
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestAnimationTranslateOnMainThread(
      bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() {
    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(300, 300));
    typename Types::ContentLayerType* layer =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(),
                                 gfx::Size(300, 300),
                                 true);
    typename Types::ContentLayerType* surface =
        this->CreateDrawingSurface(parent,
                                   this->identity_matrix,
                                   gfx::PointF(),
                                   gfx::Size(300, 300),
                                   true);
    typename Types::ContentLayerType* surface_child =
        this->CreateDrawingLayer(surface,
                                 this->identity_matrix,
                                 gfx::PointF(),
                                 gfx::Size(200, 300),
                                 true);
    typename Types::ContentLayerType* surface_child2 =
        this->CreateDrawingLayer(surface,
                                 this->identity_matrix,
                                 gfx::PointF(),
                                 gfx::Size(100, 300),
                                 true);
    typename Types::ContentLayerType* surface2 = this->CreateDrawingSurface(
        parent, this->identity_matrix, gfx::PointF(), gfx::Size(50, 300), true);

    AddAnimatedTransformToController(
        layer->layer_animation_controller(), 10.0, 30, 0);
    AddAnimatedTransformToController(
        surface->layer_animation_controller(), 10.0, 30, 0);
    AddAnimatedTransformToController(
        surface_child->layer_animation_controller(), 10.0, 30, 0);
    this->CalcDrawEtc(parent);

    EXPECT_TRUE(layer->draw_transform_is_animating());
    EXPECT_TRUE(layer->screen_space_transform_is_animating());
    EXPECT_TRUE(
        surface->render_surface()->target_surface_transforms_are_animating());
    EXPECT_TRUE(
        surface->render_surface()->screen_space_transforms_are_animating());
    // The surface owning layer doesn't animate against its own surface.
    EXPECT_FALSE(surface->draw_transform_is_animating());
    EXPECT_TRUE(surface->screen_space_transform_is_animating());
    EXPECT_TRUE(surface_child->draw_transform_is_animating());
    EXPECT_TRUE(surface_child->screen_space_transform_is_animating());

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(surface2, &occlusion);
    this->EnterContributingSurface(surface2, &occlusion);

    EXPECT_EQ(gfx::Rect(0, 0, 50, 300).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->LeaveContributingSurface(surface2, &occlusion);
    this->EnterLayer(surface_child2, &occlusion);
    // surface_child2 is moving in screen space but not relative to its target,
    // so occlusion should happen in its target space only.  It also means that
    // things occluding from outside the target (e.g. surface2) cannot occlude
    // this layer.
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->LeaveLayer(surface_child2, &occlusion);
    this->EnterLayer(surface_child, &occlusion);
    // surface_child2 added to the occlusion since it is not moving relative
    // to its target.
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(0, 0, 100, 300).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->LeaveLayer(surface_child, &occlusion);
    // surface_child is moving relative to its target, so it does not add
    // occlusion.
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(0, 0, 100, 300).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->EnterLayer(surface, &occlusion);
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(0, 0, 100, 300).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->LeaveLayer(surface, &occlusion);
    // The surface's owning layer is moving in screen space but not relative to
    // its target, so it adds to the occlusion.
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(0, 0, 300, 300).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->EnterContributingSurface(surface, &occlusion);
    this->LeaveContributingSurface(surface, &occlusion);
    // The |surface| is moving in the screen and in its target, so all occlusion
    // within the surface is lost when leaving it. Only the |surface2| occlusion
    // is left.
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(0, 0, 50, 300).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->VisitLayer(layer, &occlusion);
    // The |layer| is animating in the screen and in its target, so no occlusion
    // is added.
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(0, 0, 50, 300).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

MAIN_THREAD_TEST(OcclusionTrackerTestAnimationTranslateOnMainThread);

template <class Types>
class OcclusionTrackerTestSurfaceOcclusionTranslatesToParent
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestSurfaceOcclusionTranslatesToParent(
      bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() {
    gfx::Transform surface_transform;
    surface_transform.Translate(300.0, 300.0);
    surface_transform.Scale(2.0, 2.0);
    surface_transform.Translate(-150.0, -150.0);

    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(500, 500));
    typename Types::ContentLayerType* surface = this->CreateDrawingSurface(
        parent, surface_transform, gfx::PointF(), gfx::Size(300, 300), false);
    typename Types::ContentLayerType* surface2 =
        this->CreateDrawingSurface(parent,
                                   this->identity_matrix,
                                   gfx::PointF(50.f, 50.f),
                                   gfx::Size(300, 300),
                                   false);
    surface->SetOpaqueContentsRect(gfx::Rect(0, 0, 200, 200));
    surface2->SetOpaqueContentsRect(gfx::Rect(0, 0, 200, 200));
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(surface2, &occlusion);
    this->VisitContributingSurface(surface2, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(50, 50, 200, 200).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    // Clear any stored occlusion.
    occlusion.set_occlusion_from_outside_target(Region());
    occlusion.set_occlusion_from_inside_target(Region());

    this->VisitLayer(surface, &occlusion);
    this->VisitContributingSurface(surface, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(0, 0, 400, 400).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

MAIN_AND_IMPL_THREAD_TEST(
    OcclusionTrackerTestSurfaceOcclusionTranslatesToParent);

template <class Types>
class OcclusionTrackerTestSurfaceOcclusionTranslatesWithClipping
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestSurfaceOcclusionTranslatesWithClipping(
      bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() {
    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(300, 300));
    parent->SetMasksToBounds(true);
    typename Types::ContentLayerType* surface =
        this->CreateDrawingSurface(parent,
                                   this->identity_matrix,
                                   gfx::PointF(),
                                   gfx::Size(500, 300),
                                   false);
    surface->SetOpaqueContentsRect(gfx::Rect(0, 0, 400, 200));
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(surface, &occlusion);
    this->VisitContributingSurface(surface, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(0, 0, 300, 200).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

MAIN_AND_IMPL_THREAD_TEST(
    OcclusionTrackerTestSurfaceOcclusionTranslatesWithClipping);

template <class Types>
class OcclusionTrackerTestReplicaOccluded : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestReplicaOccluded(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() {
    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(100, 200));
    typename Types::LayerType* surface =
        this->CreateDrawingSurface(parent,
                                   this->identity_matrix,
                                   gfx::PointF(),
                                   gfx::Size(100, 100),
                                   true);
    this->CreateReplicaLayer(surface,
                             this->identity_matrix,
                             gfx::PointF(0.f, 100.f),
                             gfx::Size(100, 100));
    typename Types::LayerType* topmost =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(0.f, 100.f),
                                 gfx::Size(100, 100),
                                 true);
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    // |topmost| occludes the replica, but not the surface itself.
    this->VisitLayer(topmost, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(0, 100, 100, 100).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->VisitLayer(surface, &occlusion);

    // Render target with replica ignores occlusion from outside.
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->EnterContributingSurface(surface, &occlusion);

    // Surface is not occluded so it shouldn't think it is.
    EXPECT_RECT_EQ(gfx::Rect(0, 0, 100, 100),
                   occlusion.UnoccludedSurfaceContentRect(
                       surface, false, gfx::Rect(0, 0, 100, 100)));
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestReplicaOccluded);

template <class Types>
class OcclusionTrackerTestSurfaceWithReplicaUnoccluded
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestSurfaceWithReplicaUnoccluded(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() {
    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(100, 200));
    typename Types::LayerType* surface =
        this->CreateDrawingSurface(parent,
                                   this->identity_matrix,
                                   gfx::PointF(),
                                   gfx::Size(100, 100),
                                   true);
    this->CreateReplicaLayer(surface,
                             this->identity_matrix,
                             gfx::PointF(0.f, 100.f),
                             gfx::Size(100, 100));
    typename Types::LayerType* topmost =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(),
                                 gfx::Size(100, 110),
                                 true);
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    // |topmost| occludes the surface, but not the entire surface's replica.
    this->VisitLayer(topmost, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(0, 0, 100, 110).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->VisitLayer(surface, &occlusion);

    // Render target with replica ignores occlusion from outside.
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->EnterContributingSurface(surface, &occlusion);

    // Surface is occluded, but only the top 10px of the replica.
    EXPECT_RECT_EQ(gfx::Rect(0, 0, 0, 0),
                   occlusion.UnoccludedSurfaceContentRect(
                       surface, false, gfx::Rect(0, 0, 100, 100)));
    EXPECT_RECT_EQ(gfx::Rect(0, 10, 100, 90),
                   occlusion.UnoccludedSurfaceContentRect(
                       surface, true, gfx::Rect(0, 0, 100, 100)));
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestSurfaceWithReplicaUnoccluded);

template <class Types>
class OcclusionTrackerTestSurfaceAndReplicaOccludedDifferently
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestSurfaceAndReplicaOccludedDifferently(
      bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() {
    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(100, 200));
    typename Types::LayerType* surface =
        this->CreateDrawingSurface(parent,
                                   this->identity_matrix,
                                   gfx::PointF(),
                                   gfx::Size(100, 100),
                                   true);
    this->CreateReplicaLayer(surface,
                             this->identity_matrix,
                             gfx::PointF(0.f, 100.f),
                             gfx::Size(100, 100));
    typename Types::LayerType* over_surface = this->CreateDrawingLayer(
        parent, this->identity_matrix, gfx::PointF(), gfx::Size(40, 100), true);
    typename Types::LayerType* over_replica =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(0.f, 100.f),
                                 gfx::Size(50, 100),
                                 true);
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    // These occlude the surface and replica differently, so we can test each
    // one.
    this->VisitLayer(over_replica, &occlusion);
    this->VisitLayer(over_surface, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(UnionRegions(gfx::Rect(0, 0, 40, 100), gfx::Rect(0, 100, 50, 100))
                  .ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->VisitLayer(surface, &occlusion);

    // Render target with replica ignores occlusion from outside.
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->EnterContributingSurface(surface, &occlusion);

    // Surface and replica are occluded different amounts.
    EXPECT_RECT_EQ(gfx::Rect(40, 0, 60, 100),
                   occlusion.UnoccludedSurfaceContentRect(
                       surface, false, gfx::Rect(0, 0, 100, 100)));
    EXPECT_RECT_EQ(gfx::Rect(50, 0, 50, 100),
                   occlusion.UnoccludedSurfaceContentRect(
                       surface, true, gfx::Rect(0, 0, 100, 100)));
  }
};

ALL_OCCLUSIONTRACKER_TEST(
    OcclusionTrackerTestSurfaceAndReplicaOccludedDifferently);

template <class Types>
class OcclusionTrackerTestSurfaceChildOfSurface
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestSurfaceChildOfSurface(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() {
    // This test verifies that the surface cliprect does not end up empty and
    // clip away the entire unoccluded rect.

    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(100, 200));
    typename Types::LayerType* surface =
        this->CreateDrawingSurface(parent,
                                   this->identity_matrix,
                                   gfx::PointF(),
                                   gfx::Size(100, 100),
                                   true);
    typename Types::LayerType* surface_child =
        this->CreateDrawingSurface(surface,
                                   this->identity_matrix,
                                   gfx::PointF(0.f, 10.f),
                                   gfx::Size(100, 50),
                                   true);
    typename Types::LayerType* topmost = this->CreateDrawingLayer(
        parent, this->identity_matrix, gfx::PointF(), gfx::Size(100, 50), true);
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(-100, -100, 1000, 1000));

    // |topmost| occludes everything partially so we know occlusion is happening
    // at all.
    this->VisitLayer(topmost, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(0, 0, 100, 50).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->VisitLayer(surface_child, &occlusion);

    // surface_child increases the occlusion in the screen by a narrow sliver.
    EXPECT_EQ(gfx::Rect(0, -10, 100, 50).ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    // In its own surface, surface_child is at 0,0 as is its occlusion.
    EXPECT_EQ(gfx::Rect(0, 0, 100, 50).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    // The root layer always has a clip rect. So the parent of |surface| has a
    // clip rect. However, the owning layer for |surface| does not mask to
    // bounds, so it doesn't have a clip rect of its own. Thus the parent of
    // |surface_child| exercises different code paths as its parent does not
    // have a clip rect.

    this->EnterContributingSurface(surface_child, &occlusion);
    // The surface_child's parent does not have a clip rect as it owns a render
    // surface. Make sure the unoccluded rect does not get clipped away
    // inappropriately.
    EXPECT_RECT_EQ(gfx::Rect(0, 40, 100, 10),
                   occlusion.UnoccludedSurfaceContentRect(
                       surface_child, false, gfx::Rect(0, 0, 100, 50)));
    this->LeaveContributingSurface(surface_child, &occlusion);

    // When the surface_child's occlusion is transformed up to its parent, make
    // sure it is not clipped away inappropriately also.
    this->EnterLayer(surface, &occlusion);
    EXPECT_EQ(gfx::Rect(0, 0, 100, 50).ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(0, 10, 100, 50).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
    this->LeaveLayer(surface, &occlusion);

    this->EnterContributingSurface(surface, &occlusion);
    // The surface's parent does have a clip rect as it is the root layer.
    EXPECT_RECT_EQ(gfx::Rect(0, 50, 100, 50),
                   occlusion.UnoccludedSurfaceContentRect(
                       surface, false, gfx::Rect(0, 0, 100, 100)));
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestSurfaceChildOfSurface);

template <class Types>
class OcclusionTrackerTestDontOccludePixelsNeededForBackgroundFilter
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestDontOccludePixelsNeededForBackgroundFilter(
      bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() {
    gfx::Transform scale_by_half;
    scale_by_half.Scale(0.5, 0.5);

    // Make a 50x50 filtered surface that is completely surrounded by opaque
    // layers which are above it in the z-order.  The surface is scaled to test
    // that the pixel moving is done in the target space, where the background
    // filter is applied.
    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(200, 150));
    typename Types::LayerType* filtered_surface =
        this->CreateDrawingLayer(parent,
                                 scale_by_half,
                                 gfx::PointF(50.f, 50.f),
                                 gfx::Size(100, 100),
                                 false);
    typename Types::LayerType* occluding_layer1 = this->CreateDrawingLayer(
        parent, this->identity_matrix, gfx::PointF(), gfx::Size(200, 50), true);
    typename Types::LayerType* occluding_layer2 =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(0.f, 100.f),
                                 gfx::Size(200, 50),
                                 true);
    typename Types::LayerType* occluding_layer3 =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(0.f, 50.f),
                                 gfx::Size(50, 50),
                                 true);
    typename Types::LayerType* occluding_layer4 =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(100.f, 50.f),
                                 gfx::Size(100, 50),
                                 true);

    // Filters make the layer own a surface.
    FilterOperations filters;
    filters.Append(FilterOperation::CreateBlurFilter(10.f));
    filtered_surface->SetBackgroundFilters(filters);

    // Save the distance of influence for the blur effect.
    int outset_top, outset_right, outset_bottom, outset_left;
    filters.GetOutsets(
        &outset_top, &outset_right, &outset_bottom, &outset_left);

    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    // These layers occlude pixels directly beside the filtered_surface. Because
    // filtered surface blends pixels in a radius, it will need to see some of
    // the pixels (up to radius far) underneath the occluding layers.
    this->VisitLayer(occluding_layer4, &occlusion);
    this->VisitLayer(occluding_layer3, &occlusion);
    this->VisitLayer(occluding_layer2, &occlusion);
    this->VisitLayer(occluding_layer1, &occlusion);

    Region expected_occlusion;
    expected_occlusion.Union(gfx::Rect(0, 0, 200, 50));
    expected_occlusion.Union(gfx::Rect(0, 50, 50, 50));
    expected_occlusion.Union(gfx::Rect(100, 50, 100, 50));
    expected_occlusion.Union(gfx::Rect(0, 100, 200, 50));

    EXPECT_EQ(expected_occlusion.ToString(),
              occlusion.occlusion_from_inside_target().ToString());
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());

    this->VisitLayer(filtered_surface, &occlusion);

    // The filtered layer does not occlude.
    Region expected_occlusion_outside_surface;
    expected_occlusion_outside_surface.Union(gfx::Rect(-50, -50, 200, 50));
    expected_occlusion_outside_surface.Union(gfx::Rect(-50, 0, 50, 50));
    expected_occlusion_outside_surface.Union(gfx::Rect(50, 0, 100, 50));
    expected_occlusion_outside_surface.Union(gfx::Rect(-50, 50, 200, 50));

    EXPECT_EQ(expected_occlusion_outside_surface.ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    // The surface has a background blur, so it needs pixels that are currently
    // considered occluded in order to be drawn. So the pixels it needs should
    // be removed some the occluded area so that when we get to the parent they
    // are drawn.
    this->VisitContributingSurface(filtered_surface, &occlusion);

    this->EnterLayer(parent, &occlusion);

    Region expected_blurred_occlusion;
    expected_blurred_occlusion.Union(gfx::Rect(0, 0, 200, 50 - outset_top));
    expected_blurred_occlusion.Union(gfx::Rect(
        0, 50 - outset_top, 50 - outset_left, 50 + outset_top + outset_bottom));
    expected_blurred_occlusion.Union(
        gfx::Rect(100 + outset_right,
                  50 - outset_top,
                  100 - outset_right,
                  50 + outset_top + outset_bottom));
    expected_blurred_occlusion.Union(
        gfx::Rect(0, 100 + outset_bottom, 200, 50 - outset_bottom));

    EXPECT_EQ(expected_blurred_occlusion.ToString(),
              occlusion.occlusion_from_inside_target().ToString());
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());

    gfx::Rect outset_rect;
    gfx::Rect test_rect;

    // Nothing in the blur outsets for the filtered_surface is occluded.
    outset_rect = gfx::Rect(50 - outset_left,
                            50 - outset_top,
                            50 + outset_left + outset_right,
                            50 + outset_top + outset_bottom);
    test_rect = outset_rect;
    EXPECT_EQ(
        outset_rect.ToString(),
        occlusion.UnoccludedLayerContentRect(parent, test_rect).ToString());

    // Stuff outside the blur outsets is still occluded though.
    test_rect = outset_rect;
    test_rect.Inset(0, 0, -1, 0);
    EXPECT_EQ(
        outset_rect.ToString(),
        occlusion.UnoccludedLayerContentRect(parent, test_rect).ToString());
    test_rect = outset_rect;
    test_rect.Inset(0, 0, 0, -1);
    EXPECT_EQ(
        outset_rect.ToString(),
        occlusion.UnoccludedLayerContentRect(parent, test_rect).ToString());
    test_rect = outset_rect;
    test_rect.Inset(-1, 0, 0, 0);
    EXPECT_EQ(
        outset_rect.ToString(),
        occlusion.UnoccludedLayerContentRect(parent, test_rect).ToString());
    test_rect = outset_rect;
    test_rect.Inset(0, -1, 0, 0);
    EXPECT_EQ(
        outset_rect.ToString(),
        occlusion.UnoccludedLayerContentRect(parent, test_rect).ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(
    OcclusionTrackerTestDontOccludePixelsNeededForBackgroundFilter);

template <class Types>
class OcclusionTrackerTestTwoBackgroundFiltersReduceOcclusionTwice
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestTwoBackgroundFiltersReduceOcclusionTwice(
      bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() {
    gfx::Transform scale_by_half;
    scale_by_half.Scale(0.5, 0.5);

    // Makes two surfaces that completely cover |parent|. The occlusion both
    // above and below the filters will be reduced by each of them.
    typename Types::ContentLayerType* root = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(75, 75));
    typename Types::LayerType* parent = this->CreateSurface(
        root, scale_by_half, gfx::PointF(), gfx::Size(150, 150));
    parent->SetMasksToBounds(true);
    typename Types::LayerType* filtered_surface1 = this->CreateDrawingLayer(
        parent, scale_by_half, gfx::PointF(), gfx::Size(300, 300), false);
    typename Types::LayerType* filtered_surface2 = this->CreateDrawingLayer(
        parent, scale_by_half, gfx::PointF(), gfx::Size(300, 300), false);
    typename Types::LayerType* occluding_layer_above =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(100.f, 100.f),
                                 gfx::Size(50, 50),
                                 true);

    // Filters make the layers own surfaces.
    FilterOperations filters;
    filters.Append(FilterOperation::CreateBlurFilter(1.f));
    filtered_surface1->SetBackgroundFilters(filters);
    filtered_surface2->SetBackgroundFilters(filters);

    // Save the distance of influence for the blur effect.
    int outset_top, outset_right, outset_bottom, outset_left;
    filters.GetOutsets(
        &outset_top, &outset_right, &outset_bottom, &outset_left);

    this->CalcDrawEtc(root);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(occluding_layer_above, &occlusion);
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(100 / 2, 100 / 2, 50 / 2, 50 / 2).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    this->VisitLayer(filtered_surface2, &occlusion);
    this->VisitContributingSurface(filtered_surface2, &occlusion);
    this->VisitLayer(filtered_surface1, &occlusion);
    this->VisitContributingSurface(filtered_surface1, &occlusion);

    // Test expectations in the target.
    gfx::Rect expected_occlusion =
        gfx::Rect(100 / 2 + outset_right * 2,
                  100 / 2 + outset_bottom * 2,
                  50 / 2 - (outset_left + outset_right) * 2,
                  50 / 2 - (outset_top + outset_bottom) * 2);
    EXPECT_EQ(expected_occlusion.ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    // Test expectations in the screen are the same as in the target, as the
    // render surface is 1:1 with the screen.
    EXPECT_EQ(expected_occlusion.ToString(),
              occlusion.occlusion_from_outside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(
    OcclusionTrackerTestTwoBackgroundFiltersReduceOcclusionTwice);

template <class Types>
class OcclusionTrackerTestDontReduceOcclusionBelowBackgroundFilter
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestDontReduceOcclusionBelowBackgroundFilter(
      bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() {
    gfx::Transform scale_by_half;
    scale_by_half.Scale(0.5, 0.5);

    // Make a surface and its replica, each 50x50, with a smaller 30x30 layer
    // centered below each.  The surface is scaled to test that the pixel moving
    // is done in the target space, where the background filter is applied, but
    // the surface appears at 50, 50 and the replica at 200, 50.
    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(300, 150));
    typename Types::LayerType* behind_surface_layer =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(60.f, 60.f),
                                 gfx::Size(30, 30),
                                 true);
    typename Types::LayerType* behind_replica_layer =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(210.f, 60.f),
                                 gfx::Size(30, 30),
                                 true);
    typename Types::LayerType* filtered_surface =
        this->CreateDrawingLayer(parent,
                                 scale_by_half,
                                 gfx::PointF(50.f, 50.f),
                                 gfx::Size(100, 100),
                                 false);
    this->CreateReplicaLayer(filtered_surface,
                             this->identity_matrix,
                             gfx::PointF(300.f, 0.f),
                             gfx::Size());

    // Filters make the layer own a surface.
    FilterOperations filters;
    filters.Append(FilterOperation::CreateBlurFilter(3.f));
    filtered_surface->SetBackgroundFilters(filters);

    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    // The surface has a background blur, so it blurs non-opaque pixels below
    // it.
    this->VisitLayer(filtered_surface, &occlusion);
    this->VisitContributingSurface(filtered_surface, &occlusion);

    this->VisitLayer(behind_replica_layer, &occlusion);
    this->VisitLayer(behind_surface_layer, &occlusion);

    // The layers behind the surface are not blurred, and their occlusion does
    // not change, until we leave the surface.  So it should not be modified by
    // the filter here.
    gfx::Rect occlusion_behind_surface = gfx::Rect(60, 60, 30, 30);
    gfx::Rect occlusion_behind_replica = gfx::Rect(210, 60, 30, 30);

    Region expected_opaque_bounds =
        UnionRegions(occlusion_behind_surface, occlusion_behind_replica);
    EXPECT_EQ(expected_opaque_bounds.ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(
    OcclusionTrackerTestDontReduceOcclusionBelowBackgroundFilter);

template <class Types>
class OcclusionTrackerTestDontReduceOcclusionIfBackgroundFilterIsOccluded
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestDontReduceOcclusionIfBackgroundFilterIsOccluded(
      bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() {
    gfx::Transform scale_by_half;
    scale_by_half.Scale(0.5, 0.5);

    // Make a 50x50 filtered surface that is completely occluded by an opaque
    // layer which is above it in the z-order.  The surface is
    // scaled to test that the pixel moving is done in the target space, where
    // the background filter is applied, but the surface appears at 50, 50.
    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(200, 150));
    typename Types::LayerType* filtered_surface =
        this->CreateDrawingLayer(parent,
                                 scale_by_half,
                                 gfx::PointF(50.f, 50.f),
                                 gfx::Size(100, 100),
                                 false);
    typename Types::LayerType* occluding_layer =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(50.f, 50.f),
                                 gfx::Size(50, 50),
                                 true);

    // Filters make the layer own a surface.
    FilterOperations filters;
    filters.Append(FilterOperation::CreateBlurFilter(3.f));
    filtered_surface->SetBackgroundFilters(filters);

    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(occluding_layer, &occlusion);

    this->VisitLayer(filtered_surface, &occlusion);
    {
      // The layers above the filtered surface occlude from outside.
      gfx::Rect occlusion_above_surface = gfx::Rect(0, 0, 50, 50);

      EXPECT_EQ(gfx::Rect().ToString(),
                occlusion.occlusion_from_inside_target().ToString());
      EXPECT_EQ(occlusion_above_surface.ToString(),
                occlusion.occlusion_from_outside_target().ToString());
    }

    // The surface has a background blur, so it blurs non-opaque pixels below
    // it.
    this->VisitContributingSurface(filtered_surface, &occlusion);
    {
      // The filter is completely occluded, so it should not blur anything and
      // reduce any occlusion.
      gfx::Rect occlusion_above_surface = gfx::Rect(50, 50, 50, 50);

      EXPECT_EQ(occlusion_above_surface.ToString(),
                occlusion.occlusion_from_inside_target().ToString());
      EXPECT_EQ(gfx::Rect().ToString(),
                occlusion.occlusion_from_outside_target().ToString());
    }
  }
};

ALL_OCCLUSIONTRACKER_TEST(
    OcclusionTrackerTestDontReduceOcclusionIfBackgroundFilterIsOccluded);

template <class Types>
class OcclusionTrackerTestReduceOcclusionWhenBackgroundFilterIsPartiallyOccluded
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit
  OcclusionTrackerTestReduceOcclusionWhenBackgroundFilterIsPartiallyOccluded(
      bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() {
    gfx::Transform scale_by_half;
    scale_by_half.Scale(0.5, 0.5);

    // Make a surface and its replica, each 50x50, that are partially occluded
    // by opaque layers which are above them in the z-order.  The surface is
    // scaled to test that the pixel moving is done in the target space, where
    // the background filter is applied, but the surface appears at 50, 50 and
    // the replica at 200, 50.
    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(300, 150));
    typename Types::LayerType* filtered_surface =
        this->CreateDrawingLayer(parent,
                                 scale_by_half,
                                 gfx::PointF(50.f, 50.f),
                                 gfx::Size(100, 100),
                                 false);
    this->CreateReplicaLayer(filtered_surface,
                             this->identity_matrix,
                             gfx::PointF(300.f, 0.f),
                             gfx::Size());
    typename Types::LayerType* above_surface_layer =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(70.f, 50.f),
                                 gfx::Size(30, 50),
                                 true);
    typename Types::LayerType* above_replica_layer =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(200.f, 50.f),
                                 gfx::Size(30, 50),
                                 true);
    typename Types::LayerType* beside_surface_layer =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(90.f, 40.f),
                                 gfx::Size(10, 10),
                                 true);
    typename Types::LayerType* beside_replica_layer =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(200.f, 40.f),
                                 gfx::Size(10, 10),
                                 true);

    // Filters make the layer own a surface.
    FilterOperations filters;
    filters.Append(FilterOperation::CreateBlurFilter(3.f));
    filtered_surface->SetBackgroundFilters(filters);

    // Save the distance of influence for the blur effect.
    int outset_top, outset_right, outset_bottom, outset_left;
    filters.GetOutsets(
        &outset_top, &outset_right, &outset_bottom, &outset_left);

    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(beside_replica_layer, &occlusion);
    this->VisitLayer(beside_surface_layer, &occlusion);
    this->VisitLayer(above_replica_layer, &occlusion);
    this->VisitLayer(above_surface_layer, &occlusion);

    // The surface has a background blur, so it blurs non-opaque pixels below
    // it.
    this->VisitLayer(filtered_surface, &occlusion);
    this->VisitContributingSurface(filtered_surface, &occlusion);

    // The filter in the surface and replica are partially unoccluded. Only the
    // unoccluded parts should reduce occlusion.  This means it will push back
    // the occlusion that touches the unoccluded part (occlusion_above___), but
    // it will not touch occlusion_beside____ since that is not beside the
    // unoccluded part of the surface, even though it is beside the occluded
    // part of the surface.
    gfx::Rect occlusion_above_surface =
        gfx::Rect(70 + outset_right, 50, 30 - outset_right, 50);
    gfx::Rect occlusion_above_replica =
        gfx::Rect(200, 50, 30 - outset_left, 50);
    gfx::Rect occlusion_beside_surface = gfx::Rect(90, 40, 10, 10);
    gfx::Rect occlusion_beside_replica = gfx::Rect(200, 40, 10, 10);

    Region expected_occlusion;
    expected_occlusion.Union(occlusion_above_surface);
    expected_occlusion.Union(occlusion_above_replica);
    expected_occlusion.Union(occlusion_beside_surface);
    expected_occlusion.Union(occlusion_beside_replica);

    ASSERT_EQ(expected_occlusion.ToString(),
              occlusion.occlusion_from_inside_target().ToString());
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());

    Region::Iterator expected_rects(expected_occlusion);
    Region::Iterator target_surface_rects(
        occlusion.occlusion_from_inside_target());
    for (; expected_rects.has_rect();
         expected_rects.next(), target_surface_rects.next()) {
      ASSERT_TRUE(target_surface_rects.has_rect());
      EXPECT_EQ(expected_rects.rect(), target_surface_rects.rect());
    }
  }
};

ALL_OCCLUSIONTRACKER_TEST(
    OcclusionTrackerTestReduceOcclusionWhenBackgroundFilterIsPartiallyOccluded);

template <class Types>
class OcclusionTrackerTestMinimumTrackingSize
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestMinimumTrackingSize(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() {
    gfx::Size tracking_size(100, 100);
    gfx::Size below_tracking_size(99, 99);

    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(400, 400));
    typename Types::LayerType* large = this->CreateDrawingLayer(
        parent, this->identity_matrix, gfx::PointF(), tracking_size, true);
    typename Types::LayerType* small =
        this->CreateDrawingLayer(parent,
                                 this->identity_matrix,
                                 gfx::PointF(),
                                 below_tracking_size,
                                 true);
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));
    occlusion.set_minimum_tracking_size(tracking_size);

    // The small layer is not tracked because it is too small.
    this->VisitLayer(small, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    // The large layer is tracked as it is large enough.
    this->VisitLayer(large, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(tracking_size).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestMinimumTrackingSize);

template <class Types>
class OcclusionTrackerTestScaledLayerIsClipped
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestScaledLayerIsClipped(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() {
    gfx::Transform scale_transform;
    scale_transform.Scale(512.0, 512.0);

    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(400, 400));
    typename Types::LayerType* clip = this->CreateLayer(parent,
                                                        this->identity_matrix,
                                                        gfx::PointF(10.f, 10.f),
                                                        gfx::Size(50, 50));
    clip->SetMasksToBounds(true);
    typename Types::LayerType* scale = this->CreateLayer(
        clip, scale_transform, gfx::PointF(), gfx::Size(1, 1));
    typename Types::LayerType* scaled = this->CreateDrawingLayer(
        scale, this->identity_matrix, gfx::PointF(), gfx::Size(500, 500), true);
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(scaled, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(10, 10, 50, 50).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestScaledLayerIsClipped)

template <class Types>
class OcclusionTrackerTestScaledLayerInSurfaceIsClipped
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestScaledLayerInSurfaceIsClipped(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() {
    gfx::Transform scale_transform;
    scale_transform.Scale(512.0, 512.0);

    typename Types::ContentLayerType* parent = this->CreateRoot(
        this->identity_matrix, gfx::PointF(), gfx::Size(400, 400));
    typename Types::LayerType* clip = this->CreateLayer(parent,
                                                        this->identity_matrix,
                                                        gfx::PointF(10.f, 10.f),
                                                        gfx::Size(50, 50));
    clip->SetMasksToBounds(true);
    typename Types::LayerType* surface = this->CreateDrawingSurface(
        clip, this->identity_matrix, gfx::PointF(), gfx::Size(400, 30), false);
    typename Types::LayerType* scale = this->CreateLayer(
        surface, scale_transform, gfx::PointF(), gfx::Size(1, 1));
    typename Types::LayerType* scaled = this->CreateDrawingLayer(
        scale, this->identity_matrix, gfx::PointF(), gfx::Size(500, 500), true);
    this->CalcDrawEtc(parent);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(scaled, &occlusion);
    this->VisitLayer(surface, &occlusion);
    this->VisitContributingSurface(surface, &occlusion);

    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(10, 10, 50, 50).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestScaledLayerInSurfaceIsClipped)

template <class Types>
class OcclusionTrackerTestCopyRequestDoesOcclude
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestCopyRequestDoesOcclude(bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() {
    typename Types::ContentLayerType* root = this->CreateRoot(
        this->identity_matrix, gfx::Point(), gfx::Size(400, 400));
    typename Types::ContentLayerType* parent = this->CreateDrawingLayer(
        root, this->identity_matrix, gfx::Point(), gfx::Size(400, 400), true);
    typename Types::LayerType* copy = this->CreateLayer(parent,
                                                        this->identity_matrix,
                                                        gfx::Point(100, 0),
                                                        gfx::Size(200, 400));
    this->AddCopyRequest(copy);
    typename Types::LayerType* copy_child = this->CreateDrawingLayer(
        copy,
        this->identity_matrix,
        gfx::PointF(),
        gfx::Size(200, 400),
        true);
    this->CalcDrawEtc(root);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(copy_child, &occlusion);
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(200, 400).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    // CopyRequests cause the layer to own a surface.
    this->VisitContributingSurface(copy, &occlusion);

    // The occlusion from the copy should be kept.
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(100, 0, 200, 400).ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestCopyRequestDoesOcclude)

template <class Types>
class OcclusionTrackerTestHiddenCopyRequestDoesNotOcclude
    : public OcclusionTrackerTest<Types> {
 protected:
  explicit OcclusionTrackerTestHiddenCopyRequestDoesNotOcclude(
      bool opaque_layers)
      : OcclusionTrackerTest<Types>(opaque_layers) {}
  void RunMyTest() {
    typename Types::ContentLayerType* root = this->CreateRoot(
        this->identity_matrix, gfx::Point(), gfx::Size(400, 400));
    typename Types::ContentLayerType* parent = this->CreateDrawingLayer(
        root, this->identity_matrix, gfx::Point(), gfx::Size(400, 400), true);
    typename Types::LayerType* hide = this->CreateLayer(
        parent, this->identity_matrix, gfx::Point(), gfx::Size());
    typename Types::LayerType* copy = this->CreateLayer(
        hide, this->identity_matrix, gfx::Point(100, 0), gfx::Size(200, 400));
    this->AddCopyRequest(copy);
    typename Types::LayerType* copy_child = this->CreateDrawingLayer(
        copy, this->identity_matrix, gfx::PointF(), gfx::Size(200, 400), true);

    // The |copy| layer is hidden but since it is being copied, it will be
    // drawn.
    hide->SetHideLayerAndSubtree(true);

    this->CalcDrawEtc(root);

    TestOcclusionTrackerWithClip<typename Types::LayerType> occlusion(
        gfx::Rect(0, 0, 1000, 1000));

    this->VisitLayer(copy_child, &occlusion);
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect(200, 400).ToString(),
              occlusion.occlusion_from_inside_target().ToString());

    // CopyRequests cause the layer to own a surface.
    this->VisitContributingSurface(copy, &occlusion);

    // The occlusion from the copy should be dropped since it is hidden.
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_outside_target().ToString());
    EXPECT_EQ(gfx::Rect().ToString(),
              occlusion.occlusion_from_inside_target().ToString());
  }
};

ALL_OCCLUSIONTRACKER_TEST(OcclusionTrackerTestHiddenCopyRequestDoesNotOcclude)

}  // namespace
}  // namespace cc
