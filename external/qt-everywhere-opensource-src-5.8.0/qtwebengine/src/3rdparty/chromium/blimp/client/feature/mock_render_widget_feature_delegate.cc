// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/feature/mock_render_widget_feature_delegate.h"

namespace blimp {
namespace client {

MockRenderWidgetFeatureDelegate::MockRenderWidgetFeatureDelegate() {}

MockRenderWidgetFeatureDelegate::~MockRenderWidgetFeatureDelegate() {}

void MockRenderWidgetFeatureDelegate::OnCompositorMessageReceived(
    int render_widget_id,
    std::unique_ptr<cc::proto::CompositorMessage> message) {
  MockableOnCompositorMessageReceived(render_widget_id, message.get());
}

}  // namespace client
}  // namespace blimp
