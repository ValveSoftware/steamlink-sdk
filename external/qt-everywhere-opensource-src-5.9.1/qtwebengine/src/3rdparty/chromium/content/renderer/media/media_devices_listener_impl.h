// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_DEVICES_LISTENER_IMPL_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_DEVICES_LISTENER_IMPL_H_

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "content/common/media/media_devices.h"
#include "content/common/media/media_devices.mojom.h"

namespace content {

// This class implements a Mojo object that receives notifications about changes
// in the set of media devices for a given frame.
class CONTENT_EXPORT MediaDevicesListenerImpl
    : public ::mojom::MediaDevicesListener {
 public:
  static void Create(int render_frame_id,
                     ::mojom::MediaDevicesListenerRequest request);
  explicit MediaDevicesListenerImpl(int render_frame_id);
  ~MediaDevicesListenerImpl() override;

  // ::mojom::MediaDevicesListener implementation.
  void OnDevicesChanged(MediaDeviceType type,
                        uint32_t subscription_id,
                        const MediaDeviceInfoArray& device_infos) override;

 private:
  // Used for DCHECKs so methods calls won't execute in the wrong thread.
  base::ThreadChecker thread_checker_;

  int render_frame_id_;

  DISALLOW_COPY_AND_ASSIGN(MediaDevicesListenerImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_DEVICES_LISTENER_IMPL_H_
