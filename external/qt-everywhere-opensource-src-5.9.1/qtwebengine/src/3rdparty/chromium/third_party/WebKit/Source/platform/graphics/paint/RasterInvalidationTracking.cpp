// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/RasterInvalidationTracking.h"

#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/Color.h"

namespace blink {

static bool compareRasterInvalidationInfo(const RasterInvalidationInfo& a,
                                          const RasterInvalidationInfo& b) {
  // Sort by rect first, bigger rects before smaller ones.
  if (a.rect.width() != b.rect.width())
    return a.rect.width() > b.rect.width();
  if (a.rect.height() != b.rect.height())
    return a.rect.height() > b.rect.height();
  if (a.rect.x() != b.rect.x())
    return a.rect.x() > b.rect.x();
  if (a.rect.y() != b.rect.y())
    return a.rect.y() > b.rect.y();

  // Then compare clientDebugName, in alphabetic order.
  int nameCompareResult =
      codePointCompare(a.clientDebugName, b.clientDebugName);
  if (nameCompareResult != 0)
    return nameCompareResult < 0;

  return a.reason < b.reason;
}

template <typename T>
static std::unique_ptr<JSONArray> rectAsJSONArray(const T& rect) {
  std::unique_ptr<JSONArray> array = JSONArray::create();
  array->pushDouble(rect.x());
  array->pushDouble(rect.y());
  array->pushDouble(rect.width());
  array->pushDouble(rect.height());
  return array;
}

void RasterInvalidationTracking::asJSON(JSONObject* json) {
  if (!trackedRasterInvalidations.isEmpty()) {
    std::sort(trackedRasterInvalidations.begin(),
              trackedRasterInvalidations.end(), &compareRasterInvalidationInfo);
    std::unique_ptr<JSONArray> paintInvalidationsJSON = JSONArray::create();
    for (auto& info : trackedRasterInvalidations) {
      std::unique_ptr<JSONObject> infoJSON = JSONObject::create();
      infoJSON->setString("object", info.clientDebugName);
      if (!info.rect.isEmpty()) {
        if (info.rect == LayoutRect::infiniteIntRect())
          infoJSON->setString("rect", "infinite");
        else
          infoJSON->setArray("rect", rectAsJSONArray(info.rect));
      }
      infoJSON->setString("reason",
                          paintInvalidationReasonToString(info.reason));
      paintInvalidationsJSON->pushObject(std::move(infoJSON));
    }
    json->setArray("paintInvalidations", std::move(paintInvalidationsJSON));
  }

  if (!underPaintInvalidations.isEmpty()) {
    std::unique_ptr<JSONArray> underPaintInvalidationsJSON =
        JSONArray::create();
    for (auto& underPaintInvalidation : underPaintInvalidations) {
      std::unique_ptr<JSONObject> underPaintInvalidationJSON =
          JSONObject::create();
      underPaintInvalidationJSON->setDouble("x", underPaintInvalidation.x);
      underPaintInvalidationJSON->setDouble("y", underPaintInvalidation.y);
      underPaintInvalidationJSON->setString(
          "oldPixel",
          Color(underPaintInvalidation.oldPixel).nameForLayoutTreeAsText());
      underPaintInvalidationJSON->setString(
          "newPixel",
          Color(underPaintInvalidation.newPixel).nameForLayoutTreeAsText());
      underPaintInvalidationsJSON->pushObject(
          std::move(underPaintInvalidationJSON));
    }
    json->setArray("underPaintInvalidations",
                   std::move(underPaintInvalidationsJSON));
  }
}

}  // namespace blink
