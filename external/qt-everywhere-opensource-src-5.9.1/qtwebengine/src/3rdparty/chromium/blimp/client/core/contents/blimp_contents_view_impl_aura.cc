// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/contents/blimp_contents_view_impl_aura.h"

#include "base/memory/ptr_util.h"
#include "cc/layers/layer.h"

namespace blimp {
namespace client {

namespace {
class AuraImeDelegate : public ImeFeature::Delegate {
 public:
  ~AuraImeDelegate() override = default;
  void OnShowImeRequested(const ImeFeature::WebInputRequest& request) override {
    request.show_ime_callback.Run(ImeFeature::WebInputResponse());
  }

  void OnHideImeRequested() override {}
};
}  // namespace

// static
std::unique_ptr<BlimpContentsViewImpl> BlimpContentsViewImpl::Create(
    BlimpContentsImpl* blimp_contents,
    scoped_refptr<cc::Layer> contents_layer) {
  return base::MakeUnique<BlimpContentsViewImplAura>(blimp_contents,
                                                     contents_layer);
}

BlimpContentsViewImplAura::BlimpContentsViewImplAura(
    BlimpContentsImpl* blimp_contents,
    scoped_refptr<cc::Layer> contents_layer)
    : BlimpContentsViewImpl(blimp_contents, contents_layer),
      ime_delegate_(base::MakeUnique<AuraImeDelegate>()) {}

BlimpContentsViewImplAura::~BlimpContentsViewImplAura() = default;

gfx::NativeView BlimpContentsViewImplAura::GetNativeView() {
  return nullptr;
}

ImeFeature::Delegate* BlimpContentsViewImplAura::GetImeDelegate() {
  return ime_delegate_.get();
}

}  // namespace client
}  // namespace blimp
