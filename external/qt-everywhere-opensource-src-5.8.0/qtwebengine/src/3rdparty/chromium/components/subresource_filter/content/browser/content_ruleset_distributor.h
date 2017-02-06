// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CONTENT_RULESET_DISTRIBUTOR_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CONTENT_RULESET_DISTRIBUTOR_H_

#include "base/files/file.h"
#include "base/macros.h"
#include "components/subresource_filter/core/browser/ruleset_distributor.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace subresource_filter {

// The content-layer specific implementation that receives the new versions of
// subresource filtering rules from the RulesetService, and distributes them to
// renderer processes, each with its own RulesetDealer.
//
// The distribution pipeline looks like this:
//
//                      RulesetService
//                           |
//                           v                  Browser
//                    RulesetDistributor
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
class ContentRulesetDistributor : public RulesetDistributor,
                                  content::NotificationObserver {
 public:
  ContentRulesetDistributor();
  ~ContentRulesetDistributor() override;

  // RulesetDistributor:
  void PublishNewVersion(base::File ruleset_data) override;

 private:
  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  content::NotificationRegistrar notification_registrar_;
  base::File ruleset_data_;

  DISALLOW_COPY_AND_ASSIGN(ContentRulesetDistributor);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_CONTENT_RULESET_DISTRIBUTOR_H_
