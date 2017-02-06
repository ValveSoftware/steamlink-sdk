// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/notifications/notification_id_generator.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {
namespace {

const int kRenderProcessId = 42;
const int64_t kPersistentNotificationId = 430;
const int kNonPersistentNotificationId = 5400;
const char kExampleTag[] = "example";

class TestBrowserContextConfigurableIncognito : public TestBrowserContext {
 public:
  TestBrowserContextConfigurableIncognito() {}
  ~TestBrowserContextConfigurableIncognito() override {}

  void set_incognito(bool incognito) { incognito_ = incognito; }

  // TestBrowserContext implementation.
  bool IsOffTheRecord() const override { return incognito_; }

 private:
  bool incognito_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestBrowserContextConfigurableIncognito);
};

class NotificationIdGeneratorTest : public ::testing::Test {
 public:
  NotificationIdGeneratorTest()
      : generator_(&browser_context_, kRenderProcessId) {}

  void SetUp() override {}

 protected:
  GURL origin() const { return GURL("https://example.com"); }

  TestBrowserContextConfigurableIncognito* browser_context() {
    return &browser_context_;
  }

  NotificationIdGenerator* generator() { return &generator_; }

 private:
  TestBrowserContextConfigurableIncognito browser_context_;
  NotificationIdGenerator generator_;
};

// -----------------------------------------------------------------------------
// Persistent and non-persistent notifications
//
// Tests that cover logic common to both persistent and non-persistent
// notifications: different browser contexts, Incognito mode or not,

// Two calls to the generator with exactly the same information should result
// in exactly the same notification ids being generated.
TEST_F(NotificationIdGeneratorTest, DeterministicGeneration) {
  // Persistent notifications.
  EXPECT_EQ(generator()->GenerateForPersistentNotification(
                origin(), kExampleTag, kPersistentNotificationId),
            generator()->GenerateForPersistentNotification(
                origin(), kExampleTag, kPersistentNotificationId));

  EXPECT_EQ(generator()->GenerateForPersistentNotification(
                origin(), "" /* tag */, kPersistentNotificationId),
            generator()->GenerateForPersistentNotification(
                origin(), "" /* tag */, kPersistentNotificationId));

  // Non-persistent notifications.
  EXPECT_EQ(generator()->GenerateForNonPersistentNotification(
                origin(), kExampleTag, kNonPersistentNotificationId),
            generator()->GenerateForNonPersistentNotification(
                origin(), kExampleTag, kNonPersistentNotificationId));

  EXPECT_EQ(generator()->GenerateForNonPersistentNotification(
                origin(), "" /* tag */, kNonPersistentNotificationId),
            generator()->GenerateForNonPersistentNotification(
                origin(), "" /* tag */, kNonPersistentNotificationId));
}

// Uniqueness of notification ids will be impacted by the browser context.
TEST_F(NotificationIdGeneratorTest, DifferentBrowserContexts) {
  TestBrowserContextConfigurableIncognito second_browser_context;
  NotificationIdGenerator second_generator(&second_browser_context,
                                           kRenderProcessId);

  // Persistent notifications.
  EXPECT_NE(generator()->GenerateForPersistentNotification(
                origin(), kExampleTag, kPersistentNotificationId),
            second_generator.GenerateForPersistentNotification(
                origin(), kExampleTag, kPersistentNotificationId));

  EXPECT_NE(generator()->GenerateForPersistentNotification(
                origin(), "" /* tag */, kPersistentNotificationId),
            second_generator.GenerateForPersistentNotification(
                origin(), "" /* tag */, kPersistentNotificationId));

  // Non-persistent notifications.
  EXPECT_NE(generator()->GenerateForNonPersistentNotification(
                origin(), kExampleTag, kNonPersistentNotificationId),
            second_generator.GenerateForNonPersistentNotification(
                origin(), kExampleTag, kNonPersistentNotificationId));

  EXPECT_NE(generator()->GenerateForNonPersistentNotification(
                origin(), "" /* tag */, kNonPersistentNotificationId),
            second_generator.GenerateForNonPersistentNotification(
                origin(), "" /* tag */, kNonPersistentNotificationId));
}

