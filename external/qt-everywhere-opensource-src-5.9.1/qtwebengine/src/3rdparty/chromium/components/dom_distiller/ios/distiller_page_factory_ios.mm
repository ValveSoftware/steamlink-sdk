// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/ios/distiller_page_factory_ios.h"

#include "base/memory/ptr_util.h"
#include "components/dom_distiller/ios/distiller_page_ios.h"
#include "ios/web/public/browser_state.h"

namespace dom_distiller {

DistillerPageFactoryIOS::DistillerPageFactoryIOS(
    web::BrowserState* browser_state)
    : browser_state_(browser_state) {
}

std::unique_ptr<DistillerPage> DistillerPageFactoryIOS::CreateDistillerPage(
    const gfx::Size& view_size) const {
  return base::WrapUnique<DistillerPage>(new DistillerPageIOS(browser_state_));
}

std::unique_ptr<DistillerPage>
DistillerPageFactoryIOS::CreateDistillerPageWithHandle(
    std::unique_ptr<SourcePageHandle> handle) const {
  return base::WrapUnique<DistillerPage>(new DistillerPageIOS(browser_state_));
}

}  // namespace dom_distiller
