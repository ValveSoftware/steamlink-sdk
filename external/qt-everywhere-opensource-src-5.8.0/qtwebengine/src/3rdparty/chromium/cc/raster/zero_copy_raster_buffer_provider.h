// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_ZERO_COPY_RASTER_BUFFER_PROVIDER_H_
#define CC_RASTER_ZERO_COPY_RASTER_BUFFER_PROVIDER_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "cc/raster/raster_buffer_provider.h"

namespace base {
namespace trace_event {
class ConvertableToTraceFormat;
}
}

namespace cc {
class ResourceProvider;

class CC_EXPORT ZeroCopyRasterBufferProvider : public RasterBufferProvider {
 public:
  ~ZeroCopyRasterBufferProvider() override;

  static std::unique_ptr<RasterBufferProvider> Create(
      ResourceProvider* resource_provider,
      ResourceFormat preferred_tile_format);

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
  ZeroCopyRasterBufferProvider(ResourceProvider* resource_provider,
                               ResourceFormat preferred_tile_format);

 private:
  std::unique_ptr<base::trace_event::ConvertableToTraceFormat> StateAsValue()
      const;

  ResourceProvider* resource_provider_;
  ResourceFormat preferred_tile_format_;

  DISALLOW_COPY_AND_ASSIGN(ZeroCopyRasterBufferProvider);
};

}  // namespace cc

#endif  // CC_RASTER_ZERO_COPY_RASTER_BUFFER_PROVIDER_H_
