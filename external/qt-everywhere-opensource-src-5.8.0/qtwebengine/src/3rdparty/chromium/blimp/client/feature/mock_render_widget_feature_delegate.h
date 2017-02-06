// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_FEATURE_MOCK_RENDER_WIDGET_FEATURE_DELEGATE_H_
#define BLIMP_CLIENT_FEATURE_MOCK_RENDER_WIDGET_FEATURE_DELEGATE_H_

#include <string>

#include "blimp/client/feature/render_widget_feature.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace blimp {
namespace client {

class MockRenderWidgetFeatureDelegate
    : public RenderWidgetFeature::RenderWidgetFeatureDelegate {
 public:
  MockRenderWidgetFeatureDelegate();
  ~MockRenderWidgetFeatureDelegate() override;

  MOCK_METHOD1(OnRenderWidgetCreated, void(int render_widget_id));
  MOCK_METHOD1(OnRenderWidgetInitialized, void(int render_widget_id));
  MOCK_METHOD1(OnRenderWidgetDeleted, void(int render_widget_id));
  MOCK_METHOD2(MockableOnCompositorMessageReceived,
               void(int render_widget_id,
                    cc::proto::CompositorMessage* message));

  void OnCompositorMessageReceived(
      int render_widget_id,
      std::unique_ptr<cc::proto::CompositorMessage> message) override;
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_FEATURE_MOCK_RENDER_WIDGET_FEATURE_DELEGATE_H_
