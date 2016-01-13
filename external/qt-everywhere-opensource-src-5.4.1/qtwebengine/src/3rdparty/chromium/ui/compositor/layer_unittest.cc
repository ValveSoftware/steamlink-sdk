// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/debug/trace_event.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "cc/layers/delegated_frame_provider.h"
#include "cc/layers/delegated_frame_resource_collection.h"
#include "cc/layers/layer.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "cc/output/delegated_frame_data.h"
#include "cc/test/pixel_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/test/context_factories_for_test.h"
#include "ui/compositor/test/draw_waiter_for_test.h"
#include "ui/compositor/test/test_compositor_host.h"
#include "ui/compositor/test/test_layers.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/gfx_paths.h"
#include "ui/gfx/skia_util.h"

using cc::MatchesPNGFile;

namespace ui {

namespace {

// There are three test classes in here that configure the Compositor and
// Layer's slightly differently:
// - LayerWithNullDelegateTest uses NullLayerDelegate as the LayerDelegate. This
//   is typically the base class you want to use.
// - LayerWithDelegateTest uses LayerDelegate on the delegates.
// - LayerWithRealCompositorTest when a real compositor is required for testing.
//    - Slow because they bring up a window and run the real compositor. This
//      is typically not what you want.

class ColoredLayer : public Layer, public LayerDelegate {
 public:
  explicit ColoredLayer(SkColor color)
      : Layer(LAYER_TEXTURED),
        color_(color) {
    set_delegate(this);
  }

  virtual ~ColoredLayer() { }

  // Overridden from LayerDelegate:
  virtual void OnPaintLayer(gfx::Canvas* canvas) OVERRIDE {
    canvas->DrawColor(color_);
  }

  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE {
  }

  virtual base::Closure PrepareForLayerBoundsChange() OVERRIDE {
    return base::Closure();
  }

 private:
  SkColor color_;
};

class LayerWithRealCompositorTest : public testing::Test {
 public:
  LayerWithRealCompositorTest() {
    if (PathService::Get(gfx::DIR_TEST_DATA, &test_data_directory_)) {
      test_data_directory_ = test_data_directory_.AppendASCII("compositor");
    } else {
      LOG(ERROR) << "Could not open test data directory.";
    }
  }
  virtual ~LayerWithRealCompositorTest() {}

  // Overridden from testing::Test:
  virtual void SetUp() OVERRIDE {
    bool enable_pixel_output = true;
    ui::ContextFactory* context_factory =
        InitializeContextFactoryForTests(enable_pixel_output);

    const gfx::Rect host_bounds(10, 10, 500, 500);
    compositor_host_.reset(TestCompositorHost::Create(
                               host_bounds, context_factory));
    compositor_host_->Show();
  }

  virtual void TearDown() OVERRIDE {
    compositor_host_.reset();
    TerminateContextFactoryForTests();
  }

  Compositor* GetCompositor() { return compositor_host_->GetCompositor(); }

  Layer* CreateLayer(LayerType type) {
    return new Layer(type);
  }

  Layer* CreateColorLayer(SkColor color, const gfx::Rect& bounds) {
    Layer* layer = new ColoredLayer(color);
    layer->SetBounds(bounds);
    return layer;
  }

  Layer* CreateNoTextureLayer(const gfx::Rect& bounds) {
    Layer* layer = CreateLayer(LAYER_NOT_DRAWN);
    layer->SetBounds(bounds);
    return layer;
  }

  void DrawTree(Layer* root) {
    GetCompositor()->SetRootLayer(root);
    GetCompositor()->ScheduleDraw();
    WaitForDraw();
  }

  bool ReadPixels(SkBitmap* bitmap) {
    return ReadPixels(bitmap, gfx::Rect(GetCompositor()->size()));
  }

  bool ReadPixels(SkBitmap* bitmap, gfx::Rect source_rect) {
    scoped_refptr<ReadbackHolder> holder(new ReadbackHolder);
    scoped_ptr<cc::CopyOutputRequest> request =
        cc::CopyOutputRequest::CreateBitmapRequest(
            base::Bind(&ReadbackHolder::OutputRequestCallback, holder));
    request->set_area(source_rect);

    GetCompositor()->root_layer()->RequestCopyOfOutput(request.Pass());

    // Wait for copy response.  This needs to wait as the compositor could
    // be in the middle of a draw right now, and the commit with the
    // copy output request may not be done on the first draw.
    for (int i = 0; i < 2; i++) {
      GetCompositor()->ScheduleDraw();
      WaitForDraw();
    }

    if (holder->completed()) {
      *bitmap = holder->result();
      return true;
    }

    // Callback never called.
    NOTREACHED();
    return false;
  }

  void WaitForDraw() {
    ui::DrawWaiterForTest::Wait(GetCompositor());
  }

  void WaitForCommit() {
    ui::DrawWaiterForTest::WaitForCommit(GetCompositor());
  }

  // Invalidates the entire contents of the layer.
  void SchedulePaintForLayer(Layer* layer) {
    layer->SchedulePaint(
        gfx::Rect(0, 0, layer->bounds().width(), layer->bounds().height()));
  }

  const base::FilePath& test_data_directory() const {
    return test_data_directory_;
  }

 private:
  class ReadbackHolder : public base::RefCountedThreadSafe<ReadbackHolder> {
   public:
    ReadbackHolder() : completed_(false) {}

    void OutputRequestCallback(scoped_ptr<cc::CopyOutputResult> result) {
      DCHECK(!completed_);
      result_ = result->TakeBitmap();
      completed_ = true;
    }
    bool completed() const {
      return completed_;
    };
    const SkBitmap& result() const { return *result_; }

   private:
    friend class base::RefCountedThreadSafe<ReadbackHolder>;

    virtual ~ReadbackHolder() {}

    scoped_ptr<SkBitmap> result_;
    bool completed_;
  };

  scoped_ptr<TestCompositorHost> compositor_host_;

  // The root directory for test files.
  base::FilePath test_data_directory_;

  DISALLOW_COPY_AND_ASSIGN(LayerWithRealCompositorTest);
};

// LayerDelegate that paints colors to the layer.
class TestLayerDelegate : public LayerDelegate {
 public:
  explicit TestLayerDelegate() { reset(); }
  virtual ~TestLayerDelegate() {}

  void AddColor(SkColor color) {
    colors_.push_back(color);
  }

  const gfx::Size& paint_size() const { return paint_size_; }
  int color_index() const { return color_index_; }

  std::string ToScaleString() const {
    return base::StringPrintf("%.1f %.1f", scale_x_, scale_y_);
  }

  float device_scale_factor() const {
    return device_scale_factor_;
  }

  // Overridden from LayerDelegate:
  virtual void OnPaintLayer(gfx::Canvas* canvas) OVERRIDE {
    SkISize size = canvas->sk_canvas()->getBaseLayerSize();
    paint_size_ = gfx::Size(size.width(), size.height());
    canvas->FillRect(gfx::Rect(paint_size_), colors_[color_index_]);
    color_index_ = (color_index_ + 1) % static_cast<int>(colors_.size());
    const SkMatrix& matrix = canvas->sk_canvas()->getTotalMatrix();
    scale_x_ = matrix.getScaleX();
    scale_y_ = matrix.getScaleY();
  }

  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE {
    device_scale_factor_ = device_scale_factor;
  }

  virtual base::Closure PrepareForLayerBoundsChange() OVERRIDE {
    return base::Closure();
  }

  void reset() {
    color_index_ = 0;
    paint_size_.SetSize(0, 0);
    scale_x_ = scale_y_ = 0.0f;
    device_scale_factor_ = 0.0f;
  }

 private:
  std::vector<SkColor> colors_;
  int color_index_;
  gfx::Size paint_size_;
  float scale_x_;
  float scale_y_;
  float device_scale_factor_;

