// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/message_center/mojo/traits_test_service.mojom.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notifier_settings.h"

namespace message_center {
namespace {

class StructTraitsTest : public testing::Test, public mojom::TraitsTestService {
 public:
  StructTraitsTest() {}

 protected:
  mojom::TraitsTestServicePtr GetTraitsTestProxy() {
    return traits_test_bindings_.CreateInterfacePtrAndBind(this);
  }

  void Compare(const Notification& input, const Notification& output) {
    EXPECT_EQ(input.id(), output.id());
    EXPECT_EQ(input.title(), output.title());
    EXPECT_EQ(input.message(), output.message());
    EXPECT_EQ(input.icon().Width(), output.icon().Width());
    EXPECT_EQ(input.icon().Height(), output.icon().Height());
    EXPECT_EQ(input.display_source(), output.display_source());
    EXPECT_EQ(input.origin_url(), output.origin_url());
  }

 private:
  // TraitsTestService:
  void EchoNotification(const Notification& n,
                        const EchoNotificationCallback& callback) override {
    callback.Run(n);
  }

  base::MessageLoop loop_;
  mojo::BindingSet<TraitsTestService> traits_test_bindings_;

  DISALLOW_COPY_AND_ASSIGN(StructTraitsTest);
};

}  // namespace

TEST_F(StructTraitsTest, Notification) {
  NotificationType type = NotificationType::NOTIFICATION_TYPE_SIMPLE;
  std::string id("notification_id");
  base::string16 title(base::ASCIIToUTF16("Notification"));
  base::string16 message(base::ASCIIToUTF16("I'm a notification!"));
  gfx::Image icon(gfx::test::CreateImage(64, 64));
  base::string16 display_source(base::ASCIIToUTF16("display_source"));
  GURL origin_url("www.example.com");
  NotifierId notifier_id(NotifierId::NotifierType::APPLICATION, id);
  Notification input(type, id, title, message, icon, display_source, origin_url,
                     notifier_id, RichNotificationData(), nullptr);
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  Notification output;
  proxy->EchoNotification(input, &output);
  Compare(input, output);
}

TEST_F(StructTraitsTest, EmptyIconNotification) {
  NotificationType type = NotificationType::NOTIFICATION_TYPE_SIMPLE;
  std::string id("notification_id");
  base::string16 title(base::ASCIIToUTF16("Notification"));
  base::string16 message(
      base::ASCIIToUTF16("I'm a notification with no icon!"));
  base::string16 display_source(base::ASCIIToUTF16("display_source"));
  GURL origin_url("www.example.com");
  NotifierId notifier_id(NotifierId::NotifierType::APPLICATION, id);
  Notification input(type, id, title, message, gfx::Image(), display_source,
                     origin_url, notifier_id, RichNotificationData(), nullptr);
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  Notification output;
  proxy->EchoNotification(input, &output);
  Compare(input, output);
}

}  // namespace message_center
