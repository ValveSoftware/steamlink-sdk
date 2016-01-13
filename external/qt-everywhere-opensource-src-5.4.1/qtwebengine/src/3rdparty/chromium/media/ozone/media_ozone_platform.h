// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_OZONE_MEDIA_OZONE_PLATFORM_H_
#define MEDIA_OZONE_MEDIA_OZONE_PLATFORM_H_

#include "base/callback.h"
#include "media/base/media_export.h"

namespace media {

class VideoDecodeAccelerator;

// Class for Ozone platform media implementations. Note that the base class for
// Ozone platform is at ui/ozone.
//
// Ozone platforms must override this class and implement the virtual
// GetFooFactoryOzone() methods to provide implementations of the
// various ozone interfaces.
class MEDIA_EXPORT MediaOzonePlatform {
 public:
  MediaOzonePlatform();
  virtual ~MediaOzonePlatform();

  // Besides get the global instance, initializes the subsystems/resources
  // necessary for media also.
  static MediaOzonePlatform* GetInstance();

  // Factory getters to override in subclasses. The returned objects will be
  // injected into the appropriate layer at startup. Subclasses should not
  // inject these objects themselves. Ownership is retained by
  // MediaOzonePlatform.
  virtual VideoDecodeAccelerator* CreateVideoDecodeAccelerator(
      const base::Callback<bool(void)>& make_context_current);

 private:
  static void CreateInstance();

  static MediaOzonePlatform* instance_;

  DISALLOW_COPY_AND_ASSIGN(MediaOzonePlatform);
};

}  // namespace media

#endif  // MEDIA_OZONE_MEDIA_OZONE_PLATFORM_H_