  DISALLOW_COPY_AND_ASSIGN(TestLayerDelegate);
};

// LayerDelegate that verifies that a layer was asked to update its canvas.
class DrawTreeLayerDelegate : public LayerDelegate {
 public:
  DrawTreeLayerDelegate() : painted_(false) {}
  virtual ~DrawTreeLayerDelegate() {}

  void Reset() {
    painted_ = false;
  }

  bool painted() const { return painted_; }

 private:
  // Overridden from LayerDelegate:
  virtual void OnPaintLayer(gfx::Canvas* canvas) OVERRIDE {
    painted_ = true;
  }
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE {
  }
  virtual base::Closure PrepareForLayerBoundsChange() OVERRIDE {
    return base::Closure();
  }

  bool painted_;

  DISALLOW_COPY_AND_ASSIGN(DrawTreeLayerDelegate);
};

// The simplest possible layer delegate. Does nothing.
class NullLayerDelegate : public LayerDelegate {
 public:
  NullLayerDelegate() {}
  virtual ~NullLayerDelegate() {}

 private:
  // Overridden from LayerDelegate:
  virtual void OnPaintLayer(gfx::Canvas* canvas) OVERRIDE {
  }
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE {
  }
  virtual base::Closure PrepareForLayerBoundsChange() OVERRIDE {
    return base::Closure();
  }

  DISALLOW_COPY_AND_ASSIGN(NullLayerDelegate);
};

// Remembers if it has been notified.
class TestCompositorObserver : public CompositorObserver {
 public:
  TestCompositorObserver()
      : committed_(false), started_(false), ended_(false), aborted_(false) {}

  bool committed() const { return committed_; }
  bool notified() const { return started_ && ended_; }
  bool aborted() const { return aborted_; }

  void Reset() {
    committed_ = false;
    started_ = false;
    ended_ = false;
    aborted_ = false;
  }

 private:
  virtual void OnCompositingDidCommit(Compositor* compositor) OVERRIDE {
    committed_ = true;
  }

  virtual void OnCompositingStarted(Compositor* compositor,
                                    base::TimeTicks start_time) OVERRIDE {
    started_ = true;
  }

  virtual void OnCompositingEnded(Compositor* compositor) OVERRIDE {
    ended_ = true;
  }

  virtual void OnCompositingAborted(Compositor* compositor) OVERRIDE {
    aborted_ = true;
  }

  virtual void OnCompositingLockStateChanged(Compositor* compositor) OVERRIDE {
  }

  bool committed_;
  bool started_;
  bool ended_;
  bool aborted_;

  DISALLOW_COPY_AND_ASSIGN(TestCompositorObserver);
};

}  // namespace

TEST_F(LayerWithRealCompositorTest, Draw) {
  scoped_ptr<Layer> layer(CreateColorLayer(SK_ColorRED,
                                           gfx::Rect(20, 20, 50, 50)));
  DrawTree(layer.get());
}

// Create this hierarchy:
// L1 - red
// +-- L2 - blue
// |   +-- L3 - yellow
// +-- L4 - magenta
//
TEST_F(LayerWithRealCompositorTest, Hierarchy) {
  scoped_ptr<Layer> l1(CreateColorLayer(SK_ColorRED,
                                        gfx::Rect(20, 20, 400, 400)));
  scoped_ptr<Layer> l2(CreateColorLayer(SK_ColorBLUE,
                                        gfx::Rect(10, 10, 350, 350)));
  scoped_ptr<Layer> l3(CreateColorLayer(SK_ColorYELLOW,
                                        gfx::Rect(5, 5, 25, 25)));
  scoped_ptr<Layer> l4(CreateColorLayer(SK_ColorMAGENTA,
                                        gfx::Rect(300, 300, 100, 100)));

  l1->Add(l2.get());
  l1->Add(l4.get());
  l2->Add(l3.get());

  DrawTree(l1.get());
}

class LayerWithDelegateTest : public testing::Test {
 public:
  LayerWithDelegateTest() {}
  virtual ~LayerWithDelegateTest() {}

  // Overridden from testing::Test:
  virtual void SetUp() OVERRIDE {
    bool enable_pixel_output = false;
    ui::ContextFactory* context_factory =
        InitializeContextFactoryForTests(enable_pixel_output);

    const gfx::Rect host_bounds(1000, 1000);
    compositor_host_.reset(TestCompositorHost::Create(host_bounds,
                                                      context_factory));
    compositor_host_->Show();
  }

  virtual void TearDown() OVERRIDE {
    compositor_host_.reset();
    TerminateContextFactoryForTests();
  }

  Compositor* compositor() { return compositor_host_->GetCompositor(); }

  virtual Layer* CreateLayer(LayerType type) {
    return new Layer(type);
  }

  Layer* CreateColorLayer(SkColor color, const gfx::Rect& bounds) {
    Layer* layer = new ColoredLayer(color);
    layer->SetBounds(bounds);
    return layer;
  }

  virtual Layer* CreateNoTextureLayer(const gfx::Rect& bounds) {
    Layer* layer = CreateLayer(LAYER_NOT_DRAWN);
    layer->SetBounds(bounds);
    return layer;
  }

  void DrawTree(Layer* root) {
    compositor()->SetRootLayer(root);
    Draw();
  }

  // Invalidates the entire contents of the layer.
  void SchedulePaintForLayer(Layer* layer) {
    layer->SchedulePaint(
        gfx::Rect(0, 0, layer->bounds().width(), layer->bounds().height()));
  }

  // Invokes DrawTree on the compositor.
  void Draw() {
    compositor()->ScheduleDraw();
    WaitForDraw();
  }

  void WaitForDraw() {
    DrawWaiterForTest::Wait(compositor());
  }

  void WaitForCommit() {
    DrawWaiterForTest::WaitForCommit(compositor());
  }

 private:
  scoped_ptr<TestCompositorHost> compositor_host_;

