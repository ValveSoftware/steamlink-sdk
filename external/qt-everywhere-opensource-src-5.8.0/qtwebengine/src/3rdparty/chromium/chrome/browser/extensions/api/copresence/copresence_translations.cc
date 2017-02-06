// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/copresence/copresence_translations.h"

#include "chrome/common/extensions/api/copresence.h"
#include "components/copresence/proto/data.pb.h"
#include "components/copresence/proto/enums.pb.h"
#include "components/copresence/proto/rpcs.pb.h"

using copresence::AUDIO_CONFIGURATION_AUDIBLE;
using copresence::AUDIO_CONFIGURATION_UNKNOWN;
using copresence::BROADCAST_AND_SCAN;
using copresence::BROADCAST_ONLY;
using copresence::BROADCAST_SCAN_CONFIGURATION_UNKNOWN;
using copresence::BroadcastScanConfiguration;
using copresence::ReportRequest;
using copresence::SCAN_ONLY;
using copresence::TokenExchangeStrategy;

using extensions::api::copresence::Strategy;

namespace {

const int kDefaultTimeToLiveMs = 5 * 60 * 1000;    // 5 minutes.
const int kMaxTimeToLiveMs = 24 * 60 * 60 * 1000;  // 24 hours.

// Checks and returns the ttl provided by the user. If invalid, returns -1.
int SanitizeTtl(int* user_ttl) {
  return !user_ttl
      ? kDefaultTimeToLiveMs
      : (*user_ttl <= 0 || *user_ttl > kMaxTimeToLiveMs ? -1 : *user_ttl);
}

BroadcastScanConfiguration TranslateStrategy(const Strategy& strategy) {
  bool only_broadcast = strategy.only_broadcast && *strategy.only_broadcast;
  bool only_scan = strategy.only_scan && *strategy.only_scan;

  if (only_broadcast && only_scan)
    return BROADCAST_AND_SCAN;
  if (only_broadcast)
    return BROADCAST_ONLY;
  if (only_scan)
    return SCAN_ONLY;

  return BROADCAST_SCAN_CONFIGURATION_UNKNOWN;
}

// The strategy may be null (unspecified), so we pass it as a pointer.
void SetTokenExchangeStrategy(const Strategy* strategy,
                              BroadcastScanConfiguration default_config,
                              TokenExchangeStrategy* strategy_proto) {
  if (strategy) {
    BroadcastScanConfiguration config;
    if (strategy->low_power && *(strategy->low_power)) {
      config = BROADCAST_SCAN_CONFIGURATION_UNKNOWN;
    } else {
      config = TranslateStrategy(*strategy);
      if (config == BROADCAST_SCAN_CONFIGURATION_UNKNOWN)
        config = default_config;
    }

    strategy_proto->set_broadcast_scan_configuration(config);
    strategy_proto->set_audio_configuration(
        strategy->audible && *strategy->audible ? AUDIO_CONFIGURATION_AUDIBLE
                                                : AUDIO_CONFIGURATION_UNKNOWN);
  } else {
    strategy_proto->set_broadcast_scan_configuration(default_config);
    strategy_proto->set_audio_configuration(AUDIO_CONFIGURATION_UNKNOWN);
  }
}

}  // namespace

