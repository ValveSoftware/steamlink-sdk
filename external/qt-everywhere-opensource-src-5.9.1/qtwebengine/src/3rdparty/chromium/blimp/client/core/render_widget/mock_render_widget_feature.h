// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_RENDER_WIDGET_MOCK_RENDER_WIDGET_FEATURE_H_
#define BLIMP_CLIENT_CORE_RENDER_WIDGET_MOCK_RENDER_WIDGET_FEATURE_H_

#include "blimp/client/core/render_widget/render_widget_feature.h"

#include "base/macros.h"
#include "cc/proto/compositor_message.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"

namespace blimp {
namespace client {

class MockRenderWidgetFeature : public RenderWidgetFeature {
 public:
  MockRenderWidgetFeature();
  ~MockRenderWidgetFeature() override;

  MOCK_METHOD3(SendCompositorMessage,
               void(const int, const int, const cc::proto::CompositorMessage&));
  MOCK_METHOD3(SendWebGestureEvent,
               void(const int, const int, const blink::WebGestureEvent&));

  // RenderWidgetFeature implementation.
  MOCK_METHOD2(SetDelegate, void(int, RenderWidgetFeatureDelegate*));
  MOCK_METHOD1(RemoveDelegate, void(const int));
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_RENDER_WIDGET_MOCK_RENDER_WIDGET_FEATURE_H_