  DISALLOW_COPY_AND_ASSIGN(LayerWithDelegateTest);
};

// L1
//  +-- L2
TEST_F(LayerWithDelegateTest, ConvertPointToLayer_Simple) {
  scoped_ptr<Layer> l1(CreateColorLayer(SK_ColorRED,
                                        gfx::Rect(20, 20, 400, 400)));
  scoped_ptr<Layer> l2(CreateColorLayer(SK_ColorBLUE,
                                        gfx::Rect(10, 10, 350, 350)));
  l1->Add(l2.get());
  DrawTree(l1.get());

  gfx::Point point1_in_l2_coords(5, 5);
  Layer::ConvertPointToLayer(l2.get(), l1.get(), &point1_in_l2_coords);
  gfx::Point point1_in_l1_coords(15, 15);
  EXPECT_EQ(point1_in_l1_coords, point1_in_l2_coords);

  gfx::Point point2_in_l1_coords(5, 5);
  Layer::ConvertPointToLayer(l1.get(), l2.get(), &point2_in_l1_coords);
  gfx::Point point2_in_l2_coords(-5, -5);
  EXPECT_EQ(point2_in_l2_coords, point2_in_l1_coords);
}

// L1
//  +-- L2
//       +-- L3
TEST_F(LayerWithDelegateTest, ConvertPointToLayer_Medium) {
  scoped_ptr<Layer> l1(CreateColorLayer(SK_ColorRED,
                                        gfx::Rect(20, 20, 400, 400)));
  scoped_ptr<Layer> l2(CreateColorLayer(SK_ColorBLUE,
                                        gfx::Rect(10, 10, 350, 350)));
  scoped_ptr<Layer> l3(CreateColorLayer(SK_ColorYELLOW,
                                        gfx::Rect(10, 10, 100, 100)));
  l1->Add(l2.get());
  l2->Add(l3.get());
  DrawTree(l1.get());

  gfx::Point point1_in_l3_coords(5, 5);
  Layer::ConvertPointToLayer(l3.get(), l1.get(), &point1_in_l3_coords);
  gfx::Point point1_in_l1_coords(25, 25);
  EXPECT_EQ(point1_in_l1_coords, point1_in_l3_coords);

  gfx::Point point2_in_l1_coords(5, 5);
  Layer::ConvertPointToLayer(l1.get(), l3.get(), &point2_in_l1_coords);
  gfx::Point point2_in_l3_coords(-15, -15);
  EXPECT_EQ(point2_in_l3_coords, point2_in_l1_coords);
}

TEST_F(LayerWithRealCompositorTest, Delegate) {
  scoped_ptr<Layer> l1(CreateColorLayer(SK_ColorBLACK,
                                        gfx::Rect(20, 20, 400, 400)));
  GetCompositor()->SetRootLayer(l1.get());
  WaitForDraw();

  TestLayerDelegate delegate;
  l1->set_delegate(&delegate);
  delegate.AddColor(SK_ColorWHITE);
  delegate.AddColor(SK_ColorYELLOW);
  delegate.AddColor(SK_ColorGREEN);

  l1->SchedulePaint(gfx::Rect(0, 0, 400, 400));
  WaitForDraw();

  EXPECT_EQ(delegate.color_index(), 1);
  EXPECT_EQ(delegate.paint_size(), l1->bounds().size());

  l1->SchedulePaint(gfx::Rect(10, 10, 200, 200));
  WaitForDraw();
  EXPECT_EQ(delegate.color_index(), 2);
  EXPECT_EQ(delegate.paint_size(), gfx::Size(200, 200));

  l1->SchedulePaint(gfx::Rect(5, 5, 50, 50));
  WaitForDraw();
  EXPECT_EQ(delegate.color_index(), 0);
  EXPECT_EQ(delegate.paint_size(), gfx::Size(50, 50));
}

TEST_F(LayerWithRealCompositorTest, DrawTree) {
  scoped_ptr<Layer> l1(CreateColorLayer(SK_ColorRED,
                                        gfx::Rect(20, 20, 400, 400)));
  scoped_ptr<Layer> l2(CreateColorLayer(SK_ColorBLUE,
                                        gfx::Rect(10, 10, 350, 350)));
  scoped_ptr<Layer> l3(CreateColorLayer(SK_ColorYELLOW,
                                        gfx::Rect(10, 10, 100, 100)));
  l1->Add(l2.get());
  l2->Add(l3.get());

  GetCompositor()->SetRootLayer(l1.get());
  WaitForDraw();

  DrawTreeLayerDelegate d1;
  l1->set_delegate(&d1);
  DrawTreeLayerDelegate d2;
  l2->set_delegate(&d2);
  DrawTreeLayerDelegate d3;
  l3->set_delegate(&d3);

  l2->SchedulePaint(gfx::Rect(5, 5, 5, 5));
  WaitForDraw();
  EXPECT_FALSE(d1.painted());
  EXPECT_TRUE(d2.painted());
  EXPECT_FALSE(d3.painted());
}

// Tests no-texture Layers.
// Create this hierarchy:
// L1 - red
// +-- L2 - NO TEXTURE
// |   +-- L3 - yellow
// +-- L4 - magenta
//
TEST_F(LayerWithRealCompositorTest, HierarchyNoTexture) {
  scoped_ptr<Layer> l1(CreateColorLayer(SK_ColorRED,
                                        gfx::Rect(20, 20, 400, 400)));
  scoped_ptr<Layer> l2(CreateNoTextureLayer(gfx::Rect(10, 10, 350, 350)));
  scoped_ptr<Layer> l3(CreateColorLayer(SK_ColorYELLOW,
                                        gfx::Rect(5, 5, 25, 25)));
  scoped_ptr<Layer> l4(CreateColorLayer(SK_ColorMAGENTA,
                                        gfx::Rect(300, 300, 100, 100)));

  l1->Add(l2.get());
  l1->Add(l4.get());
  l2->Add(l3.get());

  GetCompositor()->SetRootLayer(l1.get());
  WaitForDraw();

  DrawTreeLayerDelegate d2;
  l2->set_delegate(&d2);
  DrawTreeLayerDelegate d3;
  l3->set_delegate(&d3);

  l2->SchedulePaint(gfx::Rect(5, 5, 5, 5));
  l3->SchedulePaint(gfx::Rect(5, 5, 5, 5));
  WaitForDraw();

  // |d2| should not have received a paint notification since it has no texture.
  EXPECT_FALSE(d2.painted());
  // |d3| should have received a paint notification.
  EXPECT_TRUE(d3.painted());
}

class LayerWithNullDelegateTest : public LayerWithDelegateTest {
 public:
  LayerWithNullDelegateTest() {}
  virtual ~LayerWithNullDelegateTest() {}

  virtual void SetUp() OVERRIDE {
    LayerWithDelegateTest::SetUp();
    default_layer_delegate_.reset(new NullLayerDelegate());
  }

  virtual Layer* CreateLayer(LayerType type) OVERRIDE {
    Layer* layer = new Layer(type);
    layer->set_delegate(default_layer_delegate_.get());
    return layer;
  }

  Layer* CreateTextureRootLayer(const gfx::Rect& bounds) {
    Layer* layer = CreateTextureLayer(bounds);
    compositor()->SetRootLayer(layer);
    return layer;
  }

  Layer* CreateTextureLayer(const gfx::Rect& bounds) {
    Layer* layer = CreateLayer(LAYER_TEXTURED);
    layer->SetBounds(bounds);
    return layer;
  }

  virtual Layer* CreateNoTextureLayer(const gfx::Rect& bounds) OVERRIDE {
    Layer* layer = CreateLayer(LAYER_NOT_DRAWN);
    layer->SetBounds(bounds);
    return layer;
  }

 private:
  scoped_ptr<NullLayerDelegate> default_layer_delegate_;

