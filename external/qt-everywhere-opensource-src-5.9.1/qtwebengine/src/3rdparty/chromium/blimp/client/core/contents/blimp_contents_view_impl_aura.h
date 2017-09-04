// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_CONTENTS_BLIMP_CONTENTS_VIEW_IMPL_AURA_H_
#define BLIMP_CLIENT_CORE_CONTENTS_BLIMP_CONTENTS_VIEW_IMPL_AURA_H_

#include "base/macros.h"
#include "blimp/client/core/contents/blimp_contents_view_impl.h"

namespace blimp {
namespace client {

// The Aura (Linux) specific implementation of a BlimpContentsViewImpl.
class BlimpContentsViewImplAura : public BlimpContentsViewImpl {
 public:
  BlimpContentsViewImplAura(BlimpContentsImpl* blimp_contents,
                            scoped_refptr<cc::Layer> contents_layer);
  ~BlimpContentsViewImplAura() override;

  // BlimpContentsView implementation.
  gfx::NativeView GetNativeView() override;
  ImeFeature::Delegate* GetImeDelegate() override;

 private:
  std::unique_ptr<ImeFeature::Delegate> ime_delegate_;

  DISALLOW_COPY_AND_ASSIGN(BlimpContentsViewImplAura);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_CONTENTS_BLIMP_CONTENTS_VIEW_IMPL_AURA_H_
