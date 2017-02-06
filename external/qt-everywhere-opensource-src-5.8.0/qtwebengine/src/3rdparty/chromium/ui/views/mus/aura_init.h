// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MUS_AURA_INIT_H_
#define UI_VIEWS_MUS_AURA_INIT_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/views/mus/mus_export.h"

namespace aura {
class Env;
}

namespace font_service {
class FontLoader;
}

namespace shell {
class Connector;
}

namespace views {
class ViewsDelegate;

// Sets up necessary state for aura when run with the viewmanager.
// |resource_file| is the path to the apk file containing the resources.
class VIEWS_MUS_EXPORT AuraInit {
 public:
  AuraInit(shell::Connector* connector, const std::string& resource_file);
  ~AuraInit();

 private:
  void InitializeResources(shell::Connector* connector);

#if defined(OS_LINUX) && !defined(OS_ANDROID)
  sk_sp<font_service::FontLoader> font_loader_;
#endif

  const std::string resource_file_;

  std::unique_ptr<aura::Env> env_;
  std::unique_ptr<ViewsDelegate> views_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AuraInit);
};

}  // namespace views

#endif  // UI_VIEWS_MUS_AURA_INIT_H_
