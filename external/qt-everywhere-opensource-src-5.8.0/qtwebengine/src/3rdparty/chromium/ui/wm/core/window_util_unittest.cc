// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/window_util.h"

#include <memory>

#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/paint_context.h"

namespace wm {
namespace {

// Used to check if the delegate for recreated layer is created for
// the correct delegate.
class TestLayerDelegate : public ui::LayerDelegate {
 public:
  explicit TestLayerDelegate(ui::LayerDelegate* original_delegate)
      : original_delegate_(original_delegate) {}
  ~TestLayerDelegate() override = default;

  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override {}
  void OnDelegatedFrameDamage(const gfx::Rect& damage_rect_in_dip) override {}
  void OnDeviceScaleFactorChanged(float device_scale_factor) override {}
  base::Closure PrepareForLayerBoundsChange() override {
    return base::Closure();
  }

  ui::LayerDelegate* original_delegate() { return original_delegate_; }

 private:
  ui::LayerDelegate* original_delegate_;

  DISALLOW_COPY_AND_ASSIGN(TestLayerDelegate);
};

// Used to create a TestLayerDelegate for recreated layers.
class TestLayerDelegateFactory : public ::wm::LayerDelegateFactory {
 public:
  TestLayerDelegateFactory() = default;
  ~TestLayerDelegateFactory() override = default;

  // ::wm::LayerDelegateFactory:
  ui::LayerDelegate* CreateDelegate(ui::LayerDelegate* delegate) override {
    TestLayerDelegate* new_delegate = new TestLayerDelegate(delegate);
    delegates_.push_back(base::WrapUnique(new_delegate));
    return new_delegate;
  }

  size_t delegate_count() const { return delegates_.size(); }

  TestLayerDelegate* GetDelegateAt(int index) {
    return delegates_[index].get();
  }

 private:
  std::vector<std::unique_ptr<TestLayerDelegate>> delegates_;

  DISALLOW_COPY_AND_ASSIGN(TestLayerDelegateFactory);
};

}  // namespace

typedef aura::test::AuraTestBase WindowUtilTest;

// Test if the recreate layers does not recreate layers that have
// already been acquired.
TEST_F(WindowUtilTest, RecreateLayers) {
  std::unique_ptr<aura::Window> window1(
      aura::test::CreateTestWindowWithId(0, NULL));
  std::unique_ptr<aura::Window> window11(
      aura::test::CreateTestWindowWithId(1, window1.get()));
  std::unique_ptr<aura::Window> window12(
      aura::test::CreateTestWindowWithId(2, window1.get()));

  ASSERT_EQ(2u, window1->layer()->children().size());

  std::unique_ptr<ui::Layer> acquired(window11->AcquireLayer());
  EXPECT_TRUE(acquired.get());
  EXPECT_EQ(acquired.get(), window11->layer());

  std::unique_ptr<ui::LayerTreeOwner> tree =
      wm::RecreateLayers(window1.get(), nullptr);

  // The detached layer should not have the layer that has
  // already been detached.
  ASSERT_EQ(1u, tree->root()->children().size());
  // Child layer is new instance.
  EXPECT_NE(window11->layer(), tree->root()->children()[0]);
  EXPECT_NE(window12->layer(), tree->root()->children()[0]);

  // The original window should have both.
  ASSERT_EQ(2u, window1->layer()->children().size());
  EXPECT_EQ(window11->layer(), window1->layer()->children()[0]);
  EXPECT_EQ(window12->layer(), window1->layer()->children()[1]);

  // Delete the window before the acquired layer is deleted.
  window11.reset();
}

// Test if the LayerDelegateFactory creates new delegates for
// recreated layers correctly.
TEST_F(WindowUtilTest, RecreateLayersWithDelegate) {
  TestLayerDelegateFactory factory;
  std::unique_ptr<aura::Window> window1(
      aura::test::CreateTestWindowWithId(0, NULL));
  std::unique_ptr<aura::Window> window11(
      aura::test::CreateTestWindowWithId(1, window1.get()));
  std::unique_ptr<aura::Window> window12(
      aura::test::CreateTestWindowWithId(2, window1.get()));

  std::unique_ptr<ui::LayerTreeOwner> tree =
      wm::RecreateLayers(window1.get(), &factory);

  ASSERT_EQ(3u, factory.delegate_count());

  TestLayerDelegate* new_delegate_1 = factory.GetDelegateAt(0);
  TestLayerDelegate* new_delegate_11 = factory.GetDelegateAt(1);
  TestLayerDelegate* new_delegate_12 = factory.GetDelegateAt(2);

  EXPECT_EQ(window1->layer()->delegate(), new_delegate_1->original_delegate());
  EXPECT_EQ(window11->layer()->delegate(),
            new_delegate_11->original_delegate());
  EXPECT_EQ(window12->layer()->delegate(),
            new_delegate_12->original_delegate());

  EXPECT_EQ(tree->root()->delegate(), new_delegate_1);
  EXPECT_EQ(tree->root()->children()[0]->delegate(), new_delegate_11);
  EXPECT_EQ(tree->root()->children()[1]->delegate(), new_delegate_12);
}

}  // namespace wm