  DISALLOW_COPY_AND_ASSIGN(LayerWithNullDelegateTest);
};

TEST_F(LayerWithNullDelegateTest, EscapedDebugNames) {
  scoped_ptr<Layer> layer(CreateLayer(LAYER_NOT_DRAWN));
  std::string name = "\"\'\\/\b\f\n\r\t\n";
  layer->set_name(name);
  scoped_refptr<base::debug::ConvertableToTraceFormat> debug_info =
    layer->TakeDebugInfo();
  EXPECT_TRUE(!!debug_info);
  std::string json;
  debug_info->AppendAsTraceFormat(&json);
  base::JSONReader json_reader;
  scoped_ptr<base::Value> debug_info_value(json_reader.ReadToValue(json));
  EXPECT_TRUE(!!debug_info_value);
  EXPECT_TRUE(debug_info_value->IsType(base::Value::TYPE_DICTIONARY));
  base::DictionaryValue* dictionary = 0;
  EXPECT_TRUE(debug_info_value->GetAsDictionary(&dictionary));
  std::string roundtrip;
  EXPECT_TRUE(dictionary->GetString("layer_name", &roundtrip));
  EXPECT_EQ(name, roundtrip);
}

void ReturnMailbox(bool* run, uint32 sync_point, bool is_lost) {
  *run = true;
}

TEST_F(LayerWithNullDelegateTest, SwitchLayerPreservesCCLayerState) {
  scoped_ptr<Layer> l1(CreateColorLayer(SK_ColorRED,
                                        gfx::Rect(20, 20, 400, 400)));
  l1->SetFillsBoundsOpaquely(true);
  l1->SetForceRenderSurface(true);
  l1->SetVisible(false);

  EXPECT_EQ(gfx::Point3F(), l1->cc_layer()->transform_origin());
  EXPECT_TRUE(l1->cc_layer()->DrawsContent());
  EXPECT_TRUE(l1->cc_layer()->contents_opaque());
  EXPECT_TRUE(l1->cc_layer()->force_render_surface());
  EXPECT_TRUE(l1->cc_layer()->hide_layer_and_subtree());

  cc::Layer* before_layer = l1->cc_layer();

  bool callback1_run = false;
  cc::TextureMailbox mailbox(gpu::Mailbox::Generate(), 0, 0);
  l1->SetTextureMailbox(mailbox,
                        cc::SingleReleaseCallback::Create(
                            base::Bind(ReturnMailbox, &callback1_run)),
                        gfx::Size(1, 1));

  EXPECT_NE(before_layer, l1->cc_layer());

  EXPECT_EQ(gfx::Point3F(), l1->cc_layer()->transform_origin());
  EXPECT_TRUE(l1->cc_layer()->DrawsContent());
  EXPECT_TRUE(l1->cc_layer()->contents_opaque());
  EXPECT_TRUE(l1->cc_layer()->force_render_surface());
  EXPECT_TRUE(l1->cc_layer()->hide_layer_and_subtree());
  EXPECT_FALSE(callback1_run);

  bool callback2_run = false;
  mailbox = cc::TextureMailbox(gpu::Mailbox::Generate(), 0, 0);
  l1->SetTextureMailbox(mailbox,
                        cc::SingleReleaseCallback::Create(
                            base::Bind(ReturnMailbox, &callback2_run)),
                        gfx::Size(1, 1));
  EXPECT_TRUE(callback1_run);
  EXPECT_FALSE(callback2_run);

  l1->SetShowPaintedContent();
  EXPECT_EQ(gfx::Point3F(), l1->cc_layer()->transform_origin());
  EXPECT_TRUE(l1->cc_layer()->DrawsContent());
  EXPECT_TRUE(l1->cc_layer()->contents_opaque());
  EXPECT_TRUE(l1->cc_layer()->force_render_surface());
  EXPECT_TRUE(l1->cc_layer()->hide_layer_and_subtree());
  EXPECT_TRUE(callback2_run);
}

// Various visibile/drawn assertions.
TEST_F(LayerWithNullDelegateTest, Visibility) {
  scoped_ptr<Layer> l1(new Layer(LAYER_TEXTURED));
  scoped_ptr<Layer> l2(new Layer(LAYER_TEXTURED));
  scoped_ptr<Layer> l3(new Layer(LAYER_TEXTURED));
  l1->Add(l2.get());
  l2->Add(l3.get());

  NullLayerDelegate delegate;
  l1->set_delegate(&delegate);
  l2->set_delegate(&delegate);
  l3->set_delegate(&delegate);

  // Layers should initially be drawn.
  EXPECT_TRUE(l1->IsDrawn());
  EXPECT_TRUE(l2->IsDrawn());
  EXPECT_TRUE(l3->IsDrawn());
  EXPECT_FALSE(l1->cc_layer()->hide_layer_and_subtree());
  EXPECT_FALSE(l2->cc_layer()->hide_layer_and_subtree());
  EXPECT_FALSE(l3->cc_layer()->hide_layer_and_subtree());

  compositor()->SetRootLayer(l1.get());

  Draw();

  l1->SetVisible(false);
  EXPECT_FALSE(l1->IsDrawn());
  EXPECT_FALSE(l2->IsDrawn());
  EXPECT_FALSE(l3->IsDrawn());
  EXPECT_TRUE(l1->cc_layer()->hide_layer_and_subtree());
  EXPECT_FALSE(l2->cc_layer()->hide_layer_and_subtree());
  EXPECT_FALSE(l3->cc_layer()->hide_layer_and_subtree());

  l3->SetVisible(false);
  EXPECT_FALSE(l1->IsDrawn());
  EXPECT_FALSE(l2->IsDrawn());
  EXPECT_FALSE(l3->IsDrawn());
  EXPECT_TRUE(l1->cc_layer()->hide_layer_and_subtree());
  EXPECT_FALSE(l2->cc_layer()->hide_layer_and_subtree());
  EXPECT_TRUE(l3->cc_layer()->hide_layer_and_subtree());

  l1->SetVisible(true);
  EXPECT_TRUE(l1->IsDrawn());
  EXPECT_TRUE(l2->IsDrawn());
  EXPECT_FALSE(l3->IsDrawn());
  EXPECT_FALSE(l1->cc_layer()->hide_layer_and_subtree());
  EXPECT_FALSE(l2->cc_layer()->hide_layer_and_subtree());
  EXPECT_TRUE(l3->cc_layer()->hide_layer_and_subtree());
}

// Checks that stacking-related methods behave as advertised.
TEST_F(LayerWithNullDelegateTest, Stacking) {
  scoped_ptr<Layer> root(new Layer(LAYER_NOT_DRAWN));
  scoped_ptr<Layer> l1(new Layer(LAYER_TEXTURED));
  scoped_ptr<Layer> l2(new Layer(LAYER_TEXTURED));
  scoped_ptr<Layer> l3(new Layer(LAYER_TEXTURED));
  l1->set_name("1");
  l2->set_name("2");
  l3->set_name("3");
  root->Add(l3.get());
  root->Add(l2.get());
  root->Add(l1.get());

  // Layers' children are stored in bottom-to-top order.
  EXPECT_EQ("3 2 1", test::ChildLayerNamesAsString(*root.get()));

  root->StackAtTop(l3.get());
  EXPECT_EQ("2 1 3", test::ChildLayerNamesAsString(*root.get()));

  root->StackAtTop(l1.get());
  EXPECT_EQ("2 3 1", test::ChildLayerNamesAsString(*root.get()));

  root->StackAtTop(l1.get());
  EXPECT_EQ("2 3 1", test::ChildLayerNamesAsString(*root.get()));

  root->StackAbove(l2.get(), l3.get());
  EXPECT_EQ("3 2 1", test::ChildLayerNamesAsString(*root.get()));

  root->StackAbove(l1.get(), l3.get());
  EXPECT_EQ("3 1 2", test::ChildLayerNamesAsString(*root.get()));

  root->StackAbove(l2.get(), l1.get());
  EXPECT_EQ("3 1 2", test::ChildLayerNamesAsString(*root.get()));

  root->StackAtBottom(l2.get());
  EXPECT_EQ("2 3 1", test::ChildLayerNamesAsString(*root.get()));

  root->StackAtBottom(l3.get());
  EXPECT_EQ("3 2 1", test::ChildLayerNamesAsString(*root.get()));

  root->StackAtBottom(l3.get());
  EXPECT_EQ("3 2 1", test::ChildLayerNamesAsString(*root.get()));

  root->StackBelow(l2.get(), l3.get());
  EXPECT_EQ("2 3 1", test::ChildLayerNamesAsString(*root.get()));

  root->StackBelow(l1.get(), l3.get());
  EXPECT_EQ("2 1 3", test::ChildLayerNamesAsString(*root.get()));

  root->StackBelow(l3.get(), l2.get());
  EXPECT_EQ("3 2 1", test::ChildLayerNamesAsString(*root.get()));

  root->StackBelow(l3.get(), l2.get());
  EXPECT_EQ("3 2 1", test::ChildLayerNamesAsString(*root.get()));

  root->StackBelow(l3.get(), l1.get());
  EXPECT_EQ("2 3 1", test::ChildLayerNamesAsString(*root.get()));
}

// Verifies SetBounds triggers the appropriate painting/drawing.
TEST_F(LayerWithNullDelegateTest, SetBoundsSchedulesPaint) {
  scoped_ptr<Layer> l1(CreateTextureLayer(gfx::Rect(0, 0, 200, 200)));
  compositor()->SetRootLayer(l1.get());

  Draw();

  l1->SetBounds(gfx::Rect(5, 5, 200, 200));

  // The CompositorDelegate (us) should have been told to draw for a move.
  WaitForDraw();

  l1->SetBounds(gfx::Rect(5, 5, 100, 100));

  // The CompositorDelegate (us) should have been told to draw for a resize.
  WaitForDraw();
}

// Checks that pixels are actually drawn to the screen with a read back.
TEST_F(LayerWithRealCompositorTest, DrawPixels) {
  gfx::Size viewport_size = GetCompositor()->size();

  // The window should be some non-trivial size but may not be exactly
  // 500x500 on all platforms/bots.
  EXPECT_GE(viewport_size.width(), 200);
  EXPECT_GE(viewport_size.height(), 200);

  int blue_height = 10;

  scoped_ptr<Layer> layer(
      CreateColorLayer(SK_ColorRED, gfx::Rect(viewport_size)));
  scoped_ptr<Layer> layer2(
      CreateColorLayer(SK_ColorBLUE,
                       gfx::Rect(0, 0, viewport_size.width(), blue_height)));

  layer->Add(layer2.get());

  DrawTree(layer.get());

  SkBitmap bitmap;
  ASSERT_TRUE(ReadPixels(&bitmap, gfx::Rect(viewport_size)));
  ASSERT_FALSE(bitmap.empty());

  SkAutoLockPixels lock(bitmap);
  for (int x = 0; x < viewport_size.width(); x++) {
    for (int y = 0; y < viewport_size.height(); y++) {
      SkColor actual_color = bitmap.getColor(x, y);
      SkColor expected_color = y < blue_height ? SK_ColorBLUE : SK_ColorRED;
      EXPECT_EQ(expected_color, actual_color)
          << "Pixel error at x=" << x << " y=" << y << "; "
          << "actual RGBA=("
          << SkColorGetR(actual_color) << ","
          << SkColorGetG(actual_color) << ","
          << SkColorGetB(actual_color) << ","
          << SkColorGetA(actual_color) << "); "
          << "expected RGBA=("
          << SkColorGetR(expected_color) << ","
          << SkColorGetG(expected_color) << ","
          << SkColorGetB(expected_color) << ","
          << SkColorGetA(expected_color) << ")";
    }
  }
}

// Checks the logic around Compositor::SetRootLayer and Layer::SetCompositor.
TEST_F(LayerWithRealCompositorTest, SetRootLayer) {
  Compositor* compositor = GetCompositor();
  scoped_ptr<Layer> l1(CreateColorLayer(SK_ColorRED,
                                        gfx::Rect(20, 20, 400, 400)));
  scoped_ptr<Layer> l2(CreateColorLayer(SK_ColorBLUE,
                                        gfx::Rect(10, 10, 350, 350)));

  EXPECT_EQ(NULL, l1->GetCompositor());
  EXPECT_EQ(NULL, l2->GetCompositor());

  compositor->SetRootLayer(l1.get());
  EXPECT_EQ(compositor, l1->GetCompositor());

  l1->Add(l2.get());
  EXPECT_EQ(compositor, l2->GetCompositor());

  l1->Remove(l2.get());
  EXPECT_EQ(NULL, l2->GetCompositor());

  l1->Add(l2.get());
  EXPECT_EQ(compositor, l2->GetCompositor());

  compositor->SetRootLayer(NULL);
  EXPECT_EQ(NULL, l1->GetCompositor());
  EXPECT_EQ(NULL, l2->GetCompositor());
}

// Checks that compositor observers are notified when:
// - DrawTree is called,
// - After ScheduleDraw is called, or
// - Whenever SetBounds, SetOpacity or SetTransform are called.
// TODO(vollick): could be reorganized into compositor_unittest.cc
TEST_F(LayerWithRealCompositorTest, CompositorObservers) {
  scoped_ptr<Layer> l1(CreateColorLayer(SK_ColorRED,
                                        gfx::Rect(20, 20, 400, 400)));
  scoped_ptr<Layer> l2(CreateColorLayer(SK_ColorBLUE,
                                        gfx::Rect(10, 10, 350, 350)));
  l1->Add(l2.get());
  TestCompositorObserver observer;
  GetCompositor()->AddObserver(&observer);

  // Explicitly called DrawTree should cause the observers to be notified.
  // NOTE: this call to DrawTree sets l1 to be the compositor's root layer.
  DrawTree(l1.get());
  EXPECT_TRUE(observer.notified());

  // ScheduleDraw without any visible change should cause a commit.
  observer.Reset();
  l1->ScheduleDraw();
  WaitForCommit();
  EXPECT_TRUE(observer.committed());

  // Moving, but not resizing, a layer should alert the observers.
  observer.Reset();
  l2->SetBounds(gfx::Rect(0, 0, 350, 350));
  WaitForDraw();
  EXPECT_TRUE(observer.notified());

  // So should resizing a layer.
  observer.Reset();
  l2->SetBounds(gfx::Rect(0, 0, 400, 400));
  WaitForDraw();
  EXPECT_TRUE(observer.notified());

  // Opacity changes should alert the observers.
  observer.Reset();
  l2->SetOpacity(0.5f);
  WaitForDraw();
  EXPECT_TRUE(observer.notified());

  // So should setting the opacity back.
  observer.Reset();
  l2->SetOpacity(1.0f);
  WaitForDraw();
  EXPECT_TRUE(observer.notified());

  // Setting the transform of a layer should alert the observers.
  observer.Reset();
  gfx::Transform transform;
  transform.Translate(200.0, 200.0);
  transform.Rotate(90.0);
  transform.Translate(-200.0, -200.0);
  l2->SetTransform(transform);
  WaitForDraw();
  EXPECT_TRUE(observer.notified());

  // A change resulting in an aborted swap buffer should alert the observer
  // and also signal an abort.
  observer.Reset();
  l2->SetOpacity(0.1f);
  GetCompositor()->DidAbortSwapBuffers();
  WaitForDraw();
  EXPECT_TRUE(observer.notified());
  EXPECT_TRUE(observer.aborted());

  GetCompositor()->RemoveObserver(&observer);

  // Opacity changes should no longer alert the removed observer.
  observer.Reset();
  l2->SetOpacity(0.5f);
  WaitForDraw();

  EXPECT_FALSE(observer.notified());
}

// Checks that modifying the hierarchy correctly affects final composite.
TEST_F(LayerWithRealCompositorTest, ModifyHierarchy) {
  GetCompositor()->SetScaleAndSize(1.0f, gfx::Size(50, 50));

  // l0
  //  +-l11
  //  | +-l21
  //  +-l12
  scoped_ptr<Layer> l0(CreateColorLayer(SK_ColorRED,
                                        gfx::Rect(0, 0, 50, 50)));
  scoped_ptr<Layer> l11(CreateColorLayer(SK_ColorGREEN,
                                         gfx::Rect(0, 0, 25, 25)));
  scoped_ptr<Layer> l21(CreateColorLayer(SK_ColorMAGENTA,
                                         gfx::Rect(0, 0, 15, 15)));
  scoped_ptr<Layer> l12(CreateColorLayer(SK_ColorBLUE,
                                         gfx::Rect(10, 10, 25, 25)));

  base::FilePath ref_img1 =
      test_data_directory().AppendASCII("ModifyHierarchy1.png");
  base::FilePath ref_img2 =
      test_data_directory().AppendASCII("ModifyHierarchy2.png");
  SkBitmap bitmap;

  l0->Add(l11.get());
  l11->Add(l21.get());
  l0->Add(l12.get());
  DrawTree(l0.get());
  ASSERT_TRUE(ReadPixels(&bitmap));
  ASSERT_FALSE(bitmap.empty());
  // WritePNGFile(bitmap, ref_img1);
  EXPECT_TRUE(MatchesPNGFile(bitmap, ref_img1, cc::ExactPixelComparator(true)));

  l0->StackAtTop(l11.get());
  DrawTree(l0.get());
  ASSERT_TRUE(ReadPixels(&bitmap));
  ASSERT_FALSE(bitmap.empty());
  // WritePNGFile(bitmap, ref_img2);
  EXPECT_TRUE(MatchesPNGFile(bitmap, ref_img2, cc::ExactPixelComparator(true)));

  // should restore to original configuration
  l0->StackAbove(l12.get(), l11.get());
  DrawTree(l0.get());
  ASSERT_TRUE(ReadPixels(&bitmap));
  ASSERT_FALSE(bitmap.empty());
  EXPECT_TRUE(MatchesPNGFile(bitmap, ref_img1, cc::ExactPixelComparator(true)));

  // l11 back to front
  l0->StackAtTop(l11.get());
  DrawTree(l0.get());
  ASSERT_TRUE(ReadPixels(&bitmap));
  ASSERT_FALSE(bitmap.empty());
  EXPECT_TRUE(MatchesPNGFile(bitmap, ref_img2, cc::ExactPixelComparator(true)));

  // should restore to original configuration
  l0->StackAbove(l12.get(), l11.get());
  DrawTree(l0.get());
  ASSERT_TRUE(ReadPixels(&bitmap));
  ASSERT_FALSE(bitmap.empty());
  EXPECT_TRUE(MatchesPNGFile(bitmap, ref_img1, cc::ExactPixelComparator(true)));

  // l11 back to front
  l0->StackAbove(l11.get(), l12.get());
  DrawTree(l0.get());
  ASSERT_TRUE(ReadPixels(&bitmap));
  ASSERT_FALSE(bitmap.empty());
  EXPECT_TRUE(MatchesPNGFile(bitmap, ref_img2, cc::ExactPixelComparator(true)));
}

// Opacity is rendered correctly.
// Checks that modifying the hierarchy correctly affects final composite.
TEST_F(LayerWithRealCompositorTest, Opacity) {
  GetCompositor()->SetScaleAndSize(1.0f, gfx::Size(50, 50));

  // l0
  //  +-l11
  scoped_ptr<Layer> l0(CreateColorLayer(SK_ColorRED,
                                        gfx::Rect(0, 0, 50, 50)));
  scoped_ptr<Layer> l11(CreateColorLayer(SK_ColorGREEN,
                                         gfx::Rect(0, 0, 25, 25)));

  base::FilePath ref_img = test_data_directory().AppendASCII("Opacity.png");

  l11->SetOpacity(0.75);
  l0->Add(l11.get());
  DrawTree(l0.get());
  SkBitmap bitmap;
  ASSERT_TRUE(ReadPixels(&bitmap));
  ASSERT_FALSE(bitmap.empty());
  // WritePNGFile(bitmap, ref_img);
  EXPECT_TRUE(MatchesPNGFile(bitmap, ref_img, cc::ExactPixelComparator(true)));
}

namespace {

class SchedulePaintLayerDelegate : public LayerDelegate {
 public:
  SchedulePaintLayerDelegate() : paint_count_(0), layer_(NULL) {}

