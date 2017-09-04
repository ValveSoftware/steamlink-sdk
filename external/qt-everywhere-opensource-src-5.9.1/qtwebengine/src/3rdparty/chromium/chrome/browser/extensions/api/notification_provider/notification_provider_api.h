// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_NOTIFICATION_PROVIDER_NOTIFICATION_PROVIDER_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_NOTIFICATION_PROVIDER_NOTIFICATION_PROVIDER_API_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/notification_provider.h"
#include "extensions/browser/extension_function.h"
#include "ui/message_center/notification_types.h"

namespace extensions {

// Send events to the client. This will send events onCreated, onUpdated and
// onCleared to extensions/apps using this API.
class NotificationProviderEventRouter {
 public:
  explicit NotificationProviderEventRouter(Profile* profile);
  virtual ~NotificationProviderEventRouter();

  void CreateNotification(
      const std::string& notification_provider_id,
      const std::string& sender_id,
      const std::string& notification_id,
      const api::notifications::NotificationOptions& options);
  void UpdateNotification(
      const std::string& notification_provider_id,
      const std::string& sender_id,
      const std::string& notificaiton_id,
      const api::notifications::NotificationOptions& options);
  void ClearNotification(const std::string& notification_provider_id,
                         const std::string& sender_id,
                         const std::string& notification_id);

 private:
  void Create(const std::string& notification_provider_id,
              const std::string& sender_id,
              const std::string& notification_id,
              const api::notifications::NotificationOptions& options);
  void Update(const std::string& notification_provider_id,
              const std::string& sender_id,
              const std::string& notification_id,
              const api::notifications::NotificationOptions& options);
  void Clear(const std::string& notification_provider_id,
             const std::string& sender_id,
             const std::string& notification_id);

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(NotificationProviderEventRouter);
};

// Implememtation of NotifyOnCleared function of the API. It will inform the
// notifier that the user cleared a notification sent from that notifier.
class NotificationProviderNotifyOnClearedFunction
    : public ChromeUIThreadExtensionFunction {
 public:
  NotificationProviderNotifyOnClearedFunction();

 protected:
  ~NotificationProviderNotifyOnClearedFunction() override;

 private:
  DECLARE_EXTENSION_FUNCTION("notificationProvider.notifyOnCleared",
                             NOTIFICATIONPROVIDER_NOTIFYONCLEARED);

  // UIThreadExtensionFunction implementation.
  ExtensionFunction::ResponseAction Run() override;
};

// Implememtation of NotifyOnClicked function of the API. It will inform the
// notifier that the user clicked in a non-button area of a notification sent
// from that notifier.
class NotificationProviderNotifyOnClickedFunction
    : public ChromeUIThreadExtensionFunction {
 public:
  NotificationProviderNotifyOnClickedFunction();

 protected:
  ~NotificationProviderNotifyOnClickedFunction() override;

 private:
  DECLARE_EXTENSION_FUNCTION("notificationProvider.notifyOnClicked",
                             NOTIFICATIONPROVIDER_NOTIFYONCLICKED);

  // UIThreadExtensionFunction implementation.
  ExtensionFunction::ResponseAction Run() override;
};

// Implememtation of NotifyOnButtonClicked function of the API. It will inform
// the
// notifier that the user pressed a button in the notification sent from that
// notifier.
class NotificationProviderNotifyOnButtonClickedFunction
    : public ChromeUIThreadExtensionFunction {
 public:
  NotificationProviderNotifyOnButtonClickedFunction();

 protected:
  ~NotificationProviderNotifyOnButtonClickedFunction() override;

 private:
  DECLARE_EXTENSION_FUNCTION("notificationProvider.notifyOnButtonClicked",
                             NOTIFICATIONPROVIDER_NOTIFYONBUTTONCLICKED);

  // UIThreadExtensionFunction implementation.
  ExtensionFunction::ResponseAction Run() override;
};

// Implememtation of NotifyOnPermissionLevelChanged function of the API. It will
// inform the notifier that the user changed the permission level of that
// notifier.
class NotificationProviderNotifyOnPermissionLevelChangedFunction
    : public ChromeUIThreadExtensionFunction {
 public:
  NotificationProviderNotifyOnPermissionLevelChangedFunction();

 protected:
  ~NotificationProviderNotifyOnPermissionLevelChangedFunction() override;

 private:
  DECLARE_EXTENSION_FUNCTION(
      "notificationProvider.notifyOnPermissionLevelChanged",
      NOTIFICATIONPROVIDER_NOTIFYONPERMISSIONLEVELCHANGED);

  // UIThreadExtensionFunction implementation.
  ExtensionFunction::ResponseAction Run() override;
};

// Implememtation of NotifyOnShowSettings function of the API. It will inform
// the notifier that the user clicked on advanced settings of that notifier.
class NotificationProviderNotifyOnShowSettingsFunction
    : public ChromeUIThreadExtensionFunction {
 public:
  NotificationProviderNotifyOnShowSettingsFunction();

 protected:
  ~NotificationProviderNotifyOnShowSettingsFunction() override;

 private:
  DECLARE_EXTENSION_FUNCTION("notificationProvider.notifyOnShowSettings",
                             NOTIFICATIONPROVIDER_NOTIFYONSHOWSETTINGS);

  // UIThreadExtensionFunction implementation.
  ExtensionFunction::ResponseAction Run() override;
};

// Implememtation of GetNotifier function of the API. It will get the notifier
// object that corresponds to the notifier ID.
class NotificationProviderGetNotifierFunction
    : public ChromeUIThreadExtensionFunction {
 public:
  NotificationProviderGetNotifierFunction();

 protected:
  ~NotificationProviderGetNotifierFunction() override;

 private:
  DECLARE_EXTENSION_FUNCTION("notificationProvider.getNotifier",
                             NOTIFICATIONPROVIDER_GETNOTIFIER);

  // UIThreadExtensionFunction implementation.
  ExtensionFunction::ResponseAction Run() override;
};

// Implememtation of GetAllNotifiers function of the API. It will get all the
// notifiers that would send notifications.
class NotificationProviderGetAllNotifiersFunction
    : public ChromeUIThreadExtensionFunction {
 public:
  NotificationProviderGetAllNotifiersFunction();

 protected:
  ~NotificationProviderGetAllNotifiersFunction() override;

 private:
  DECLARE_EXTENSION_FUNCTION("notificationProvider.getAllNotifiers",
                             NOTIFICATIONPROVIDER_GETALLNOTIFIERS);

  // UIThreadExtensionFunction implementation.
  ExtensionFunction::ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_NOTIFICATION_PROVIDER_NOTIFICATION_PROVIDER_API_H_
