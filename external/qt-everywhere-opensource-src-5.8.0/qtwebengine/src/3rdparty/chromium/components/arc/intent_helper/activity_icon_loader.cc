// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/intent_helper/activity_icon_loader.h"

#include <string.h>

#include <tuple>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task_runner_util.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"
#include "ui/base/layout.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace arc {

namespace {

const size_t kSmallIconSizeInDip = 16;
const size_t kLargeIconSizeInDip = 20;
const size_t kMaxIconSizeInPx = 200;

const int kMinInstanceVersion = 3;  // see intent_helper.mojom

mojom::IntentHelperInstance* GetIntentHelperInstance(
    ActivityIconLoader::GetResult* out_error_code) {
  DCHECK(out_error_code);
  ArcBridgeService* bridge_service = arc::ArcBridgeService::Get();
  if (!bridge_service) {
    VLOG(2) << "ARC bridge is not ready.";
    *out_error_code = ActivityIconLoader::GetResult::FAILED_ARC_NOT_SUPPORTED;
    return nullptr;
  }
  mojom::IntentHelperInstance* intent_helper_instance =
      bridge_service->intent_helper()->instance();
  if (!intent_helper_instance) {
    VLOG(2) << "ARC intent helper instance is not ready.";
    *out_error_code = ActivityIconLoader::GetResult::FAILED_ARC_NOT_READY;
    return nullptr;
  }
  if (bridge_service->intent_helper()->version() < kMinInstanceVersion) {
    VLOG(1) << "ARC intent helper instance is too old.";
    *out_error_code = ActivityIconLoader::GetResult::FAILED_ARC_NOT_SUPPORTED;
    return nullptr;
  }
  return intent_helper_instance;
}

ui::ScaleFactor GetSupportedScaleFactor() {
  std::vector<ui::ScaleFactor> scale_factors = ui::GetSupportedScaleFactors();
  DCHECK(!scale_factors.empty());
  return scale_factors.back();
}

}  // namespace

ActivityIconLoader::Icons::Icons(const gfx::Image& icon16,
                                 const gfx::Image& icon20)
    : icon16(icon16), icon20(icon20) {}

ActivityIconLoader::ActivityName::ActivityName(const std::string& package_name,
                                               const std::string& activity_name)
    : package_name(package_name), activity_name(activity_name) {}

bool ActivityIconLoader::ActivityName::operator<(
    const ActivityName& other) const {
  return std::tie(package_name, activity_name) <
         std::tie(other.package_name, other.activity_name);
}

ActivityIconLoader::ActivityIconLoader()
    : scale_factor_(GetSupportedScaleFactor()) {}

ActivityIconLoader::~ActivityIconLoader() {}

void ActivityIconLoader::InvalidateIcons(const std::string& package_name) {
  for (auto it = cached_icons_.begin(); it != cached_icons_.end();) {
    if (it->first.package_name == package_name)
      it = cached_icons_.erase(it);
    else
      ++it;
  }
}

ActivityIconLoader::GetResult ActivityIconLoader::GetActivityIcons(
    const std::vector<ActivityName>& activities,
    const OnIconsReadyCallback& cb) {
  std::unique_ptr<ActivityToIconsMap> result(new ActivityToIconsMap);
  mojo::Array<mojom::ActivityNamePtr> activities_to_fetch;

  for (const auto& activity : activities) {
    const auto& it = cached_icons_.find(activity);
    if (it == cached_icons_.end()) {
      mojom::ActivityNamePtr name(mojom::ActivityName::New());
      name->package_name = activity.package_name;
      name->activity_name = activity.activity_name;
      activities_to_fetch.push_back(std::move(name));
    } else {
      result->insert(std::make_pair(activity, it->second));
    }
  }

  if (activities_to_fetch.empty()) {
    // If there's nothing to fetch, run the callback now.
    cb.Run(std::move(result));
    return GetResult::SUCCEEDED_SYNC;
  }

  GetResult error_code = GetResult::FAILED_ARC_NOT_SUPPORTED;
  mojom::IntentHelperInstance* instance = GetIntentHelperInstance(&error_code);
  if (!instance) {
    // The mojo channel is not yet ready (or not supported at all). Run the
    // callback with |result| that could be empty.
    cb.Run(std::move(result));
    return error_code;
  }

  // Fetch icons from ARC.
  instance->RequestActivityIcons(
      std::move(activities_to_fetch), mojom::ScaleFactor(scale_factor_),
      base::Bind(&ActivityIconLoader::OnIconsReady, this, base::Passed(&result),
                 cb));
  return GetResult::SUCCEEDED_ASYNC;
}

