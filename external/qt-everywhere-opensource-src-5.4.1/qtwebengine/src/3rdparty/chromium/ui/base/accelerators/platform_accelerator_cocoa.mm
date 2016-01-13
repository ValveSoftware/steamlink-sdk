// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/accelerators/platform_accelerator_cocoa.h"

#include "base/memory/scoped_ptr.h"

namespace ui {

PlatformAcceleratorCocoa::PlatformAcceleratorCocoa() : modifier_mask_(0) {
}

PlatformAcceleratorCocoa::PlatformAcceleratorCocoa(NSString* key_code,
                                                   NSUInteger modifier_mask)
    : characters_([key_code copy]),
      modifier_mask_(modifier_mask) {
}

PlatformAcceleratorCocoa::~PlatformAcceleratorCocoa() {
}

scoped_ptr<PlatformAccelerator> PlatformAcceleratorCocoa::CreateCopy() const {
  scoped_ptr<PlatformAcceleratorCocoa> copy(new PlatformAcceleratorCocoa);
  copy->characters_.reset([characters_ copy]);
  copy->modifier_mask_ = modifier_mask_;
  return scoped_ptr<PlatformAccelerator>(copy.release());
}

bool PlatformAcceleratorCocoa::Equals(const PlatformAccelerator& rhs) const {
  const PlatformAcceleratorCocoa& rhs_cocoa =
      static_cast<const PlatformAcceleratorCocoa&>(rhs);
  return [characters_ isEqualToString:rhs_cocoa.characters_] &&
         modifier_mask_ == rhs_cocoa.modifier_mask_;
}

}  // namespace ui
