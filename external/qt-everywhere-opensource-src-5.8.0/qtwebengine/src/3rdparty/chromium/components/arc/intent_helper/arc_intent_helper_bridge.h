// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_INTENT_HELPER_ARC_INTENT_HELPER_BRIDGE_H_
#define COMPONENTS_ARC_INTENT_HELPER_ARC_INTENT_HELPER_BRIDGE_H_

#include <memory>
#include <string>

#include "ash/link_handler_model_factory.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service.h"
#include "components/arc/common/intent_helper.mojom.h"
#include "components/arc/instance_holder.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace ash {

class LinkHandlerModel;

}  // namespace ash

namespace arc {

class ActivityIconLoader;
class LocalActivityResolver;
class SetWallpaperDelegate;

// Receives intents from ARC.
class ArcIntentHelperBridge
    : public ArcService,
      public InstanceHolder<mojom::IntentHelperInstance>::Observer,
      public mojom::IntentHelperHost,
      public ash::LinkHandlerModelFactory {
 public:
  ArcIntentHelperBridge(
      ArcBridgeService* bridge_service,
      const scoped_refptr<ActivityIconLoader>& icon_loader,
      std::unique_ptr<SetWallpaperDelegate> set_wallpaper_delegate,
      const scoped_refptr<LocalActivityResolver>& activity_resolver);
  ~ArcIntentHelperBridge() override;

  // InstanceHolder<mojom::IntentHelperInstance>::Observer
  void OnInstanceReady() override;
  void OnInstanceClosed() override;

  // arc::mojom::IntentHelperHost
  void OnIconInvalidated(const mojo::String& package_name) override;
  void OnIntentFiltersUpdated(
      mojo::Array<mojom::IntentFilterPtr> intent_filters) override;
  void OnOpenDownloads() override;
  void OnOpenUrl(const mojo::String& url) override;
  void OpenWallpaperPicker() override;
  void SetWallpaper(mojo::Array<uint8_t> jpeg_data) override;

  // ash::LinkHandlerModelFactory
  std::unique_ptr<ash::LinkHandlerModel> CreateModel(const GURL& url) override;

  // Returns false if |package_name| is for the intent_helper apk.
  static bool IsIntentHelperPackage(const std::string& package_name);

  // Filters out handlers that belong to the intent_helper apk and returns
  // a new array.
  static mojo::Array<mojom::UrlHandlerInfoPtr> FilterOutIntentHelper(
      mojo::Array<mojom::UrlHandlerInfoPtr> handlers);

 private:
  mojo::Binding<mojom::IntentHelperHost> binding_;
  scoped_refptr<ActivityIconLoader> icon_loader_;
  std::unique_ptr<SetWallpaperDelegate> set_wallpaper_delegate_;
  scoped_refptr<LocalActivityResolver> activity_resolver_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(ArcIntentHelperBridge);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_INTENT_HELPER_ARC_INTENT_HELPER_BRIDGE_H_
