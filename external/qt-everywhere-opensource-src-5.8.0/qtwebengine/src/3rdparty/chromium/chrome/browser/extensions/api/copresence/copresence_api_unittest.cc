// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/copresence/copresence_api.h"

#include <utility>

#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "components/copresence/proto/data.pb.h"
#include "components/copresence/proto/rpcs.pb.h"
#include "components/copresence/public/copresence_manager.h"

using base::ListValue;
using copresence::AUDIO_CONFIGURATION_AUDIBLE;
using copresence::AUDIO_CONFIGURATION_UNKNOWN;
using copresence::BROADCAST_ONLY;
using copresence::CopresenceDelegate;
using copresence::CopresenceManager;
using copresence::FAIL;
using copresence::PublishedMessage;
using copresence::ReportRequest;
using copresence::SCAN_ONLY;
using copresence::Subscription;
using google::protobuf::RepeatedPtrField;

namespace test_utils = extension_function_test_utils;

namespace extensions {

using api::copresence::Message;
using api::copresence::Operation;
using api::copresence::PublishOperation;
using api::copresence::Strategy;
using api::copresence::SubscribeOperation;
using api::copresence::UnpublishOperation;
using api::copresence::UnsubscribeOperation;


PublishOperation* CreatePublish(const std::string& id) {
  PublishOperation* publish = new PublishOperation;

  publish->id = id;
  publish->time_to_live_millis.reset(new int(1000));
  publish->message.type = "joke";
  std::string payload("Knock Knock!");
  publish->message.payload.assign(payload.begin(), payload.end());

  return publish;
}

SubscribeOperation* CreateSubscribe(const std::string& id) {
  SubscribeOperation* subscribe = new SubscribeOperation;

  subscribe->id = id;
  subscribe->time_to_live_millis.reset(new int(1000));
  subscribe->filter.type = "joke";

  return subscribe;
}

template <typename T>
bool GetOnly(const RepeatedPtrField<T>& things, T* out) {
  if (things.size() != 1)
    return false;

  *out = things.Get(0);
  return true;
}

class FakeCopresenceManager : public CopresenceManager {
 public:
  explicit FakeCopresenceManager(CopresenceDelegate* delegate)
      : delegate_(delegate) {}
  ~FakeCopresenceManager() override {}

  // CopresenceManager overrides.
  copresence::CopresenceState* state() override {
    NOTREACHED();
    return nullptr;
  }
  void ExecuteReportRequest(
      const ReportRequest& request,
      const std::string& app_id,
      const std::string& /* auth_token */,
      const copresence::StatusCallback& status_callback) override {
    request_ = request;
    app_id_ = app_id;
    status_callback.Run(copresence::SUCCESS);
  }

  CopresenceDelegate* delegate_;

  ReportRequest request_;
  std::string app_id_;
};

class CopresenceApiUnittest : public ExtensionApiUnittest {
 public:
  CopresenceApiUnittest() {}
  ~CopresenceApiUnittest() override {}

  void SetUp() override {
    ExtensionApiUnittest::SetUp();

    CopresenceService* service =
        CopresenceService::GetFactoryInstance()->Get(profile());
    copresence_manager_ = new FakeCopresenceManager(service);
    service->set_manager_for_testing(
        base::WrapUnique<CopresenceManager>(copresence_manager_));
  }

  // Takes ownership of the operation_list.
  bool ExecuteOperations(ListValue* operation_list) {
    std::unique_ptr<ListValue> args_list(new ListValue);
    args_list->Append(operation_list);

    scoped_refptr<UIThreadExtensionFunction> function =
        new CopresenceExecuteFunction;
    function->set_extension(extension());
    function->set_browser_context(profile());
    function->set_has_callback(true);
    test_utils::RunFunction(function.get(), std::move(args_list), browser(),
                            test_utils::NONE);
    return function->GetResultList();
  }

  bool ExecuteOperation(std::unique_ptr<Operation> operation) {
    ListValue* operation_list = new ListValue;
    operation_list->Append(operation->ToValue());
    return ExecuteOperations(operation_list);
  }

  const ReportRequest& request_sent() const {
    return copresence_manager_->request_;
  }

  const std::string& app_id_sent() const {
    return copresence_manager_->app_id_;
  }

  void clear_app_id() {
    copresence_manager_->app_id_.clear();
  }

  CopresenceDelegate* delegate() {
    return copresence_manager_->delegate_;
  }

