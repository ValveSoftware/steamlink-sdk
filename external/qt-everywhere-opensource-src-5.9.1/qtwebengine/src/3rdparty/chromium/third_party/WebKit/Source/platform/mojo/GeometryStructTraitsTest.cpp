// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/mojo/geometry_traits_test_service.mojom-blink.h"

namespace blink {

namespace {

class GeometryStructTraitsTest
    : public testing::Test,
      public gfx::mojom::blink::GeometryTraitsTestService {
 public:
  GeometryStructTraitsTest() {}

 protected:
  gfx::mojom::blink::GeometryTraitsTestServicePtr GetTraitsTestProxy() {
    return m_TraitsTestBindings.CreateInterfacePtrAndBind(this);
  }

 private:
  // GeometryTraitsTestService:
  void EchoPoint(gfx::mojom::blink::PointPtr, const EchoPointCallback&) {
    // The type map is not specified.
    NOTREACHED();
  }

  void EchoPointF(gfx::mojom::blink::PointFPtr, const EchoPointFCallback&) {
    // The type map is not specified.
    NOTREACHED();
  }

  void EchoSize(const WebSize& s, const EchoSizeCallback& callback) {
    callback.Run(s);
  }

  void EchoSizeF(gfx::mojom::blink::SizeFPtr, const EchoSizeFCallback&) {
    // The type map is not specified.
    NOTREACHED();
  }

  void EchoRect(gfx::mojom::blink::RectPtr, const EchoRectCallback&) {
    // The type map is not specified.
    NOTREACHED();
  }

  void EchoRectF(gfx::mojom::blink::RectFPtr, const EchoRectFCallback&) {
    // The type map is not specified.
    NOTREACHED();
  }

  void EchoInsets(gfx::mojom::blink::InsetsPtr, const EchoInsetsCallback&) {
    // The type map is not specified.
    NOTREACHED();
  }

  void EchoInsetsF(gfx::mojom::blink::InsetsFPtr, const EchoInsetsFCallback&) {
    // The type map is not specified.
    NOTREACHED();
  }

  void EchoVector2d(gfx::mojom::blink::Vector2dPtr,
                    const EchoVector2dCallback&) {
    // The type map is not specified.
    NOTREACHED();
  }

  void EchoVector2dF(gfx::mojom::blink::Vector2dFPtr,
                     const EchoVector2dFCallback&) {
    // The type map is not specified.
    NOTREACHED();
  }

  mojo::BindingSet<gfx::mojom::blink::GeometryTraitsTestService>
      m_TraitsTestBindings;

  base::MessageLoop m_messageLoop;

  DISALLOW_COPY_AND_ASSIGN(GeometryStructTraitsTest);
};

}  // namespace

TEST_F(GeometryStructTraitsTest, Size) {
  const int32_t width = 1234;
  const int32_t height = 5678;
  WebSize input(width, height);
  gfx::mojom::blink::GeometryTraitsTestServicePtr proxy = GetTraitsTestProxy();
  WebSize output;
  proxy->EchoSize(input, &output);
  EXPECT_EQ(input, output);
}

}  // namespace blink
