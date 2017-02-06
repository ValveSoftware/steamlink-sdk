// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/intent_helper/link_handler_model_impl.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/google/core/browser/google_util.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace arc {

namespace {

constexpr int kMinInstanceVersion = 2;  // see intent_helper.mojom
constexpr int kMaxValueLen = 2048;

bool GetQueryValue(const GURL& url,
                   const std::string& key_to_find,
                   base::string16* out) {
  const std::string str(url.query());

  url::Component query(0, str.length());
  url::Component key;
  url::Component value;

  while (url::ExtractQueryKeyValue(str.c_str(), &query, &key, &value)) {
    if (!value.is_nonempty())
      continue;
    if (str.substr(key.begin, key.len) == key_to_find) {
      if (value.len >= kMaxValueLen)
        return false;
      url::RawCanonOutputW<kMaxValueLen> output;
      url::DecodeURLEscapeSequences(str.c_str() + value.begin, value.len,
                                    &output);
      *out = base::string16(output.data(), output.length());
      return true;
    }
  }
  return false;
}

}  // namespace

LinkHandlerModelImpl::LinkHandlerModelImpl(
    scoped_refptr<ActivityIconLoader> icon_loader)
    : icon_loader_(icon_loader), weak_ptr_factory_(this) {}

LinkHandlerModelImpl::~LinkHandlerModelImpl() {}

bool LinkHandlerModelImpl::Init(const GURL& url) {
  mojom::IntentHelperInstance* intent_helper_instance = GetIntentHelper();
  if (!intent_helper_instance)
    return false;

  // Check if ARC apps can handle the |url|. Since the information is held in
  // a different (ARC) process, issue a mojo IPC request. Usually, the
  // callback function, OnUrlHandlerList, is called within a few milliseconds
  // even on the slowest Chromebook we support.
  const GURL rewritten(RewriteUrlFromQueryIfAvailable(url));
  intent_helper_instance->RequestUrlHandlerList(
      rewritten.spec(), base::Bind(&LinkHandlerModelImpl::OnUrlHandlerList,
                                   weak_ptr_factory_.GetWeakPtr()));
  return true;
}

void LinkHandlerModelImpl::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void LinkHandlerModelImpl::OpenLinkWithHandler(const GURL& url,
                                               uint32_t handler_id) {
  mojom::IntentHelperInstance* intent_helper_instance = GetIntentHelper();
  if (!intent_helper_instance)
    return;
  if (handler_id >= handlers_.size())
    return;
  const GURL rewritten(RewriteUrlFromQueryIfAvailable(url));
  intent_helper_instance->HandleUrl(rewritten.spec(),
                                    handlers_[handler_id]->package_name);
}

mojom::IntentHelperInstance* LinkHandlerModelImpl::GetIntentHelper() {
  ArcBridgeService* bridge_service = arc::ArcBridgeService::Get();
  if (!bridge_service) {
    DLOG(WARNING) << "ARC bridge is not ready.";
    return nullptr;
  }
  mojom::IntentHelperInstance* intent_helper_instance =
      bridge_service->intent_helper()->instance();
  if (!intent_helper_instance) {
    DLOG(WARNING) << "ARC intent helper instance is not ready.";
    return nullptr;
  }
  if (bridge_service->intent_helper()->version() < kMinInstanceVersion) {
    DLOG(WARNING) << "ARC intent helper instance is too old.";
    return nullptr;
  }
  return intent_helper_instance;
}

void LinkHandlerModelImpl::OnUrlHandlerList(
    mojo::Array<mojom::UrlHandlerInfoPtr> handlers) {
  handlers_ = ArcIntentHelperBridge::FilterOutIntentHelper(std::move(handlers));

  bool icon_info_notified = false;
  if (icon_loader_) {
    std::vector<ActivityIconLoader::ActivityName> activities;
    for (size_t i = 0; i < handlers_.size(); ++i) {
      activities.emplace_back(handlers_[i]->package_name,
                              handlers_[i]->activity_name);
    }
    const ActivityIconLoader::GetResult result = icon_loader_->GetActivityIcons(
        activities, base::Bind(&LinkHandlerModelImpl::NotifyObserver,
                               weak_ptr_factory_.GetWeakPtr()));
    icon_info_notified = ActivityIconLoader::HasIconsReadyCallbackRun(result);
  }

  if (!icon_info_notified) {
    // Call NotifyObserver() without icon information, unless
    // GetActivityIcons has already called it. Otherwise if we delay the
    // notification waiting for all icons, context menu may flicker.
    NotifyObserver(nullptr);
  }
}

void LinkHandlerModelImpl::NotifyObserver(
    std::unique_ptr<ActivityIconLoader::ActivityToIconsMap> icons) {
  if (icons) {
    icons_.insert(icons->begin(), icons->end());
    icons.reset();
  }

  std::vector<ash::LinkHandlerInfo> handlers;
  for (size_t i = 0; i < handlers_.size(); ++i) {
    gfx::Image icon;
    const ActivityIconLoader::ActivityName activity(
        handlers_[i]->package_name, handlers_[i]->activity_name);
    const auto it = icons_.find(activity);
    if (it != icons_.end())
      icon = it->second.icon16;
    // Use the handler's index as an ID.
    ash::LinkHandlerInfo handler = {handlers_[i]->name.get(), icon, i};
    handlers.push_back(handler);
  }
  FOR_EACH_OBSERVER(Observer, observer_list_, ModelChanged(handlers));
}

// static
GURL LinkHandlerModelImpl::RewriteUrlFromQueryIfAvailableForTesting(
    const GURL& url) {
  return RewriteUrlFromQueryIfAvailable(url);
}

// static
GURL LinkHandlerModelImpl::RewriteUrlFromQueryIfAvailable(const GURL& url) {
  static const char kPathToFind[] = "/url";
  static const char kKeyToFind[] = "url";

  if (!google_util::IsGoogleHostname(url.host_piece(),
                                     google_util::DISALLOW_SUBDOMAIN)) {
    return url;
  }
  if (!url.has_path() || url.path() != kPathToFind)
    return url;

  base::string16 value;
  if (!GetQueryValue(url, kKeyToFind, &value))
    return url;

  const GURL new_url(value);
  if (!new_url.is_valid())
    return url;
  return new_url;
}

}  // namespace arc
