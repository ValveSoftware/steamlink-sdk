// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_BITMAP_RASTER_BUFFER_PROVIDER_H_
#define CC_RASTER_BITMAP_RASTER_BUFFER_PROVIDER_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/values.h"
#include "cc/raster/raster_buffer_provider.h"

namespace base {
namespace trace_event {
class ConvertableToTraceFormat;
}
}

namespace cc {
class ResourceProvider;

class CC_EXPORT BitmapRasterBufferProvider : public RasterBufferProvider {
 public:
  ~BitmapRasterBufferProvider() override;

  static std::unique_ptr<RasterBufferProvider> Create(
      ResourceProvider* resource_provider);

  // Overridden from RasterBufferProvider:
  std::unique_ptr<RasterBuffer> AcquireBufferForRaster(
      const Resource* resource,
      uint64_t resource_content_id,
      uint64_t previous_content_id) override;
  void ReleaseBufferForRaster(std::unique_ptr<RasterBuffer> buffer) override;
  void OrderingBarrier() override;
  ResourceFormat GetResourceFormat(bool must_support_alpha) const override;
  bool GetResourceRequiresSwizzle(bool must_support_alpha) const override;
  void Shutdown() override;

 protected:
  explicit BitmapRasterBufferProvider(ResourceProvider* resource_provider);

 private:
  std::unique_ptr<base::trace_event::ConvertableToTraceFormat> StateAsValue()
      const;

  ResourceProvider* resource_provider_;

  DISALLOW_COPY_AND_ASSIGN(BitmapRasterBufferProvider);
};

}  // namespace cc

#endif  // CC_RASTER_BITMAP_RASTER_BUFFER_PROVIDER_H_
