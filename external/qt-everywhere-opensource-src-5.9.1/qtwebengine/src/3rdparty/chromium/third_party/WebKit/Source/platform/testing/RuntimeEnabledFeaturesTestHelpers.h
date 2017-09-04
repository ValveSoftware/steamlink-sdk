// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RuntimeEnabledFeaturesTestHelpers_h
#define RuntimeEnabledFeaturesTestHelpers_h

#include "platform/RuntimeEnabledFeatures.h"
#include "wtf/Assertions.h"

namespace blink {

template <bool (&getter)(), void (&setter)(bool)>
class ScopedRuntimeEnabledFeatureForTest {
 public:
  ScopedRuntimeEnabledFeatureForTest(bool enabled)
      : m_enabled(enabled), m_original(getter()) {
    setter(enabled);
  }

  ~ScopedRuntimeEnabledFeatureForTest() {
    CHECK_EQ(m_enabled, getter());
    setter(m_original);
  }

 private:
  bool m_enabled;
  bool m_original;
};

typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::compositeOpaqueFixedPositionEnabled,
    RuntimeEnabledFeatures::setCompositeOpaqueFixedPositionEnabled>
    ScopedCompositeFixedPositionForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::compositeOpaqueScrollersEnabled,
    RuntimeEnabledFeatures::setCompositeOpaqueScrollersEnabled>
    ScopedCompositeOpaqueScrollersForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::compositorWorkerEnabled,
    RuntimeEnabledFeatures::setCompositorWorkerEnabled>
    ScopedCompositorWorkerForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::rootLayerScrollingEnabled,
    RuntimeEnabledFeatures::setRootLayerScrollingEnabled>
    ScopedRootLayerScrollingForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::slimmingPaintV2Enabled,
    RuntimeEnabledFeatures::setSlimmingPaintV2Enabled>
    ScopedSlimmingPaintV2ForTest;
typedef ScopedRuntimeEnabledFeatureForTest<
    RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled,
    RuntimeEnabledFeatures::setSlimmingPaintInvalidationEnabled>
    ScopedSlimmingPaintInvalidationForTest;

}  // namespace blink

#endif  // RuntimeEnabledFeaturesTestHelpers_h