void ActivityIconLoader::OnIconsResizedForTesting(
    const OnIconsReadyCallback& cb,
    std::unique_ptr<ActivityToIconsMap> result) {
  OnIconsResized(base::MakeUnique<ActivityToIconsMap>(), cb, std::move(result));
}

void ActivityIconLoader::AddIconToCacheForTesting(const ActivityName& activity,
                                                  const gfx::Image& image) {
  cached_icons_.insert(std::make_pair(activity, Icons(image, image)));
}

// static
bool ActivityIconLoader::HasIconsReadyCallbackRun(GetResult result) {
  switch (result) {
    case GetResult::SUCCEEDED_ASYNC:
      return false;
    case GetResult::SUCCEEDED_SYNC:
    case GetResult::FAILED_ARC_NOT_READY:
    case GetResult::FAILED_ARC_NOT_SUPPORTED:
      break;
  }
  return true;
}

void ActivityIconLoader::OnIconsReady(
    std::unique_ptr<ActivityToIconsMap> cached_result,
    const OnIconsReadyCallback& cb,
    mojo::Array<mojom::ActivityIconPtr> icons) {
  ArcServiceManager* manager = ArcServiceManager::Get();
  base::PostTaskAndReplyWithResult(
      manager->blocking_task_runner().get(), FROM_HERE,
      base::Bind(&ActivityIconLoader::ResizeIcons, this, base::Passed(&icons)),
      base::Bind(&ActivityIconLoader::OnIconsResized, this,
                 base::Passed(&cached_result), cb));
}

std::unique_ptr<ActivityIconLoader::ActivityToIconsMap>
ActivityIconLoader::ResizeIcons(mojo::Array<mojom::ActivityIconPtr> icons) {
  // Runs only on the blocking pool.
  DCHECK(thread_checker_.CalledOnValidThread());
  std::unique_ptr<ActivityToIconsMap> result(new ActivityToIconsMap);

  for (size_t i = 0; i < icons.size(); ++i) {
    static const size_t kBytesPerPixel = 4;
    const mojom::ActivityIconPtr& icon = icons.at(i);
    if (icon->width > kMaxIconSizeInPx || icon->height > kMaxIconSizeInPx ||
        icon->width == 0 || icon->height == 0 ||
        icon->icon.size() != (icon->width * icon->height * kBytesPerPixel)) {
      continue;
    }

    SkBitmap bitmap;
    bitmap.allocPixels(SkImageInfo::MakeN32Premul(icon->width, icon->height));
    if (!bitmap.getPixels())
      continue;
    DCHECK_GE(bitmap.getSafeSize(), icon->icon.size());
    memcpy(bitmap.getPixels(), &icon->icon.front(), icon->icon.size());

    gfx::ImageSkia original(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));
    // Resize the original icon to the sizes intent_helper needs.
    gfx::ImageSkia icon_large(gfx::ImageSkiaOperations::CreateResizedImage(
        original, skia::ImageOperations::RESIZE_BEST,
        gfx::Size(kLargeIconSizeInDip, kLargeIconSizeInDip)));
    gfx::ImageSkia icon_small(gfx::ImageSkiaOperations::CreateResizedImage(
        original, skia::ImageOperations::RESIZE_BEST,
        gfx::Size(kSmallIconSizeInDip, kSmallIconSizeInDip)));

    result->insert(
        std::make_pair(ActivityName(icon->activity->package_name,
                                    icon->activity->activity_name),
                       Icons(gfx::Image(icon_small), gfx::Image(icon_large))));
  }

  return result;
}

void ActivityIconLoader::OnIconsResized(
    std::unique_ptr<ActivityToIconsMap> cached_result,
    const OnIconsReadyCallback& cb,
    std::unique_ptr<ActivityToIconsMap> result) {
  // Update |cached_icons_|.
  for (const auto& kv : *result) {
    cached_icons_.erase(kv.first);
    cached_icons_.insert(std::make_pair(kv.first, kv.second));
  }

  // Merge the results that were obtained from cache before doing IPC.
  result->insert(cached_result->begin(), cached_result->end());
  cb.Run(std::move(result));
}

}  // namespace arc
