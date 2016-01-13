// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_RESOURCE_RESOURCE_DATA_DLL_WIN_H_
#define UI_BASE_RESOURCE_RESOURCE_DATA_DLL_WIN_H_

#include <windows.h>

#include "base/compiler_specific.h"
#include "ui/base/resource/resource_handle.h"

namespace ui {

class ResourceDataDLL : public ResourceHandle {
 public:
  explicit ResourceDataDLL(HINSTANCE module);
  virtual ~ResourceDataDLL();

  // ResourceHandle implementation:
  virtual bool HasResource(uint16 resource_id) const OVERRIDE;
  virtual bool GetStringPiece(uint16 resource_id,
                              base::StringPiece* data) const OVERRIDE;
  virtual base::RefCountedStaticMemory* GetStaticMemory(
      uint16 resource_id) const OVERRIDE;
  virtual TextEncodingType GetTextEncodingType() const OVERRIDE;
  virtual ScaleFactor GetScaleFactor() const OVERRIDE;

 private:
  const HINSTANCE module_;

  DISALLOW_COPY_AND_ASSIGN(ResourceDataDLL);
};

}  // namespace ui

#endif  // UI_BASE_RESOURCE_RESOURCE_DATA_DLL_WIN_H_
