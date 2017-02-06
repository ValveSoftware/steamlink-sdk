// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_INTENT_HELPER_LINK_HANDLER_MODEL_IMPL_H_
#define COMPONENTS_ARC_INTENT_HELPER_LINK_HANDLER_MODEL_IMPL_H_

#include <memory>

#include "ash/link_handler_model.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/arc/arc_service.h"
#include "components/arc/common/intent_helper.mojom.h"
#include "components/arc/intent_helper/activity_icon_loader.h"

namespace arc {

class LinkHandlerModelImpl : public ash::LinkHandlerModel {
 public:
  explicit LinkHandlerModelImpl(scoped_refptr<ActivityIconLoader> icon_loader);
  ~LinkHandlerModelImpl() override;

  // ash::LinkHandlerModel overrides:
  void AddObserver(Observer* observer) override;
  void OpenLinkWithHandler(const GURL& url, uint32_t handler_id) override;

  // Starts retrieving handler information for the |url| and returns true.
  // Returns false when the information cannot be retrieved. In that case,
  // the caller should delete |this| object.
  bool Init(const GURL& url);

  static GURL RewriteUrlFromQueryIfAvailableForTesting(const GURL& url);

 private:
  mojom::IntentHelperInstance* GetIntentHelper();
  void OnUrlHandlerList(mojo::Array<mojom::UrlHandlerInfoPtr> handlers);
  void NotifyObserver(
      std::unique_ptr<ActivityIconLoader::ActivityToIconsMap> icons);

  // Checks if the |url| matches the following pattern:
  //   "http(s)://<valid_google_hostname>/url?...&url=<valid_url>&..."
  // If it does, creates a new GURL object from the <valid_url> and returns it.
  // Otherwise, returns the original |url| as-us.
  static GURL RewriteUrlFromQueryIfAvailable(const GURL& url);

  base::ObserverList<Observer> observer_list_;

  // Url handler info passed from ARC.
  mojo::Array<mojom::UrlHandlerInfoPtr> handlers_;
  // Activity icon info passed from ARC.
  ActivityIconLoader::ActivityToIconsMap icons_;

  // Use refptr to retain the object even if ArcIntentHelperBridge is destructed
  // first.
  scoped_refptr<ActivityIconLoader> icon_loader_;

  // Always keep this the last member of this class to make sure it's the
  // first thing to be destructed.
  base::WeakPtrFactory<LinkHandlerModelImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(LinkHandlerModelImpl);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_INTENT_HELPER_LINK_HANDLER_MODEL_IMPL_H_
