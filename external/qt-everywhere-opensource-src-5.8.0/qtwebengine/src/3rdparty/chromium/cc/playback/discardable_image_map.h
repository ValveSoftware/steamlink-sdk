// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PLAYBACK_DISCARDABLE_IMAGE_MAP_H_
#define CC_PLAYBACK_DISCARDABLE_IMAGE_MAP_H_

#include <utility>
#include <vector>

#include "cc/base/cc_export.h"
#include "cc/base/rtree.h"
#include "cc/playback/draw_image.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

// Helper function to apply the matrix to the rect and return the result.
SkRect MapRect(const SkMatrix& matrix, const SkRect& src);

// This class is used for generating discardable images data (see DrawImage
// for the type of data it stores). It allows the client to query a particular
// rect and get back a list of DrawImages in that rect.
class CC_EXPORT DiscardableImageMap {
 public:
  class CC_EXPORT ScopedMetadataGenerator {
   public:
    ScopedMetadataGenerator(DiscardableImageMap* image_map,
                            const gfx::Size& bounds);
    ~ScopedMetadataGenerator();

    SkCanvas* canvas() { return metadata_canvas_.get(); }

   private:
    DiscardableImageMap* image_map_;
    sk_sp<SkCanvas> metadata_canvas_;
  };

  DiscardableImageMap();
  ~DiscardableImageMap();

  bool empty() const { return all_images_.empty(); }
  void GetDiscardableImagesInRect(const gfx::Rect& rect,
                                  float raster_scale,
                                  std::vector<DrawImage>* images) const;

 private:
  friend class ScopedMetadataGenerator;
  friend class DiscardableImageMapTest;

  sk_sp<SkCanvas> BeginGeneratingMetadata(const gfx::Size& bounds);
  void EndGeneratingMetadata();

  std::vector<std::pair<DrawImage, gfx::Rect>> all_images_;
  RTree images_rtree_;
};

}  // namespace cc

#endif  // CC_PLAYBACK_DISCARDABLE_IMAGE_MAP_H_