 protected:
  FakeCopresenceManager* copresence_manager_;
};

TEST_F(CopresenceApiUnittest, Publish) {
  std::unique_ptr<PublishOperation> publish(CreatePublish("pub"));
  publish->strategies.reset(new Strategy);
  publish->strategies->only_broadcast.reset(new bool(true));  // Default

  std::unique_ptr<Operation> operation(new Operation);
  operation->publish = std::move(publish);

  clear_app_id();
  EXPECT_TRUE(ExecuteOperation(std::move(operation)));
  EXPECT_EQ(extension()->id(), app_id_sent());

  PublishedMessage message;
  ASSERT_TRUE(GetOnly(
      request_sent().manage_messages_request().message_to_publish(), &message));
  EXPECT_EQ("pub", message.id());
  EXPECT_EQ(1000, message.access_policy().ttl_millis());
  EXPECT_EQ(copresence::NO_ACL_CHECK, message.access_policy().acl().acl_type());
  EXPECT_EQ("joke", message.message().type().type());
  EXPECT_EQ("Knock Knock!", message.message().payload());
  EXPECT_EQ(BROADCAST_ONLY,
            message.token_exchange_strategy().broadcast_scan_configuration());
  EXPECT_EQ(AUDIO_CONFIGURATION_UNKNOWN,
            message.token_exchange_strategy().audio_configuration());
}

TEST_F(CopresenceApiUnittest, Subscribe) {
  std::unique_ptr<SubscribeOperation> subscribe(CreateSubscribe("sub"));
  subscribe->strategies.reset(new Strategy);
  subscribe->strategies->only_broadcast.reset(new bool(true));  // Not default
  subscribe->strategies->audible.reset(new bool(true));  // Not default

  std::unique_ptr<Operation> operation(new Operation);
  operation->subscribe = std::move(subscribe);

  clear_app_id();
  EXPECT_TRUE(ExecuteOperation(std::move(operation)));
  EXPECT_EQ(extension()->id(), app_id_sent());

  Subscription subscription;
  ASSERT_TRUE(GetOnly(
      request_sent().manage_subscriptions_request().subscription(),
      &subscription));
  EXPECT_EQ("sub", subscription.id());
  EXPECT_EQ(1000, subscription.ttl_millis());
  EXPECT_EQ("joke", subscription.message_type().type());
  copresence::BroadcastScanConfiguration broadcast_scan =
      subscription.token_exchange_strategy().broadcast_scan_configuration();
  EXPECT_EQ(BROADCAST_ONLY, broadcast_scan);
  EXPECT_EQ(AUDIO_CONFIGURATION_AUDIBLE,
            subscription.token_exchange_strategy().audio_configuration());
}

TEST_F(CopresenceApiUnittest, DefaultStrategies) {
  std::unique_ptr<Operation> publish_operation(new Operation);
  publish_operation->publish.reset(CreatePublish("pub"));

  std::unique_ptr<Operation> subscribe_operation(new Operation);
  subscribe_operation->subscribe.reset(CreateSubscribe("sub"));

  ListValue* operation_list = new ListValue;
  operation_list->Append(publish_operation->ToValue());
  operation_list->Append(subscribe_operation->ToValue());
  EXPECT_TRUE(ExecuteOperations(operation_list));

  EXPECT_EQ(BROADCAST_ONLY,
            request_sent().manage_messages_request().message_to_publish(0)
                .token_exchange_strategy().broadcast_scan_configuration());
  EXPECT_EQ(SCAN_ONLY,
            request_sent().manage_subscriptions_request().subscription(0)
                .token_exchange_strategy().broadcast_scan_configuration());
}

TEST_F(CopresenceApiUnittest, LowPowerStrategy) {
  std::unique_ptr<Operation> subscribe_operation(new Operation);
  subscribe_operation->subscribe.reset(CreateSubscribe("sub"));
  subscribe_operation->subscribe->strategies.reset(new Strategy);
  subscribe_operation->subscribe->strategies->low_power.reset(new bool(true));

  ListValue* operation_list = new ListValue;
  operation_list->Append(subscribe_operation->ToValue());
  EXPECT_TRUE(ExecuteOperations(operation_list));

  EXPECT_EQ(copresence::BROADCAST_SCAN_CONFIGURATION_UNKNOWN,
            request_sent().manage_subscriptions_request().subscription(0)
                .token_exchange_strategy().broadcast_scan_configuration());
}

TEST_F(CopresenceApiUnittest, UnPubSub) {
  // First we need to create a publish and a subscribe to cancel.
  std::unique_ptr<Operation> publish_operation(new Operation);
  std::unique_ptr<Operation> subscribe_operation(new Operation);
  publish_operation->publish.reset(CreatePublish("pub"));
  subscribe_operation->subscribe.reset(CreateSubscribe("sub"));
  ListValue* operation_list = new ListValue;
  operation_list->Append(publish_operation->ToValue());
  operation_list->Append(subscribe_operation->ToValue());
  EXPECT_TRUE(ExecuteOperations(operation_list));

  std::unique_ptr<Operation> unpublish_operation(new Operation);
  unpublish_operation->unpublish.reset(new UnpublishOperation);
  unpublish_operation->unpublish->unpublish_id = "pub";

  std::unique_ptr<Operation> unsubscribe_operation(new Operation);
  unsubscribe_operation->unsubscribe.reset(new UnsubscribeOperation);
  unsubscribe_operation->unsubscribe->unsubscribe_id = "sub";

  operation_list = new ListValue;
  operation_list->Append(unpublish_operation->ToValue());
  operation_list->Append(unsubscribe_operation->ToValue());
  EXPECT_TRUE(ExecuteOperations(operation_list));

  std::string unpublish_id;
  ASSERT_TRUE(GetOnly(
      request_sent().manage_messages_request().id_to_unpublish(),
      &unpublish_id));
  EXPECT_EQ("pub", unpublish_id);

  std::string unsubscribe_id;
  ASSERT_TRUE(GetOnly(
      request_sent().manage_subscriptions_request().id_to_unsubscribe(),
      &unsubscribe_id));
  EXPECT_EQ("sub", unsubscribe_id);
}

TEST_F(CopresenceApiUnittest, BadId) {
  std::unique_ptr<Operation> unsubscribe_operation(new Operation);
  unsubscribe_operation->unsubscribe.reset(new UnsubscribeOperation);
  unsubscribe_operation->unsubscribe->unsubscribe_id = "invalid id";

  EXPECT_FALSE(ExecuteOperation(std::move(unsubscribe_operation)));
}

TEST_F(CopresenceApiUnittest, MultipleOperations) {
  std::unique_ptr<Operation> multi_operation(new Operation);
  multi_operation->publish.reset(CreatePublish("pub"));
  multi_operation->subscribe.reset(CreateSubscribe("sub"));

  EXPECT_FALSE(ExecuteOperation(std::move(multi_operation)));
}

}  // namespace extensions

// TODO(ckehoe): add tests for auth tokens and api key functionality
