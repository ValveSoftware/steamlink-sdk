// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/intent_helper/arc_intent_helper_bridge.h"

#include <utility>

#include "ash/common/shell_delegate.h"
#include "ash/common/wallpaper/wallpaper_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/public/interfaces/new_window.mojom.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/intent_helper/activity_icon_loader.h"
#include "components/arc/intent_helper/link_handler_model_impl.h"
#include "components/arc/intent_helper/local_activity_resolver.h"
#include "ui/base/layout.h"
#include "url/gurl.h"

namespace arc {

// static
const char ArcIntentHelperBridge::kArcIntentHelperPackageName[] =
    "org.chromium.arc.intent_helper";

ArcIntentHelperBridge::ArcIntentHelperBridge(
    ArcBridgeService* bridge_service,
    const scoped_refptr<ActivityIconLoader>& icon_loader,
    const scoped_refptr<LocalActivityResolver>& activity_resolver)
    : ArcService(bridge_service),
      binding_(this),
      icon_loader_(icon_loader),
      activity_resolver_(activity_resolver) {
  DCHECK(thread_checker_.CalledOnValidThread());
  arc_bridge_service()->intent_helper()->AddObserver(this);
}

ArcIntentHelperBridge::~ArcIntentHelperBridge() {
  DCHECK(thread_checker_.CalledOnValidThread());
  arc_bridge_service()->intent_helper()->RemoveObserver(this);
}

void ArcIntentHelperBridge::OnInstanceReady() {
  DCHECK(thread_checker_.CalledOnValidThread());
  ash::Shell::GetInstance()->set_link_handler_model_factory(this);
  auto* instance =
      arc_bridge_service()->intent_helper()->GetInstanceForMethod("Init");
  DCHECK(instance);
  instance->Init(binding_.CreateInterfacePtrAndBind());
}

void ArcIntentHelperBridge::OnInstanceClosed() {
  DCHECK(thread_checker_.CalledOnValidThread());
  ash::Shell::GetInstance()->set_link_handler_model_factory(nullptr);
}

void ArcIntentHelperBridge::OnIconInvalidated(const std::string& package_name) {
  DCHECK(thread_checker_.CalledOnValidThread());
  icon_loader_->InvalidateIcons(package_name);
}

void ArcIntentHelperBridge::OnOpenDownloads() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // TODO(607411): If the FileManager is not yet open this will open to
  // downloads by default, which is what we want.  However if it is open it will
  // simply be brought to the forgeground without forcibly being navigated to
  // downloads, which is probably not ideal.
  ash::WmShell::Get()->new_window_client()->OpenFileManager();
}

void ArcIntentHelperBridge::OnOpenUrl(const std::string& url) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ash::WmShell::Get()->delegate()->OpenUrlFromArc(GURL(url));
}

void ArcIntentHelperBridge::OpenWallpaperPicker() {
  DCHECK(thread_checker_.CalledOnValidThread());
  ash::WmShell::Get()->wallpaper_controller()->OpenSetWallpaperPage();
}

void ArcIntentHelperBridge::SetWallpaperDeprecated(
    const std::vector<uint8_t>& jpeg_data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  LOG(ERROR) << "IntentHelper.SetWallpaper is deprecated";
}

std::unique_ptr<ash::LinkHandlerModel> ArcIntentHelperBridge::CreateModel(
    const GURL& url) {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::unique_ptr<LinkHandlerModelImpl> impl(
      new LinkHandlerModelImpl(icon_loader_));
  if (!impl->Init(url))
    return nullptr;
  return std::move(impl);
}

// static
bool ArcIntentHelperBridge::IsIntentHelperPackage(
    const std::string& package_name) {
  return package_name == kArcIntentHelperPackageName;
}

// static
std::vector<mojom::IntentHandlerInfoPtr>
ArcIntentHelperBridge::FilterOutIntentHelper(
    std::vector<mojom::IntentHandlerInfoPtr> handlers) {
  std::vector<mojom::IntentHandlerInfoPtr> handlers_filtered;
  for (auto& handler : handlers) {
    if (IsIntentHelperPackage(handler->package_name))
      continue;
    handlers_filtered.push_back(std::move(handler));
  }
  return handlers_filtered;
}

// static
mojom::IntentHelperInstance*
ArcIntentHelperBridge::GetIntentHelperInstanceWithErrorCode(
    const std::string& method_name_for_logging,
    uint32_t min_instance_version,
    GetResult* out_error_code) {
  ArcBridgeService* bridge_service = ArcBridgeService::Get();
  if (!bridge_service) {
    if (!ArcBridgeService::GetEnabled(base::CommandLine::ForCurrentProcess())) {
      VLOG(2) << "ARC bridge is not supported.";
      if (out_error_code)
        *out_error_code = GetResult::FAILED_ARC_NOT_SUPPORTED;
    } else {
      VLOG(2) << "ARC bridge is not ready.";
      if (out_error_code)
        *out_error_code = GetResult::FAILED_ARC_NOT_READY;
    }
    return nullptr;
  }

  if (!bridge_service->intent_helper()->has_instance()) {
    VLOG(2) << "ARC intent helper instance is not ready.";
    if (out_error_code)
      *out_error_code = GetResult::FAILED_ARC_NOT_READY;
    return nullptr;
  }

  auto* instance = bridge_service->intent_helper()->GetInstanceForMethod(
      method_name_for_logging, min_instance_version);
  if (!instance) {
    if (out_error_code)
      *out_error_code = GetResult::FAILED_ARC_NOT_SUPPORTED;
    return nullptr;
  }
  return instance;
}

// static
mojom::IntentHelperInstance* ArcIntentHelperBridge::GetIntentHelperInstance(
    const std::string& method_name_for_logging,
    uint32_t min_instance_version) {
  return GetIntentHelperInstanceWithErrorCode(method_name_for_logging,
                                              min_instance_version, nullptr);
}

void ArcIntentHelperBridge::OnIntentFiltersUpdated(
    std::vector<mojom::IntentFilterPtr> filters) {
  DCHECK(thread_checker_.CalledOnValidThread());
  activity_resolver_->UpdateIntentFilters(std::move(filters));
}

}  // namespace arc
