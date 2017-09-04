// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/ui/public/interfaces/display/display_struct_traits_test.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace display {

namespace {

class DisplayStructTraitsTest : public testing::Test,
                                public mojom::DisplayStructTraitsTest {
 public:
  DisplayStructTraitsTest() {}

 protected:
  mojom::DisplayStructTraitsTestPtr GetTraitsTestProxy() {
    return traits_test_bindings_.CreateInterfacePtrAndBind(this);
  }

 private:
  // mojom::DisplayStructTraitsTest:
  void EchoDisplay(const Display& in,
                   const EchoDisplayCallback& callback) override {
    callback.Run(in);
  }

  base::MessageLoop loop_;  // A MessageLoop is needed for Mojo IPC to work.
  mojo::BindingSet<mojom::DisplayStructTraitsTest> traits_test_bindings_;

  DISALLOW_COPY_AND_ASSIGN(DisplayStructTraitsTest);
};

void CheckDisplaysEqual(const Display& input, const Display& output) {
  EXPECT_NE(&input, &output);  // Make sure they aren't the same object.
  EXPECT_EQ(input.id(), output.id());
  EXPECT_EQ(input.bounds(), output.bounds());
  EXPECT_EQ(input.work_area(), output.work_area());
  EXPECT_EQ(input.device_scale_factor(), output.device_scale_factor());
  EXPECT_EQ(input.rotation(), output.rotation());
  EXPECT_EQ(input.touch_support(), output.touch_support());
  EXPECT_EQ(input.maximum_cursor_size(), output.maximum_cursor_size());
}

}  // namespace

TEST_F(DisplayStructTraitsTest, DefaultDisplayValues) {
  Display input(5);

  mojom::DisplayStructTraitsTestPtr proxy = GetTraitsTestProxy();
  Display output;
  proxy->EchoDisplay(input, &output);

  CheckDisplaysEqual(input, output);
}

TEST_F(DisplayStructTraitsTest, SetAllDisplayValues) {
  const gfx::Rect bounds(100, 200, 500, 600);
  const gfx::Rect work_area(150, 250, 400, 500);
  const gfx::Size maximum_cursor_size(64, 64);

  Display input(246345234, bounds);
  input.set_work_area(work_area);
  input.set_device_scale_factor(2.0f);
  input.set_rotation(Display::ROTATE_270);
  input.set_touch_support(Display::TOUCH_SUPPORT_AVAILABLE);
  input.set_maximum_cursor_size(maximum_cursor_size);

  mojom::DisplayStructTraitsTestPtr proxy = GetTraitsTestProxy();
  Display output;
  proxy->EchoDisplay(input, &output);

  CheckDisplaysEqual(input, output);
}

}  // namespace display
