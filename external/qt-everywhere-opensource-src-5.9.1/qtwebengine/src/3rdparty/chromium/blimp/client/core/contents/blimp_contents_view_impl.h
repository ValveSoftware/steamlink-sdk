// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_CONTENTS_BLIMP_CONTENTS_VIEW_IMPL_H_
#define BLIMP_CLIENT_CORE_CONTENTS_BLIMP_CONTENTS_VIEW_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "blimp/client/core/contents/ime_feature.h"
#include "blimp/client/public/contents/blimp_contents_view.h"
#include "ui/gfx/native_widget_types.h"

namespace cc {
class CopyOutputResult;
}

namespace blimp {
namespace client {
class BlimpContentsImpl;

// The internal generic implementation of a BlimpContentsView that implements
// the common functionality shared across platforms.  Meant to be overridden and
// implemented for each supported platform.
class BlimpContentsViewImpl : public BlimpContentsView {
 public:
  // A factory method that needs to be implemented for each supported platform.
  static std::unique_ptr<BlimpContentsViewImpl> Create(
      BlimpContentsImpl* blimp_contents,
      scoped_refptr<cc::Layer> contents_layer);

  ~BlimpContentsViewImpl() override;

  // Exposes the plaform specific ImeFeature::Delegate.  This should handle text
  // input requests and present them to the user.
  virtual ImeFeature::Delegate* GetImeDelegate() = 0;

  // BlimpContentsView implementation.
  scoped_refptr<cc::Layer> GetLayer() override;
  void SetSizeAndScale(const gfx::Size& size,
                       float device_scale_factor) override;
  bool OnTouchEvent(const ui::MotionEvent& motion_event) override;
  void CopyFromCompositingSurface(
      const ReadbackRequestCallback& callback) override;

 protected:
  BlimpContentsViewImpl(BlimpContentsImpl* blimp_contents,
                        scoped_refptr<cc::Layer> contents_layer);

 private:
  void StartReadbackRequest(const ReadbackRequestCallback& callback);
  void OnReadbackComplete(const ReadbackRequestCallback& callback,
                          std::unique_ptr<cc::CopyOutputResult> result);

  BlimpContentsImpl* blimp_contents_;
  scoped_refptr<cc::Layer> contents_layer_;

  base::WeakPtrFactory<BlimpContentsViewImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlimpContentsViewImpl);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_CONTENTS_BLIMP_CONTENTS_VIEW_IMPL_H_