namespace extensions {

using api::copresence::Operation;

// Adds a publish operation to the report request. Returns false if the
// publish operation was invalid.
bool AddPublishToRequest(const std::string& app_id,
                         const api::copresence::PublishOperation& publish,
                         ReportRequest* request) {
  copresence::PublishedMessage* publish_proto =
      request->mutable_manage_messages_request()->add_message_to_publish();
  publish_proto->mutable_access_policy()->mutable_acl()->set_acl_type(
      copresence::NO_ACL_CHECK);
  publish_proto->set_id(publish.id);
  publish_proto->mutable_message()->mutable_type()->set_type(
      publish.message.type);
  publish_proto->mutable_message()->set_payload(publish.message.payload.data(),
                                                publish.message.payload.size());

  int ttl = SanitizeTtl(publish.time_to_live_millis.get());
  if (ttl < 0)
    return false;
  publish_proto->mutable_access_policy()->set_ttl_millis(ttl);

  SetTokenExchangeStrategy(publish.strategies.get(),
                           BROADCAST_ONLY,
                           publish_proto->mutable_token_exchange_strategy());

  DVLOG(2) << "Publishing message of type " << publish.message.type << ":\n"
           << std::string(publish.message.payload.begin(),
                          publish.message.payload.end());
  // TODO(ckehoe): Validate that required fields are non-empty, etc.
  return true;
}

// Adds an unpublish operation to the report request. Returns false if the
// publish id was invalid.
bool AddUnpublishToRequest(const std::string& publish_id,
                           ReportRequest* request) {
  if (publish_id.empty())
    return false;

  request->mutable_manage_messages_request()->add_id_to_unpublish(publish_id);
  DVLOG(2) << "Unpublishing message \"" << publish_id << "\"";
  return true;
}

// Adds a subscribe operation to the report request. Returns false if the
// subscription operation was invalid.
bool AddSubscribeToRequest(
    const std::string& app_id,
    const api::copresence::SubscribeOperation& subscription,
    SubscriptionToAppMap* apps_by_subscription_id,
    ReportRequest* request) {
  // Associate the subscription id with the app id.
  SubscriptionToAppMap::iterator previous_subscription =
      apps_by_subscription_id->find(subscription.id);
  if (previous_subscription == apps_by_subscription_id->end()) {
    (*apps_by_subscription_id)[subscription.id] = app_id;
  } else if (previous_subscription->second == app_id) {
    VLOG(2) << "Overwriting subscription id \"" << subscription.id
            << "\" for app \"" << app_id << "\"";
  } else {
    // A conflicting association exists already.
    VLOG(1) << "Subscription id \"" << subscription.id
            << "\" used by two apps: \"" << previous_subscription->second
            << "\" and \"" << app_id << "\"";
    return false;
  }

  // Convert from IDL to server subscription format.
  copresence::Subscription* subscription_proto =
      request->mutable_manage_subscriptions_request()->add_subscription();
  subscription_proto->set_id(subscription.id);
  int ttl = SanitizeTtl(subscription.time_to_live_millis.get());
  if (ttl < 0)
    return false;
  subscription_proto->set_ttl_millis(ttl);

  subscription_proto->mutable_message_type()->set_type(
      subscription.filter.type);

  SetTokenExchangeStrategy(
      subscription.strategies.get(),
      SCAN_ONLY,
      subscription_proto->mutable_token_exchange_strategy());

  DVLOG(2) << "Subscribing for messages of type " << subscription.filter.type;
  // TODO(ckehoe): Validate that required fields are non-empty, etc.
  return true;
}

// Adds an unpublish operation to the report request. Returns false if the
// subscription id was invalid.
bool AddUnsubscribeToRequest(const std::string& app_id,
                             const std::string& subscription_id,
                             SubscriptionToAppMap* apps_by_subscription_id,
                             ReportRequest* request) {
  if (subscription_id.empty())
    return false;

  // Check that this subscription id belongs to this app.
  SubscriptionToAppMap::iterator subscription =
      apps_by_subscription_id->find(subscription_id);
  if (subscription == apps_by_subscription_id->end()) {
    LOG(ERROR) << "No such subscription \"" << subscription_id
               << "\". Cannot unsubscribe.";
    return false;
  } else if (subscription->second != app_id) {
    LOG(ERROR) << "Subscription \"" << subscription_id
               << "\" does not belong to app \"" << app_id
               << "\". Cannot unsubscribe.";
    return false;
  } else {
    apps_by_subscription_id->erase(subscription);
  }

  request->mutable_manage_subscriptions_request()->add_id_to_unsubscribe(
      subscription_id);
  DVLOG(2) << "Cancelling subscription \"" << subscription_id << "\" for app \""
           << app_id << "\"";
  return true;
}

bool PrepareReportRequestProto(const std::vector<Operation>& operations,
                               const std::string& app_id,
                               SubscriptionToAppMap* apps_by_subscription_id,
                               ReportRequest* request) {
  for (const Operation& op : operations) {
    // Verify our object has exactly one operation.
    if (static_cast<int>(op.publish != nullptr) +
            static_cast<int>(op.subscribe != nullptr) +
            static_cast<int>(op.unpublish != nullptr) +
            static_cast<int>(op.unsubscribe != nullptr) !=
        1) {
      return false;
    }

    if (op.publish) {
      if (!AddPublishToRequest(app_id, *(op.publish), request))
        return false;
    } else if (op.subscribe) {
      if (!AddSubscribeToRequest(app_id, *(op.subscribe),
                                 apps_by_subscription_id, request))
        return false;
    } else if (op.unpublish) {
      if (!AddUnpublishToRequest(op.unpublish->unpublish_id, request))
        return false;
    } else {  // if (op.unsubscribe)
      if (!AddUnsubscribeToRequest(app_id, op.unsubscribe->unsubscribe_id,
                                   apps_by_subscription_id, request))
        return false;
    }
  }

  return true;
}

}  // namespace extensions
