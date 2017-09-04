// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/contents/blimp_contents_view_impl_android.h"

#include "base/memory/ptr_util.h"
#include "blimp/client/core/contents/android/blimp_view.h"
#include "blimp/client/core/contents/android/ime_helper_dialog.h"
#include "blimp/client/core/contents/blimp_contents_impl.h"
#include "cc/layers/layer.h"
#include "ui/android/window_android.h"

namespace blimp {
namespace client {

// static
std::unique_ptr<BlimpContentsViewImpl> BlimpContentsViewImpl::Create(
    BlimpContentsImpl* blimp_contents,
    scoped_refptr<cc::Layer> contents_layer) {
  return base::MakeUnique<BlimpContentsViewImplAndroid>(blimp_contents,
                                                        contents_layer);
}

BlimpContentsViewImplAndroid::BlimpContentsViewImplAndroid(
    BlimpContentsImpl* blimp_contents,
    scoped_refptr<cc::Layer> contents_layer)
    : BlimpContentsViewImpl(blimp_contents, contents_layer),
      ime_dialog_(new ImeHelperDialog(blimp_contents->GetNativeWindow())) {
  blimp_view_ =
      base::MakeUnique<BlimpView>(blimp_contents->GetNativeWindow(), this);
  view_ = base::MakeUnique<ui::ViewAndroid>(
      blimp_view_->CreateViewAndroidDelegate());
  view_->SetLayer(contents_layer);
  blimp_contents->GetNativeWindow()->AddChild(view_.get());
}

BlimpContentsViewImplAndroid::~BlimpContentsViewImplAndroid() = default;

gfx::NativeView BlimpContentsViewImplAndroid::GetNativeView() {
  return view_.get();
}

BlimpView* BlimpContentsViewImplAndroid::GetBlimpView() {
  return blimp_view_.get();
}

ImeFeature::Delegate* BlimpContentsViewImplAndroid::GetImeDelegate() {
  return ime_dialog_.get();
}

}  // namespace client
}  // namespace blimp
