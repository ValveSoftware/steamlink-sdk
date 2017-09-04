// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CONTENT_RULESET_SERVICE_DELEGATE_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CONTENT_RULESET_SERVICE_DELEGATE_H_

#include "base/callback.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "components/subresource_filter/core/browser/ruleset_service_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace subresource_filter {

// The content-layer specific implementation of RulesetServiceDelegate.
//
// Its main responsibility is receiving new versions of subresource filtering
// rules from the RulesetService, and distributing them to renderer processes,
// where they will be memory-mapped as-needed by the RulesetDealer.
//
// The distribution pipeline looks like this:
//
//                      RulesetService
//                           |
//                           v                  Browser
//                 RulesetServiceDelegate
//                     |              |
//        - - - - - - -|- - - - - - - |- - - - - - - - - -
//                     |       |      |
//                     v              v
//           RulesetDealer     |   RulesetDealer
//                 |                |       |
//                 |           |    |       v
//                 v                |      SubresourceFilterAgent
//    SubresourceFilterAgent   |    v
//                                SubresourceFilterAgent
//                             |
//
//         Renderer #1         |          Renderer #n
//
class ContentRulesetServiceDelegate : public RulesetServiceDelegate,
                                      content::NotificationObserver {
 public:
  ContentRulesetServiceDelegate();
  ~ContentRulesetServiceDelegate() override;

  void SetRulesetPublishedCallbackForTesting(base::Closure callback);

  // RulesetDistributor:
  void PostAfterStartupTask(base::Closure task) override;
  void PublishNewRulesetVersion(base::File ruleset_data) override;

 private:
  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  content::NotificationRegistrar notification_registrar_;
  base::File ruleset_data_;
  base::Closure ruleset_published_callback_;

  DISALLOW_COPY_AND_ASSIGN(ContentRulesetServiceDelegate);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CONTENT_RULESET_SERVICE_DELEGATE_H_
