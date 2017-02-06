// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_APP_LINUX_BLIMP_CLIENT_SESSION_LINUX_H_
#define BLIMP_CLIENT_APP_LINUX_BLIMP_CLIENT_SESSION_LINUX_H_

#include "base/macros.h"
#include "blimp/client/app/linux/blimp_display_manager.h"
#include "blimp/client/feature/ime_feature.h"
#include "blimp/client/feature/navigation_feature.h"
#include "blimp/client/session/blimp_client_session.h"

namespace ui {
class PlatformEventSource;
}

namespace blimp {
namespace client {

class BlimpClientSessionLinux : public BlimpClientSession,
                                public BlimpDisplayManagerDelegate {
 public:
  BlimpClientSessionLinux();
  ~BlimpClientSessionLinux() override;

  // BlimpDisplayManagerDelegate implementation.
  void OnClosed() override;

 private:
  std::unique_ptr<ui::PlatformEventSource> event_source_;
  std::unique_ptr<BlimpDisplayManager> blimp_display_manager_;
  std::unique_ptr<NavigationFeature::NavigationFeatureDelegate>
      navigation_feature_delegate_;
  std::unique_ptr<ImeFeature::Delegate> ime_feature_delegate_;

  DISALLOW_COPY_AND_ASSIGN(BlimpClientSessionLinux);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_APP_LINUX_BLIMP_CLIENT_SESSION_LINUX_H_
