// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/aura_init.h"

#include <utility>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "services/catalog/public/cpp/resource_loader.h"
#include "services/shell/public/cpp/connector.h"
#include "ui/aura/env.h"
#include "ui/base/ime/input_method_initializer.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/views/views_delegate.h"

#if defined(OS_LINUX) && !defined(OS_ANDROID)
#include "components/font_service/public/cpp/font_loader.h"
#endif

namespace views {

namespace {

std::set<std::string> GetResourcePaths(const std::string& resource_file) {
  std::set<std::string> paths;
  paths.insert(resource_file);
  return paths;
}

class MusViewsDelegate : public ViewsDelegate {
 public:
  MusViewsDelegate() {}
  ~MusViewsDelegate() override {}

 private:
#if defined(OS_WIN)
  HICON GetSmallWindowIcon() const override { return nullptr; }
#endif
  void OnBeforeWidgetInit(
      Widget::InitParams* params,
      internal::NativeWidgetDelegate* delegate) override {}

  DISALLOW_COPY_AND_ASSIGN(MusViewsDelegate);
};

}  // namespace

AuraInit::AuraInit(shell::Connector* connector,
                   const std::string& resource_file)
    : resource_file_(resource_file),
      env_(aura::Env::CreateInstance()),
      views_delegate_(new MusViewsDelegate) {
  ui::MaterialDesignController::Initialize();
  InitializeResources(connector);

  ui::InitializeInputMethodForTesting();
}

AuraInit::~AuraInit() {
#if defined(OS_LINUX) && !defined(OS_ANDROID)
  if (font_loader_.get()) {
    SkFontConfigInterface::SetGlobal(nullptr);
    // FontLoader is ref counted. We need to explicitly shutdown the background
    // thread, otherwise the background thread may be shutdown after the app is
    // torn down, when we're in a bad state.
    font_loader_->Shutdown();
  }
#endif
}

void AuraInit::InitializeResources(shell::Connector* connector) {
  if (ui::ResourceBundle::HasSharedInstance())
    return;
  catalog::ResourceLoader loader;
  filesystem::mojom::DirectoryPtr directory;
  connector->ConnectToInterface("mojo:catalog", &directory);
  CHECK(loader.OpenFiles(std::move(directory),
        GetResourcePaths(resource_file_)));
  ui::RegisterPathProvider();
  base::File pak_file = loader.TakeFile(resource_file_);
  base::File pak_file_2 = pak_file.Duplicate();
  ui::ResourceBundle::InitSharedInstanceWithPakFileRegion(
      std::move(pak_file), base::MemoryMappedFile::Region::kWholeFile);
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromFile(
      std::move(pak_file_2), ui::SCALE_FACTOR_100P);

// Initialize the skia font code to go ask fontconfig underneath.
#if defined(OS_LINUX) && !defined(OS_ANDROID)
  font_loader_ = sk_make_sp<font_service::FontLoader>(connector);
  SkFontConfigInterface::SetGlobal(font_loader_.get());
#endif

  // There is a bunch of static state in gfx::Font, by running this now,
  // before any other apps load, we ensure all the state is set up.
  gfx::Font();
}

}  // namespace views
