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
#include "services/catalog/public/interfaces/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/aura/env.h"
#include "ui/base/ime/input_method_initializer.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/views/mus/mus_client.h"
#include "ui/views/views_delegate.h"

#if defined(OS_LINUX)
#include "components/font_service/public/cpp/font_loader.h"
#endif

namespace views {

namespace {

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

AuraInit::AuraInit(service_manager::Connector* connector,
                   const service_manager::Identity& identity,
                   const std::string& resource_file,
                   const std::string& resource_file_200,
                   scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
                   Mode mode)
    : resource_file_(resource_file),
      resource_file_200_(resource_file_200),
      env_(aura::Env::CreateInstance(mode == Mode::AURA_MUS
                                         ? aura::Env::Mode::MUS
                                         : aura::Env::Mode::LOCAL)),
      views_delegate_(new MusViewsDelegate) {
  if (mode == Mode::AURA_MUS) {
    mus_client_ =
        base::WrapUnique(new MusClient(connector, identity, io_task_runner));
  }
  ui::MaterialDesignController::Initialize();
  InitializeResources(connector);

// Initialize the skia font code to go ask fontconfig underneath.
#if defined(OS_LINUX)
  font_loader_ = sk_make_sp<font_service::FontLoader>(connector);
  SkFontConfigInterface::SetGlobal(font_loader_.get());
#endif

  // There is a bunch of static state in gfx::Font, by running this now,
  // before any other apps load, we ensure all the state is set up.
  gfx::Font();

  ui::InitializeInputMethodForTesting();
}

AuraInit::~AuraInit() {
#if defined(OS_LINUX)
  if (font_loader_.get()) {
    SkFontConfigInterface::SetGlobal(nullptr);
    // FontLoader is ref counted. We need to explicitly shutdown the background
    // thread, otherwise the background thread may be shutdown after the app is
    // torn down, when we're in a bad state.
    font_loader_->Shutdown();
  }
#endif
}

void AuraInit::InitializeResources(service_manager::Connector* connector) {
  // Resources may have already been initialized (e.g. when 'chrome --mash' is
  // used to launch the current app).
  if (ui::ResourceBundle::HasSharedInstance())
    return;

  std::set<std::string> resource_paths({resource_file_});
  if (!resource_file_200_.empty())
    resource_paths.insert(resource_file_200_);

  catalog::ResourceLoader loader;
  filesystem::mojom::DirectoryPtr directory;
  connector->ConnectToInterface(catalog::mojom::kServiceName, &directory);
  CHECK(loader.OpenFiles(std::move(directory), resource_paths));
  ui::RegisterPathProvider();
  base::File pak_file = loader.TakeFile(resource_file_);
  base::File pak_file_2 = pak_file.Duplicate();
  ui::ResourceBundle::InitSharedInstanceWithPakFileRegion(
      std::move(pak_file), base::MemoryMappedFile::Region::kWholeFile);
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromFile(
      std::move(pak_file_2), ui::SCALE_FACTOR_100P);
  if (!resource_file_200_.empty())
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromFile(
        loader.TakeFile(resource_file_200_), ui::SCALE_FACTOR_200P);
}

}  // namespace views
