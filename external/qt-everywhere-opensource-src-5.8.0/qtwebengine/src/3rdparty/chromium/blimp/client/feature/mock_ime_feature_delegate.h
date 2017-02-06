// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_FEATURE_MOCK_IME_FEATURE_DELEGATE_H_
#define BLIMP_CLIENT_FEATURE_MOCK_IME_FEATURE_DELEGATE_H_

#include <string>

#include "blimp/client/feature/ime_feature.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace blimp {
namespace client {

class MockImeFeatureDelegate : public client::ImeFeature::Delegate {
 public:
  MockImeFeatureDelegate();
  ~MockImeFeatureDelegate() override;

  MOCK_METHOD2(OnShowImeRequested,
               void(ui::TextInputType input_type, const std::string& text));
  MOCK_METHOD0(OnHideImeRequested, void());
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_FEATURE_MOCK_IME_FEATURE_DELEGATE_H_
