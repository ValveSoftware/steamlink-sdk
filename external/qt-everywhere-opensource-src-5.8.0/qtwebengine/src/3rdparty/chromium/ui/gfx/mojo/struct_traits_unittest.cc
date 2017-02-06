// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/mojo/traits_test_service.mojom.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/selection_bound.h"
#include "ui/gfx/transform.h"

namespace gfx {

namespace {

gfx::AcceleratedWidget castToAcceleratedWidget(int i) {
#if defined(USE_OZONE) || defined(USE_X11)
  return static_cast<gfx::AcceleratedWidget>(i);
#else
  return reinterpret_cast<gfx::AcceleratedWidget>(i);
#endif
}

class StructTraitsTest : public testing::Test, public mojom::TraitsTestService {
 public:
  StructTraitsTest() {}

 protected:
  mojom::TraitsTestServicePtr GetTraitsTestProxy() {
    return traits_test_bindings_.CreateInterfacePtrAndBind(this);
  }

 private:
  // TraitsTestService:
  void EchoSelectionBound(const SelectionBound& s,
                          const EchoSelectionBoundCallback& callback) override {
    callback.Run(s);
  }

  void EchoTransform(const Transform& t,
                     const EchoTransformCallback& callback) override {
    callback.Run(t);
  }

  void EchoAcceleratedWidget(
      const AcceleratedWidget& t,
      const EchoAcceleratedWidgetCallback& callback) override {
    callback.Run(t);
  }

  base::MessageLoop loop_;
  mojo::BindingSet<TraitsTestService> traits_test_bindings_;

  DISALLOW_COPY_AND_ASSIGN(StructTraitsTest);
};

}  // namespace

TEST_F(StructTraitsTest, SelectionBound) {
  const gfx::SelectionBound::Type type = gfx::SelectionBound::CENTER;
  const gfx::PointF edge_top(1234.5f, 5678.6f);
  const gfx::PointF edge_bottom(910112.5f, 13141516.6f);
  const bool visible = true;
  gfx::SelectionBound input;
  input.set_type(type);
  input.SetEdge(edge_top, edge_bottom);
  input.set_visible(visible);
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  gfx::SelectionBound output;
  proxy->EchoSelectionBound(input, &output);
  EXPECT_EQ(type, output.type());
  EXPECT_EQ(edge_top, output.edge_top());
  EXPECT_EQ(edge_bottom, output.edge_bottom());
  EXPECT_EQ(input.edge_top_rounded(), output.edge_top_rounded());
  EXPECT_EQ(input.edge_bottom_rounded(), output.edge_bottom_rounded());
  EXPECT_EQ(visible, output.visible());
}

TEST_F(StructTraitsTest, Transform) {
  const float col1row1 = 1.f;
  const float col2row1 = 2.f;
  const float col3row1 = 3.f;
  const float col4row1 = 4.f;
  const float col1row2 = 5.f;
  const float col2row2 = 6.f;
  const float col3row2 = 7.f;
  const float col4row2 = 8.f;
  const float col1row3 = 9.f;
  const float col2row3 = 10.f;
  const float col3row3 = 11.f;
  const float col4row3 = 12.f;
  const float col1row4 = 13.f;
  const float col2row4 = 14.f;
  const float col3row4 = 15.f;
  const float col4row4 = 16.f;
  gfx::Transform input(col1row1, col2row1, col3row1, col4row1, col1row2,
                       col2row2, col3row2, col4row2, col1row3, col2row3,
                       col3row3, col4row3, col1row4, col2row4, col3row4,
                       col4row4);
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  gfx::Transform output;
  proxy->EchoTransform(input, &output);
  EXPECT_EQ(col1row1, output.matrix().get(0, 0));
  EXPECT_EQ(col2row1, output.matrix().get(0, 1));
  EXPECT_EQ(col3row1, output.matrix().get(0, 2));
  EXPECT_EQ(col4row1, output.matrix().get(0, 3));
  EXPECT_EQ(col1row2, output.matrix().get(1, 0));
  EXPECT_EQ(col2row2, output.matrix().get(1, 1));
  EXPECT_EQ(col3row2, output.matrix().get(1, 2));
  EXPECT_EQ(col4row2, output.matrix().get(1, 3));
  EXPECT_EQ(col1row3, output.matrix().get(2, 0));
  EXPECT_EQ(col2row3, output.matrix().get(2, 1));
  EXPECT_EQ(col3row3, output.matrix().get(2, 2));
  EXPECT_EQ(col4row3, output.matrix().get(2, 3));
  EXPECT_EQ(col1row4, output.matrix().get(3, 0));
  EXPECT_EQ(col2row4, output.matrix().get(3, 1));
  EXPECT_EQ(col3row4, output.matrix().get(3, 2));
  EXPECT_EQ(col4row4, output.matrix().get(3, 3));
}

// AcceleratedWidgets can only be sent between processes on X11, Ozone, Win
#if defined(OS_WIN) || defined(USE_OZONE) || defined(USE_X11)
#define MAYBE_AcceleratedWidget AcceleratedWidget
#else
#define MAYBE_AcceleratedWidget DISABLED_AcceleratedWidget
#endif

TEST_F(StructTraitsTest, MAYBE_AcceleratedWidget) {
  gfx::AcceleratedWidget input(castToAcceleratedWidget(1001));
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  gfx::AcceleratedWidget output;
  proxy->EchoAcceleratedWidget(input, &output);
  EXPECT_EQ(input, output);
}

}  // namespace gfx
