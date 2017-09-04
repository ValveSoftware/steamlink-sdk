// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RasterInvalidationTracking_h
#define RasterInvalidationTracking_h

#include "platform/geometry/IntRect.h"
#include "platform/geometry/Region.h"
#include "platform/graphics/PaintInvalidationReason.h"
#include "platform/json/JSONValues.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "wtf/Allocator.h"
#include "wtf/text/WTFString.h"

namespace blink {

class DisplayItemClient;

struct RasterInvalidationInfo {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
  // This is for comparison only. Don't dereference because the client may have
  // died.
  const DisplayItemClient* client;
  String clientDebugName;
  IntRect rect;
  PaintInvalidationReason reason;
  RasterInvalidationInfo() : reason(PaintInvalidationFull) {}
};

inline bool operator==(const RasterInvalidationInfo& a,
                       const RasterInvalidationInfo& b) {
  return a.rect == b.rect;
}

struct UnderPaintInvalidation {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
  int x;
  int y;
  SkColor oldPixel;
  SkColor newPixel;
};

struct PLATFORM_EXPORT RasterInvalidationTracking {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
  Vector<RasterInvalidationInfo> trackedRasterInvalidations;
  sk_sp<SkPicture> lastPaintedPicture;
  IntRect lastInterestRect;
  Region rasterInvalidationRegionSinceLastPaint;
  Vector<UnderPaintInvalidation> underPaintInvalidations;

  void asJSON(JSONObject*);
};

template <class TargetClass>
class PLATFORM_EXPORT RasterInvalidationTrackingMap {
 public:
  void asJSON(TargetClass* key, JSONObject* json) {
    auto it = m_invalidationTrackingMap.find(key);
    if (it != m_invalidationTrackingMap.end())
      it->value.asJSON(json);
  }

  void remove(TargetClass* key) {
    auto it = m_invalidationTrackingMap.find(key);
    if (it != m_invalidationTrackingMap.end())
      m_invalidationTrackingMap.remove(it);
  }

  RasterInvalidationTracking& add(TargetClass* key) {
    return m_invalidationTrackingMap.add(key, RasterInvalidationTracking())
        .storedValue->value;
  }

  RasterInvalidationTracking* find(TargetClass* key) {
    auto it = m_invalidationTrackingMap.find(key);
    if (it == m_invalidationTrackingMap.end())
      return nullptr;
    return &it->value;
  }

 private:
  typedef HashMap<TargetClass*, RasterInvalidationTracking>
      InvalidationTrackingMap;
  InvalidationTrackingMap m_invalidationTrackingMap;
};

}  // namespace blink

#endif  // RasterInvalidationTracking_h
