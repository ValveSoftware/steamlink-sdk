// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_BUFFER_DATA_H_
#define UI_OZONE_PLATFORM_DRI_BUFFER_DATA_H_

#include "base/memory/scoped_ptr.h"

struct gbm_bo;

namespace ui {

class DriWrapper;

// This class is used to tag a gbm buffer with custom information needed
// for presentation with the surface factory.
class BufferData {
 public:
  // When we create the BufferData we need to register the buffer. Once
  // successfully registered, the |framebuffer_| field will hold the ID of the
  // buffer. The controller will use this ID when scanning out the buffer. On
  // creation we will also associate the BufferData with the buffer.
  static BufferData* CreateData(DriWrapper* dri, gbm_bo* buffer);

  // Callback used by GBM to destroy the BufferData associated with a buffer.
  static void Destroy(gbm_bo* buffer, void* data);

  // Returns the BufferData associated with |buffer|. NULL if no data is
  // associated.
  static BufferData* GetData(gbm_bo* buffer);

  uint32_t framebuffer() const { return framebuffer_; }
  uint32_t handle() const { return handle_; }

 private:
  BufferData(DriWrapper* dri, gbm_bo* buffer);
  ~BufferData();

  DriWrapper* dri_;

  uint32_t handle_;

  // ID provided by the controller when the buffer is registered. This ID is
  // used when scanning out the buffer.
  uint32_t framebuffer_;

  DISALLOW_COPY_AND_ASSIGN(BufferData);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_BUFFER_DATA_H_