  virtual ~SchedulePaintLayerDelegate() {}

  void set_layer(Layer* layer) {
    layer_ = layer;
    layer_->set_delegate(this);
  }

  void SetSchedulePaintRect(const gfx::Rect& rect) {
    schedule_paint_rect_ = rect;
  }

  int GetPaintCountAndClear() {
    int value = paint_count_;
    paint_count_ = 0;
    return value;
  }

  const gfx::RectF& last_clip_rect() const { return last_clip_rect_; }

 private:
  // Overridden from LayerDelegate:
  virtual void OnPaintLayer(gfx::Canvas* canvas) OVERRIDE {
    paint_count_++;
    if (!schedule_paint_rect_.IsEmpty()) {
      layer_->SchedulePaint(schedule_paint_rect_);
      schedule_paint_rect_ = gfx::Rect();
    }
    SkRect sk_clip_rect;
    if (canvas->sk_canvas()->getClipBounds(&sk_clip_rect))
      last_clip_rect_ = gfx::SkRectToRectF(sk_clip_rect);
  }

  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE {
  }

  virtual base::Closure PrepareForLayerBoundsChange() OVERRIDE {
    return base::Closure();
  }

  int paint_count_;
  Layer* layer_;
  gfx::Rect schedule_paint_rect_;
  gfx::RectF last_clip_rect_;

