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
#include "ui/aura/env.h"
#include "ui/views/mus/mus_export.h"

namespace aura {
class Env;
}

namespace base {
class SingleThreadTaskRunner;
}

namespace font_service {
class FontLoader;
}

namespace service_manager {
class Connector;
class Identity;
}

namespace views {
class MusClient;
class ViewsDelegate;

// Sets up necessary state for aura when run with the viewmanager.
// |resource_file| is the path to the apk file containing the resources.
class VIEWS_MUS_EXPORT AuraInit {
 public:
  enum class Mode {
    // Indicates AuraInit should target using aura with mus.
    AURA_MUS,

    // Indicates AuraInit should target using ui::Window.
    UI,
  };

  // |resource_file| is the file to load strings and 1x icons from.
  // |resource_file_200| can be an empty string, otherwise it is the file to
  // load 2x icons from.
  AuraInit(service_manager::Connector* connector,
           const service_manager::Identity& identity,
           const std::string& resource_file,
           const std::string& resource_file_200 = std::string(),
           scoped_refptr<base::SingleThreadTaskRunner> io_task_runner = nullptr,
           Mode mode = Mode::UI);
  ~AuraInit();

  // Only valid if Mode::AURA_MUS was passed to constructor.
  MusClient* mus_client() { return mus_client_.get(); }

 private:
  void InitializeResources(service_manager::Connector* connector);

#if defined(OS_LINUX)
  sk_sp<font_service::FontLoader> font_loader_;
#endif

  const std::string resource_file_;
  const std::string resource_file_200_;

  std::unique_ptr<MusClient> mus_client_;
  std::unique_ptr<aura::Env> env_;
  std::unique_ptr<ViewsDelegate> views_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AuraInit);
};

}  // namespace views

#endif  // UI_VIEWS_MUS_AURA_INIT_H_