// Uniqueness of notification ids will be impacted by the fact whether the
// browser context is in Incognito mode.
TEST_F(NotificationIdGeneratorTest, DifferentIncognitoStates) {
  ASSERT_FALSE(browser_context()->IsOffTheRecord());

  // Persistent notifications.
  std::string normal_persistent_notification_id =
      generator()->GenerateForPersistentNotification(origin(), kExampleTag,
                                                     kPersistentNotificationId);

  browser_context()->set_incognito(true);
  ASSERT_TRUE(browser_context()->IsOffTheRecord());

  std::string incognito_persistent_notification_id =
      generator()->GenerateForPersistentNotification(origin(), kExampleTag,
                                                     kPersistentNotificationId);

  EXPECT_NE(normal_persistent_notification_id,
            incognito_persistent_notification_id);

  browser_context()->set_incognito(false);

  // Non-persistent notifications.
  ASSERT_FALSE(browser_context()->IsOffTheRecord());

  std::string normal_non_persistent_notification_id =
      generator()->GenerateForNonPersistentNotification(
          origin(), kExampleTag, kNonPersistentNotificationId);

  browser_context()->set_incognito(true);
  ASSERT_TRUE(browser_context()->IsOffTheRecord());

  std::string incognito_non_persistent_notification_id =
      generator()->GenerateForNonPersistentNotification(
          origin(), kExampleTag, kNonPersistentNotificationId);

  EXPECT_NE(normal_non_persistent_notification_id,
            incognito_non_persistent_notification_id);
}

// The origin of the notification will impact the generated notification id.
TEST_F(NotificationIdGeneratorTest, DifferentOrigins) {
  GURL different_origin("https://example2.com");

  // Persistent notifications.
  EXPECT_NE(generator()->GenerateForPersistentNotification(
                origin(), kExampleTag, kPersistentNotificationId),
            generator()->GenerateForPersistentNotification(
                different_origin, kExampleTag, kPersistentNotificationId));

  // Non-persistent notifications.
  EXPECT_NE(generator()->GenerateForNonPersistentNotification(
                origin(), kExampleTag, kNonPersistentNotificationId),
            generator()->GenerateForNonPersistentNotification(
                different_origin, kExampleTag, kNonPersistentNotificationId));
}

// The tag, when non-empty, will impact the generated notification id.
TEST_F(NotificationIdGeneratorTest, DifferentTags) {
  const std::string& different_tag = std::string(kExampleTag) + "2";

  // Persistent notifications.
  EXPECT_NE(generator()->GenerateForPersistentNotification(
                origin(), kExampleTag, kPersistentNotificationId),
            generator()->GenerateForPersistentNotification(
                origin(), different_tag, kPersistentNotificationId));

  // Non-persistent notifications.
  EXPECT_NE(generator()->GenerateForNonPersistentNotification(
                origin(), kExampleTag, kNonPersistentNotificationId),
            generator()->GenerateForNonPersistentNotification(
                origin(), different_tag, kNonPersistentNotificationId));
}

// The persistent or non-persistent notification id will impact the generated
// notification id when the tag is empty.
TEST_F(NotificationIdGeneratorTest, DifferentIds) {
  NotificationIdGenerator second_generator(browser_context(),
                                           kRenderProcessId + 1);

  // Persistent notifications.
  EXPECT_NE(generator()->GenerateForPersistentNotification(
                origin(), "" /* tag */, kPersistentNotificationId),
            generator()->GenerateForPersistentNotification(
                origin(), "" /* tag */, kPersistentNotificationId + 1));

  // Non-persistent notifications.
  EXPECT_NE(generator()->GenerateForNonPersistentNotification(
                origin(), "" /* tag */, kNonPersistentNotificationId),
            generator()->GenerateForNonPersistentNotification(
                origin(), "" /* tag */, kNonPersistentNotificationId + 1));

  // Non-persistent when a tag is being used.
  EXPECT_EQ(generator()->GenerateForNonPersistentNotification(
                origin(), kExampleTag, kNonPersistentNotificationId),
            second_generator.GenerateForNonPersistentNotification(
                origin(), kExampleTag, kNonPersistentNotificationId));
}

// Using a numeric tag that could resemble a persistent notification id should
// not be equal to a notification without a tag, but with that id.
TEST_F(NotificationIdGeneratorTest, NumericTagAmbiguity) {
  // Persistent notifications.
  EXPECT_NE(generator()->GenerateForPersistentNotification(
                origin(), base::Int64ToString(kPersistentNotificationId),
                kPersistentNotificationId),
            generator()->GenerateForPersistentNotification(
                origin(), "" /* tag */, kPersistentNotificationId));

  // Non-persistent notifications.
  EXPECT_NE(generator()->GenerateForNonPersistentNotification(
                origin(), base::IntToString(kNonPersistentNotificationId),
                kNonPersistentNotificationId),
            generator()->GenerateForNonPersistentNotification(
                origin(), "" /* tag */, kNonPersistentNotificationId));
}

