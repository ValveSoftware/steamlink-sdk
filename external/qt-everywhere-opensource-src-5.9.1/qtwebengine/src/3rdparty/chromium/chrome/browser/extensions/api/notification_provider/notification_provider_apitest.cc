// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/notification_provider/notification_provider_api.h"

#include "chrome/browser/extensions/extension_apitest.h"

typedef ExtensionApiTest NotificationProviderApiTest;

IN_PROC_BROWSER_TEST_F(NotificationProviderApiTest, Events) {
  ASSERT_TRUE(RunExtensionTest("notification_provider/events")) << message_;
}

IN_PROC_BROWSER_TEST_F(NotificationProviderApiTest, TestBasicUsage) {
  ASSERT_TRUE(RunExtensionTest("notification_provider/basic_usage"))
      << message_;
}