  DISALLOW_COPY_AND_ASSIGN(SchedulePaintLayerDelegate);
};

}  // namespace

// Verifies that if SchedulePaint is invoked during painting the layer is still
// marked dirty.
TEST_F(LayerWithDelegateTest, SchedulePaintFromOnPaintLayer) {
  scoped_ptr<Layer> root(CreateColorLayer(SK_ColorRED,
                                          gfx::Rect(0, 0, 500, 500)));
  SchedulePaintLayerDelegate child_delegate;
  scoped_ptr<Layer> child(CreateColorLayer(SK_ColorBLUE,
                                           gfx::Rect(0, 0, 200, 200)));
  child_delegate.set_layer(child.get());

  root->Add(child.get());

  SchedulePaintForLayer(root.get());
  DrawTree(root.get());
  child->SchedulePaint(gfx::Rect(0, 0, 20, 20));
  EXPECT_EQ(1, child_delegate.GetPaintCountAndClear());

  // Set a rect so that when OnPaintLayer() is invoked SchedulePaint is invoked
  // again.
  child_delegate.SetSchedulePaintRect(gfx::Rect(10, 10, 30, 30));
  WaitForCommit();
  EXPECT_EQ(1, child_delegate.GetPaintCountAndClear());

  // Because SchedulePaint() was invoked from OnPaintLayer() |child| should
  // still need to be painted.
  WaitForCommit();
  EXPECT_EQ(1, child_delegate.GetPaintCountAndClear());
  EXPECT_TRUE(child_delegate.last_clip_rect().Contains(
                  gfx::Rect(10, 10, 30, 30)));
}

TEST_F(LayerWithRealCompositorTest, ScaleUpDown) {
  scoped_ptr<Layer> root(CreateColorLayer(SK_ColorWHITE,
                                          gfx::Rect(10, 20, 200, 220)));
  TestLayerDelegate root_delegate;
  root_delegate.AddColor(SK_ColorWHITE);
  root->set_delegate(&root_delegate);

  scoped_ptr<Layer> l1(CreateColorLayer(SK_ColorWHITE,
                                        gfx::Rect(10, 20, 140, 180)));
  TestLayerDelegate l1_delegate;
  l1_delegate.AddColor(SK_ColorWHITE);
  l1->set_delegate(&l1_delegate);

  GetCompositor()->SetScaleAndSize(1.0f, gfx::Size(500, 500));
  GetCompositor()->SetRootLayer(root.get());
  root->Add(l1.get());
  WaitForDraw();

  EXPECT_EQ("10,20 200x220", root->bounds().ToString());
  EXPECT_EQ("10,20 140x180", l1->bounds().ToString());
  gfx::Size cc_bounds_size = root->cc_layer()->bounds();
  EXPECT_EQ("200x220", cc_bounds_size.ToString());
  cc_bounds_size = l1->cc_layer()->bounds();
  EXPECT_EQ("140x180", cc_bounds_size.ToString());
  // No scale change, so no scale notification.
  EXPECT_EQ(0.0f, root_delegate.device_scale_factor());
  EXPECT_EQ(0.0f, l1_delegate.device_scale_factor());

  EXPECT_EQ("200x220", root_delegate.paint_size().ToString());
  EXPECT_EQ("140x180", l1_delegate.paint_size().ToString());

  // Scale up to 2.0. Changing scale doesn't change the bounds in DIP.
  GetCompositor()->SetScaleAndSize(2.0f, gfx::Size(500, 500));
  EXPECT_EQ("10,20 200x220", root->bounds().ToString());
  EXPECT_EQ("10,20 140x180", l1->bounds().ToString());
  // CC layer should still match the UI layer bounds.
  cc_bounds_size = root->cc_layer()->bounds();
  EXPECT_EQ("200x220", cc_bounds_size.ToString());
  cc_bounds_size = l1->cc_layer()->bounds();
  EXPECT_EQ("140x180", cc_bounds_size.ToString());
  // New scale factor must have been notified.
  EXPECT_EQ(2.0f, root_delegate.device_scale_factor());
  EXPECT_EQ(2.0f, l1_delegate.device_scale_factor());

  // Canvas size must have been scaled down up.
  WaitForDraw();
  EXPECT_EQ("400x440", root_delegate.paint_size().ToString());
  EXPECT_EQ("2.0 2.0", root_delegate.ToScaleString());
  EXPECT_EQ("280x360", l1_delegate.paint_size().ToString());
  EXPECT_EQ("2.0 2.0", l1_delegate.ToScaleString());

  // Scale down back to 1.0f.
  GetCompositor()->SetScaleAndSize(1.0f, gfx::Size(500, 500));
  EXPECT_EQ("10,20 200x220", root->bounds().ToString());
  EXPECT_EQ("10,20 140x180", l1->bounds().ToString());
  // CC layer should still match the UI layer bounds.
  cc_bounds_size = root->cc_layer()->bounds();
  EXPECT_EQ("200x220", cc_bounds_size.ToString());
  cc_bounds_size = l1->cc_layer()->bounds();
  EXPECT_EQ("140x180", cc_bounds_size.ToString());
  // New scale factor must have been notified.
  EXPECT_EQ(1.0f, root_delegate.device_scale_factor());
  EXPECT_EQ(1.0f, l1_delegate.device_scale_factor());

  // Canvas size must have been scaled down too.
  WaitForDraw();
  EXPECT_EQ("200x220", root_delegate.paint_size().ToString());
  EXPECT_EQ("1.0 1.0", root_delegate.ToScaleString());
  EXPECT_EQ("140x180", l1_delegate.paint_size().ToString());
  EXPECT_EQ("1.0 1.0", l1_delegate.ToScaleString());

  root_delegate.reset();
  l1_delegate.reset();
  // Just changing the size shouldn't notify the scale change nor
  // trigger repaint.
  GetCompositor()->SetScaleAndSize(1.0f, gfx::Size(1000, 1000));
  // No scale change, so no scale notification.
  EXPECT_EQ(0.0f, root_delegate.device_scale_factor());
  EXPECT_EQ(0.0f, l1_delegate.device_scale_factor());
  WaitForDraw();
  EXPECT_EQ("0x0", root_delegate.paint_size().ToString());
  EXPECT_EQ("0.0 0.0", root_delegate.ToScaleString());
  EXPECT_EQ("0x0", l1_delegate.paint_size().ToString());
  EXPECT_EQ("0.0 0.0", l1_delegate.ToScaleString());
}

TEST_F(LayerWithRealCompositorTest, ScaleReparent) {
  scoped_ptr<Layer> root(CreateColorLayer(SK_ColorWHITE,
                                          gfx::Rect(10, 20, 200, 220)));
  scoped_ptr<Layer> l1(CreateColorLayer(SK_ColorWHITE,
                                        gfx::Rect(10, 20, 140, 180)));
  TestLayerDelegate l1_delegate;
  l1_delegate.AddColor(SK_ColorWHITE);
  l1->set_delegate(&l1_delegate);

  GetCompositor()->SetScaleAndSize(1.0f, gfx::Size(500, 500));
  GetCompositor()->SetRootLayer(root.get());
  WaitForDraw();

  root->Add(l1.get());
  EXPECT_EQ("10,20 140x180", l1->bounds().ToString());
  gfx::Size cc_bounds_size = l1->cc_layer()->bounds();
  EXPECT_EQ("140x180", cc_bounds_size.ToString());
  EXPECT_EQ(0.0f, l1_delegate.device_scale_factor());

  WaitForDraw();
  EXPECT_EQ("140x180", l1_delegate.paint_size().ToString());
  EXPECT_EQ("1.0 1.0", l1_delegate.ToScaleString());

  // Remove l1 from root and change the scale.
  root->Remove(l1.get());
  EXPECT_EQ(NULL, l1->parent());
  EXPECT_EQ(NULL, l1->GetCompositor());
  GetCompositor()->SetScaleAndSize(2.0f, gfx::Size(500, 500));
  // Sanity check on root and l1.
  EXPECT_EQ("10,20 200x220", root->bounds().ToString());
  cc_bounds_size = l1->cc_layer()->bounds();
  EXPECT_EQ("140x180", cc_bounds_size.ToString());

  root->Add(l1.get());
  EXPECT_EQ("10,20 140x180", l1->bounds().ToString());
  cc_bounds_size = l1->cc_layer()->bounds();
  EXPECT_EQ("140x180", cc_bounds_size.ToString());
  EXPECT_EQ(2.0f, l1_delegate.device_scale_factor());
  WaitForDraw();
  EXPECT_EQ("280x360", l1_delegate.paint_size().ToString());
  EXPECT_EQ("2.0 2.0", l1_delegate.ToScaleString());
}

// Verifies that when changing bounds on a layer that is invisible, and then
// made visible, the right thing happens:
// - if just a move, then no painting should happen.
// - if a resize, the layer should be repainted.
TEST_F(LayerWithDelegateTest, SetBoundsWhenInvisible) {
  scoped_ptr<Layer> root(CreateNoTextureLayer(gfx::Rect(0, 0, 1000, 1000)));

  scoped_ptr<Layer> child(CreateLayer(LAYER_TEXTURED));
  child->SetBounds(gfx::Rect(0, 0, 500, 500));
  DrawTreeLayerDelegate delegate;
  child->set_delegate(&delegate);
  root->Add(child.get());

  // Paint once for initial damage.
  child->SetVisible(true);
  DrawTree(root.get());

  // Reset into invisible state.
  child->SetVisible(false);
  DrawTree(root.get());
  delegate.Reset();

  // Move layer.
  child->SetBounds(gfx::Rect(200, 200, 500, 500));
  child->SetVisible(true);
  DrawTree(root.get());
  EXPECT_FALSE(delegate.painted());

  // Reset into invisible state.
  child->SetVisible(false);
  DrawTree(root.get());
  delegate.Reset();

  // Resize layer.
  child->SetBounds(gfx::Rect(200, 200, 400, 400));
  child->SetVisible(true);
  DrawTree(root.get());
  EXPECT_TRUE(delegate.painted());
}

static scoped_ptr<cc::DelegatedFrameData> MakeFrameData(gfx::Size size) {
  scoped_ptr<cc::DelegatedFrameData> frame_data(new cc::DelegatedFrameData);
  scoped_ptr<cc::RenderPass> render_pass(cc::RenderPass::Create());
  render_pass->SetNew(
      cc::RenderPass::Id(1, 1), gfx::Rect(size), gfx::Rect(), gfx::Transform());
  frame_data->render_pass_list.push_back(render_pass.Pass());
  return frame_data.Pass();
}

TEST_F(LayerWithDelegateTest, DelegatedLayer) {
  scoped_ptr<Layer> root(CreateNoTextureLayer(gfx::Rect(0, 0, 1000, 1000)));

  scoped_ptr<Layer> child(CreateLayer(LAYER_TEXTURED));

  child->SetBounds(gfx::Rect(0, 0, 10, 10));
  child->SetVisible(true);
  root->Add(child.get());
  DrawTree(root.get());

  scoped_refptr<cc::DelegatedFrameResourceCollection> resource_collection =
      new cc::DelegatedFrameResourceCollection;
  scoped_refptr<cc::DelegatedFrameProvider> frame_provider;

  // Content matches layer size.
  frame_provider = new cc::DelegatedFrameProvider(
      resource_collection.get(), MakeFrameData(gfx::Size(10, 10)));
  child->SetShowDelegatedContent(frame_provider, gfx::Size(10, 10));
  EXPECT_EQ(child->cc_layer()->bounds().ToString(),
            gfx::Size(10, 10).ToString());

  // Content larger than layer.
  child->SetBounds(gfx::Rect(0, 0, 5, 5));
  EXPECT_EQ(child->cc_layer()->bounds().ToString(),
            gfx::Size(5, 5).ToString());

  // Content smaller than layer.
  child->SetBounds(gfx::Rect(0, 0, 10, 10));
  frame_provider = new cc::DelegatedFrameProvider(
      resource_collection.get(), MakeFrameData(gfx::Size(5, 5)));
  child->SetShowDelegatedContent(frame_provider, gfx::Size(5, 5));
  EXPECT_EQ(child->cc_layer()->bounds().ToString(), gfx::Size(5, 5).ToString());

  // Hi-DPI content on low-DPI layer.
  frame_provider = new cc::DelegatedFrameProvider(
      resource_collection.get(), MakeFrameData(gfx::Size(20, 20)));
  child->SetShowDelegatedContent(frame_provider, gfx::Size(10, 10));
  EXPECT_EQ(child->cc_layer()->bounds().ToString(),
            gfx::Size(10, 10).ToString());

  // Hi-DPI content on hi-DPI layer.
  compositor()->SetScaleAndSize(2.f, gfx::Size(1000, 1000));
  EXPECT_EQ(child->cc_layer()->bounds().ToString(),
            gfx::Size(10, 10).ToString());

  // Low-DPI content on hi-DPI layer.
  frame_provider = new cc::DelegatedFrameProvider(
      resource_collection.get(), MakeFrameData(gfx::Size(10, 10)));
  child->SetShowDelegatedContent(frame_provider, gfx::Size(10, 10));
  EXPECT_EQ(child->cc_layer()->bounds().ToString(),
            gfx::Size(10, 10).ToString());
}

TEST_F(LayerWithDelegateTest, ExternalContent) {
  scoped_ptr<Layer> root(CreateNoTextureLayer(gfx::Rect(0, 0, 1000, 1000)));
  scoped_ptr<Layer> child(CreateLayer(LAYER_TEXTURED));

  child->SetBounds(gfx::Rect(0, 0, 10, 10));
  child->SetVisible(true);
  root->Add(child.get());

  // The layer is already showing painted content, so the cc layer won't change.
  scoped_refptr<cc::Layer> before = child->cc_layer();
  child->SetShowPaintedContent();
  EXPECT_TRUE(child->cc_layer());
  EXPECT_EQ(before, child->cc_layer());

  scoped_refptr<cc::DelegatedFrameResourceCollection> resource_collection =
      new cc::DelegatedFrameResourceCollection;
  scoped_refptr<cc::DelegatedFrameProvider> frame_provider =
      new cc::DelegatedFrameProvider(resource_collection.get(),
                                     MakeFrameData(gfx::Size(10, 10)));

  // Showing delegated content changes the underlying cc layer.
  before = child->cc_layer();
  child->SetShowDelegatedContent(frame_provider, gfx::Size(10, 10));
  EXPECT_TRUE(child->cc_layer());
  EXPECT_NE(before, child->cc_layer());

  // Changing to painted content should change the underlying cc layer.
  before = child->cc_layer();
  child->SetShowPaintedContent();
  EXPECT_TRUE(child->cc_layer());
  EXPECT_NE(before, child->cc_layer());
}

// Tests Layer::AddThreadedAnimation and Layer::RemoveThreadedAnimation.
TEST_F(LayerWithRealCompositorTest, AddRemoveThreadedAnimations) {
  scoped_ptr<Layer> root(CreateLayer(LAYER_TEXTURED));
  scoped_ptr<Layer> l1(CreateLayer(LAYER_TEXTURED));
  scoped_ptr<Layer> l2(CreateLayer(LAYER_TEXTURED));

  l1->SetAnimator(LayerAnimator::CreateImplicitAnimator());
  l2->SetAnimator(LayerAnimator::CreateImplicitAnimator());

  EXPECT_FALSE(l1->HasPendingThreadedAnimations());

  // Trigger a threaded animation.
  l1->SetOpacity(0.5f);

  EXPECT_TRUE(l1->HasPendingThreadedAnimations());

  // Ensure we can remove a pending threaded animation.
  l1->GetAnimator()->StopAnimating();

  EXPECT_FALSE(l1->HasPendingThreadedAnimations());

  // Trigger another threaded animation.
  l1->SetOpacity(0.2f);

  EXPECT_TRUE(l1->HasPendingThreadedAnimations());

  root->Add(l1.get());
  GetCompositor()->SetRootLayer(root.get());

  // Now that l1 is part of a tree, it should have dispatched the pending
  // animation.
  EXPECT_FALSE(l1->HasPendingThreadedAnimations());

  // Ensure that l1 no longer holds on to animations.
  l1->SetOpacity(0.1f);
  EXPECT_FALSE(l1->HasPendingThreadedAnimations());

  // Ensure that adding a layer to an existing tree causes its pending
  // animations to get dispatched.
  l2->SetOpacity(0.5f);
  EXPECT_TRUE(l2->HasPendingThreadedAnimations());

  l1->Add(l2.get());
  EXPECT_FALSE(l2->HasPendingThreadedAnimations());
}

// Tests that in-progress threaded animations complete when a Layer's
// cc::Layer changes.
TEST_F(LayerWithRealCompositorTest, SwitchCCLayerAnimations) {
  scoped_ptr<Layer> root(CreateLayer(LAYER_TEXTURED));
  scoped_ptr<Layer> l1(CreateLayer(LAYER_TEXTURED));
  GetCompositor()->SetRootLayer(root.get());
  root->Add(l1.get());

  l1->SetAnimator(LayerAnimator::CreateImplicitAnimator());

  EXPECT_FLOAT_EQ(l1->opacity(), 1.0f);

  // Trigger a threaded animation.
  l1->SetOpacity(0.5f);

  // Change l1's cc::Layer.
  l1->SwitchCCLayerForTest();

  // Ensure that the opacity animation completed.
  EXPECT_FLOAT_EQ(l1->opacity(), 0.5f);
}

// Tests that the animators in the layer tree is added to the
// animator-collection when the root-layer is set to the compositor.
TEST_F(LayerWithDelegateTest, RootLayerAnimatorsInCompositor) {
  scoped_ptr<Layer> root(CreateLayer(LAYER_SOLID_COLOR));
  scoped_ptr<Layer> child(CreateColorLayer(SK_ColorRED, gfx::Rect(10, 10)));
  child->SetAnimator(LayerAnimator::CreateImplicitAnimator());
  child->SetOpacity(0.5f);
  root->Add(child.get());

  EXPECT_FALSE(compositor()->layer_animator_collection()->HasActiveAnimators());
  compositor()->SetRootLayer(root.get());
  EXPECT_TRUE(compositor()->layer_animator_collection()->HasActiveAnimators());
}

// Tests that adding/removing a layer adds/removes the animator from its entire
// subtree from the compositor's animator-collection.
TEST_F(LayerWithDelegateTest, AddRemoveLayerUpdatesAnimatorsFromSubtree) {
  scoped_ptr<Layer> root(CreateLayer(LAYER_TEXTURED));
  scoped_ptr<Layer> child(CreateLayer(LAYER_TEXTURED));
  scoped_ptr<Layer> grandchild(CreateColorLayer(SK_ColorRED,
                                                gfx::Rect(10, 10)));
  root->Add(child.get());
  child->Add(grandchild.get());
  compositor()->SetRootLayer(root.get());

  grandchild->SetAnimator(LayerAnimator::CreateImplicitAnimator());
  grandchild->SetOpacity(0.5f);
  EXPECT_TRUE(compositor()->layer_animator_collection()->HasActiveAnimators());

  root->Remove(child.get());
  EXPECT_FALSE(compositor()->layer_animator_collection()->HasActiveAnimators());

  root->Add(child.get());
  EXPECT_TRUE(compositor()->layer_animator_collection()->HasActiveAnimators());
}

TEST_F(LayerWithDelegateTest, DestroyingLayerRemovesTheAnimatorFromCollection) {
  scoped_ptr<Layer> root(CreateLayer(LAYER_TEXTURED));
  scoped_ptr<Layer> child(CreateLayer(LAYER_TEXTURED));
  root->Add(child.get());
  compositor()->SetRootLayer(root.get());

  child->SetAnimator(LayerAnimator::CreateImplicitAnimator());
  child->SetOpacity(0.5f);
  EXPECT_TRUE(compositor()->layer_animator_collection()->HasActiveAnimators());

  child.reset();
  EXPECT_FALSE(compositor()->layer_animator_collection()->HasActiveAnimators());
}

}  // namespace ui