// Using port numbers and a tag which, when concatenated, could end up being
// equal to each other if origins stop ending with slashes.
TEST_F(NotificationIdGeneratorTest, OriginPortAmbiguity) {
  GURL origin_805("https://example.com:805");
  GURL origin_8051("https://example.com:8051");

  // Persistent notifications.
  EXPECT_NE(generator()->GenerateForPersistentNotification(
                origin_805, "17", kPersistentNotificationId),
            generator()->GenerateForPersistentNotification(
                origin_8051, "7", kPersistentNotificationId));

  // Non-persistent notifications.
  EXPECT_NE(generator()->GenerateForNonPersistentNotification(
                origin_805, "17", kNonPersistentNotificationId),
            generator()->GenerateForNonPersistentNotification(
                origin_8051, "7", kNonPersistentNotificationId));
}

// Verifies that the static Is(Non)PersistentNotification helpers can correctly
// identify persistent and non-persistent notification IDs.
TEST_F(NotificationIdGeneratorTest, DetectIdFormat) {
  std::string persistent_id = generator()->GenerateForPersistentNotification(
      origin(), "" /* tag */, kPersistentNotificationId);

  std::string non_persistent_id =
      generator()->GenerateForNonPersistentNotification(
          origin(), "" /* tag */, kNonPersistentNotificationId);

  EXPECT_TRUE(NotificationIdGenerator::IsPersistentNotification(persistent_id));
  EXPECT_FALSE(
      NotificationIdGenerator::IsNonPersistentNotification(persistent_id));

  EXPECT_TRUE(
      NotificationIdGenerator::IsNonPersistentNotification(non_persistent_id));
  EXPECT_FALSE(
      NotificationIdGenerator::IsPersistentNotification(non_persistent_id));
}

// -----------------------------------------------------------------------------
// Persistent notifications
//
// Tests covering the logic specific to persistent notifications. This kind of
// notification does not care about the renderer process that created them.

TEST_F(NotificationIdGeneratorTest, PersistentDifferentRenderProcessIds) {
  NotificationIdGenerator second_generator(browser_context(),
                                           kRenderProcessId + 1);

  EXPECT_EQ(generator()->GenerateForPersistentNotification(
                origin(), kExampleTag, kPersistentNotificationId),
            second_generator.GenerateForPersistentNotification(
                origin(), kExampleTag, kPersistentNotificationId));

  EXPECT_EQ(generator()->GenerateForPersistentNotification(
                origin(), "" /* tag */, kPersistentNotificationId),
            second_generator.GenerateForPersistentNotification(
                origin(), "" /* tag */, kPersistentNotificationId));
}

// -----------------------------------------------------------------------------
// Non-persistent notifications
//
// Tests covering the logic specific to non-persistent notifications. This kind
// of notification cares about the renderer process they were created by when
// the notification does not have a tag, since multiple renderers would restart
// the count for non-persistent notification ids.

TEST_F(NotificationIdGeneratorTest, NonPersistentDifferentRenderProcessIds) {
  NotificationIdGenerator second_generator(browser_context(),
                                           kRenderProcessId + 1);

  EXPECT_EQ(generator()->GenerateForNonPersistentNotification(
                origin(), kExampleTag, kNonPersistentNotificationId),
            second_generator.GenerateForNonPersistentNotification(
                origin(), kExampleTag, kNonPersistentNotificationId));

  EXPECT_NE(generator()->GenerateForNonPersistentNotification(
                origin(), "" /* tag */, kNonPersistentNotificationId),
            second_generator.GenerateForNonPersistentNotification(
                origin(), "" /* tag */, kNonPersistentNotificationId));
}

// Concatenation of the render process id and the non-persistent notification
// id should not result in the generation of a duplicated notification id.
TEST_F(NotificationIdGeneratorTest, NonPersistentRenderProcessIdAmbiguity) {
  NotificationIdGenerator generator_rpi_5(browser_context(), 5);
  NotificationIdGenerator generator_rpi_51(browser_context(), 51);

  EXPECT_NE(
      generator_rpi_5.GenerateForNonPersistentNotification(
          origin(), "" /* tag */, 1337 /* non_persistent_notification_id */),
      generator_rpi_51.GenerateForNonPersistentNotification(
          origin(), "" /* tag */, 337 /* non_persistent_notification_id */));
}

}  // namespace
}  // namespace content
