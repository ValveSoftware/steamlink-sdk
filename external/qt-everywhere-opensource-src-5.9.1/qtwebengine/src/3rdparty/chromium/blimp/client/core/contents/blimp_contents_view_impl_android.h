// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_CONTENTS_BLIMP_CONTENTS_VIEW_IMPL_ANDROID_H_
#define BLIMP_CLIENT_CORE_CONTENTS_BLIMP_CONTENTS_VIEW_IMPL_ANDROID_H_

#include "base/macros.h"
#include "blimp/client/core/contents/blimp_contents_view_impl.h"
#include "ui/android/view_android.h"

namespace blimp {
namespace client {
class BlimpContentsImpl;
class BlimpView;
class ImeHelperDialog;

// The Android specific implementation of a BlimpContentsViewImpl.
class BlimpContentsViewImplAndroid : public BlimpContentsViewImpl {
 public:
  BlimpContentsViewImplAndroid(BlimpContentsImpl* blimp_contents,
                               scoped_refptr<cc::Layer> contents_layer);
  ~BlimpContentsViewImplAndroid() override;

  // BlimpContentsViewImpl implementation.
  gfx::NativeView GetNativeView() override;
  ImeFeature::Delegate* GetImeDelegate() override;

  // Returns the JNI-bridge for the Android View for this BlimpContentsView.
  BlimpView* GetBlimpView();

 private:
  std::unique_ptr<ui::ViewAndroid> view_;

  // The JNI-bridge for the Android View for this BlimpContentsView.
  std::unique_ptr<BlimpView> blimp_view_;

  std::unique_ptr<ImeHelperDialog> ime_dialog_;

  DISALLOW_COPY_AND_ASSIGN(BlimpContentsViewImplAndroid);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_CONTENTS_BLIMP_CONTENTS_VIEW_IMPL_ANDROID_H_
