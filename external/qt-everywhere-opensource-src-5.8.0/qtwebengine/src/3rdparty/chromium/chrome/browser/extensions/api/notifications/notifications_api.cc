// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/notifications/notifications_api.h"

#include <stddef.h>

#include <utility>

#include "base/callback.h"
#include "base/guid.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_conversion_helper.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/notifications/notifier_state_tracker.h"
#include "chrome/browser/notifications/notifier_state_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/notifications/notification_style.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/layout.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/notifier_settings.h"
#include "url/gurl.h"

namespace extensions {

namespace notifications = api::notifications;

namespace {

const char kMissingRequiredPropertiesForCreateNotification[] =
    "Some of the required properties are missing: type, iconUrl, title and "
    "message.";
const char kUnableToDecodeIconError[] =
    "Unable to successfully use the provided image.";
const char kUnexpectedProgressValueForNonProgressType[] =
    "The progress value should not be specified for non-progress notification";
const char kInvalidProgressValue[] =
    "The progress value should range from 0 to 100";
const char kExtraListItemsProvided[] =
    "List items provided for notification type != list";
const char kExtraImageProvided[] =
    "Image resource provided for notification type != image";

#if !defined(OS_CHROMEOS)
const char kLowPriorityDeprecatedOnPlatform[] =
    "Low-priority notifications are deprecated on this platform.";
#endif

// Given an extension id and another id, returns an id that is unique
// relative to other extensions.
std::string CreateScopedIdentifier(const std::string& extension_id,
                                   const std::string& id) {
  return extension_id + "-" + id;
}

// Removes the unique internal identifier to send the ID as the
// extension expects it.
std::string StripScopeFromIdentifier(const std::string& extension_id,
                                     const std::string& id) {
  size_t index_of_separator = extension_id.length() + 1;
  DCHECK_LT(index_of_separator, id.length());

  return id.substr(index_of_separator);
}

const gfx::ImageSkia CreateSolidColorImage(int width,
                                           int height,
                                           SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(color);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

// Take the alpha channel of small_image, mask it with the foreground,
// then add the masked foreground on top of the background
const gfx::Image GetMaskedSmallImage(const gfx::ImageSkia& small_image) {
  int width = small_image.width();
  int height = small_image.height();

  // Background color grey
  const gfx::ImageSkia background = CreateSolidColorImage(
      width, height, message_center::kSmallImageMaskBackgroundColor);
  // Foreground color white
  const gfx::ImageSkia foreground = CreateSolidColorImage(
      width, height, message_center::kSmallImageMaskForegroundColor);
  const gfx::ImageSkia masked_small_image =
      gfx::ImageSkiaOperations::CreateMaskedImage(foreground, small_image);
  return gfx::Image(gfx::ImageSkiaOperations::CreateSuperimposedImage(
      background, masked_small_image));
}

class ShutdownNotifierFactory
    : public BrowserContextKeyedServiceShutdownNotifierFactory {
 public:
  static ShutdownNotifierFactory* GetInstance() {
    return base::Singleton<ShutdownNotifierFactory>::get();
  }

 private:
  friend struct base::DefaultSingletonTraits<ShutdownNotifierFactory>;

  ShutdownNotifierFactory()
      : BrowserContextKeyedServiceShutdownNotifierFactory(
            "NotificationsApiDelegate") {
    DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  }
  ~ShutdownNotifierFactory() override {}

  DISALLOW_COPY_AND_ASSIGN(ShutdownNotifierFactory);
};

class NotificationsApiDelegate : public NotificationDelegate {
 public:
  NotificationsApiDelegate(ChromeAsyncExtensionFunction* api_function,
                           Profile* profile,
                           const std::string& extension_id,
                           const std::string& id)
      : api_function_(api_function),
        event_router_(EventRouter::Get(profile)),
        extension_id_(extension_id),
        id_(id),
        scoped_id_(CreateScopedIdentifier(extension_id, id)) {
    DCHECK(api_function_);
    shutdown_notifier_subscription_ =
        ShutdownNotifierFactory::GetInstance()->Get(profile)->Subscribe(
            base::Bind(&NotificationsApiDelegate::Shutdown,
                       base::Unretained(this)));
  }

  void Close(bool by_user) override {
    EventRouter::UserGestureState gesture =
        by_user ? EventRouter::USER_GESTURE_ENABLED
                : EventRouter::USER_GESTURE_NOT_ENABLED;
    std::unique_ptr<base::ListValue> args(CreateBaseEventArgs());
    args->AppendBoolean(by_user);
    SendEvent(events::NOTIFICATIONS_ON_CLOSED,
              notifications::OnClosed::kEventName, gesture, std::move(args));
  }

  void Click() override {
    std::unique_ptr<base::ListValue> args(CreateBaseEventArgs());
    SendEvent(events::NOTIFICATIONS_ON_CLICKED,
              notifications::OnClicked::kEventName,
              EventRouter::USER_GESTURE_ENABLED, std::move(args));
  }

  bool HasClickedListener() override {
    if (!event_router_)
      return false;

    return event_router_->HasEventListener(
        notifications::OnClicked::kEventName);
  }

  void ButtonClick(int index) override {
    std::unique_ptr<base::ListValue> args(CreateBaseEventArgs());
    args->AppendInteger(index);
    SendEvent(events::NOTIFICATIONS_ON_BUTTON_CLICKED,
              notifications::OnButtonClicked::kEventName,
              EventRouter::USER_GESTURE_ENABLED, std::move(args));
  }

  std::string id() const override { return scoped_id_; }

 private:
  ~NotificationsApiDelegate() override {}

  void SendEvent(events::HistogramValue histogram_value,
                 const std::string& name,
                 EventRouter::UserGestureState user_gesture,
                 std::unique_ptr<base::ListValue> args) {
    if (!event_router_)
      return;

    std::unique_ptr<Event> event(
        new Event(histogram_value, name, std::move(args)));
    event->user_gesture = user_gesture;
    event_router_->DispatchEventToExtension(extension_id_, std::move(event));
  }

  void Shutdown() {
    event_router_ = nullptr;
    shutdown_notifier_subscription_.reset();
  }

  std::unique_ptr<base::ListValue> CreateBaseEventArgs() {
    std::unique_ptr<base::ListValue> args(new base::ListValue());
    args->AppendString(id_);
    return args;
  }

  scoped_refptr<ChromeAsyncExtensionFunction> api_function_;

  // Since this class is refcounted it may outlive the profile.  We listen for
  // profile-keyed service shutdown events and reset to nullptr at that time,
  // so make sure to check for a valid pointer before use.
  EventRouter* event_router_;

  const std::string extension_id_;
  const std::string id_;
  const std::string scoped_id_;

  std::unique_ptr<KeyedServiceShutdownNotifier::Subscription>
      shutdown_notifier_subscription_;

  DISALLOW_COPY_AND_ASSIGN(NotificationsApiDelegate);
};

}  // namespace

bool NotificationsApiFunction::IsNotificationsApiAvailable() {
  // We need to check this explicitly rather than letting
  // _permission_features.json enforce it, because we're sharing the
  // chrome.notifications permissions namespace with WebKit notifications.
  return extension()->is_platform_app() || extension()->is_extension();
}

NotificationsApiFunction::NotificationsApiFunction() {
}

NotificationsApiFunction::~NotificationsApiFunction() {
}

bool NotificationsApiFunction::CreateNotification(
    const std::string& id,
    api::notifications::NotificationOptions* options) {
  // First, make sure the required fields exist: type, title, message, icon.
  // These fields are defined as optional in IDL such that they can be used as
  // optional for notification updates. But for notification creations, they
  // should be present.
  if (options->type == api::notifications::TEMPLATE_TYPE_NONE ||
      !options->icon_url || !options->title || !options->message) {
    SetError(kMissingRequiredPropertiesForCreateNotification);
    return false;
  }

#if !defined(OS_CHROMEOS)
  if (options->priority &&
      *options->priority < message_center::DEFAULT_PRIORITY) {
    SetError(kLowPriorityDeprecatedOnPlatform);
    return false;
  }
#endif

  NotificationBitmapSizes bitmap_sizes = GetNotificationBitmapSizes();

  float image_scale =
      ui::GetScaleForScaleFactor(ui::GetSupportedScaleFactors().back());

  // Extract required fields: type, title, message, and icon.
  message_center::NotificationType type =
      MapApiTemplateTypeToType(options->type);
  UMA_HISTOGRAM_ENUMERATION("Notifications.ExtensionNotificationType", type,
                            message_center::NOTIFICATION_TYPE_LAST);

  const base::string16 title(base::UTF8ToUTF16(*options->title));
  const base::string16 message(base::UTF8ToUTF16(*options->message));
  gfx::Image icon;

  if (!options->icon_bitmap.get() ||
      !NotificationConversionHelper::NotificationBitmapToGfxImage(
          image_scale, bitmap_sizes.icon_size, *options->icon_bitmap, &icon)) {
    SetError(kUnableToDecodeIconError);
    return false;
  }

  // Then, handle any optional data that's been provided.
  message_center::RichNotificationData optional_fields;
  if (options->app_icon_mask_url.get()) {
    gfx::Image small_icon_mask;
    if (!NotificationConversionHelper::NotificationBitmapToGfxImage(
            image_scale, bitmap_sizes.app_icon_mask_size,
            *options->app_icon_mask_bitmap, &small_icon_mask)) {
      SetError(kUnableToDecodeIconError);
      return false;
    }
    optional_fields.small_image =
        GetMaskedSmallImage(small_icon_mask.AsImageSkia());
  }

  if (options->priority.get())
    optional_fields.priority = *options->priority;

  if (options->event_time.get())
    optional_fields.timestamp = base::Time::FromJsTime(*options->event_time);

  if (options->buttons.get()) {
    // Currently we allow up to 2 buttons.
    size_t number_of_buttons = options->buttons->size();

    // Use distinct buckets for 1-16 notification action buttons, and an
    // overflow bucket for 17 or more action buttons. Does not impact how many
    // action buttons are shown.
    UMA_HISTOGRAM_ENUMERATION("Notifications.ExtensionNotificationActionCount",
                              number_of_buttons, 17);

    number_of_buttons = number_of_buttons > 2 ? 2 : number_of_buttons;

    for (size_t i = 0; i < number_of_buttons; i++) {
      message_center::ButtonInfo info(
          base::UTF8ToUTF16((*options->buttons)[i].title));
      extensions::api::notifications::NotificationBitmap* icon_bitmap_ptr =
          (*options->buttons)[i].icon_bitmap.get();
      if (icon_bitmap_ptr) {
        NotificationConversionHelper::NotificationBitmapToGfxImage(
            image_scale, bitmap_sizes.button_icon_size, *icon_bitmap_ptr,
            &info.icon);
      }
      optional_fields.buttons.push_back(info);
    }
  }

  if (options->context_message) {
    optional_fields.context_message =
        base::UTF8ToUTF16(*options->context_message);
  }

  bool has_image = options->image_bitmap.get() &&
                   NotificationConversionHelper::NotificationBitmapToGfxImage(
                       image_scale, bitmap_sizes.image_size,
                       *options->image_bitmap, &optional_fields.image);

  // We should have an image if and only if the type is an image type.
  if (has_image != (type == message_center::NOTIFICATION_TYPE_IMAGE)) {
    SetError(kExtraImageProvided);
    return false;
  }

  // We should have list items if and only if the type is a multiple type.
  bool has_list_items = options->items.get() && options->items->size() > 0;
  if (has_list_items != (type == message_center::NOTIFICATION_TYPE_MULTIPLE)) {
    SetError(kExtraListItemsProvided);
    return false;
  }

  if (options->progress.get() != NULL) {
    // We should have progress if and only if the type is a progress type.
    if (type != message_center::NOTIFICATION_TYPE_PROGRESS) {
      SetError(kUnexpectedProgressValueForNonProgressType);
      return false;
    }
    optional_fields.progress = *options->progress;
    // Progress value should range from 0 to 100.
    if (optional_fields.progress < 0 || optional_fields.progress > 100) {
      SetError(kInvalidProgressValue);
      return false;
    }
  }

  if (has_list_items) {
    using api::notifications::NotificationItem;
    for (const NotificationItem& api_item : *options->items) {
      optional_fields.items.push_back(message_center::NotificationItem(
          base::UTF8ToUTF16(api_item.title),
          base::UTF8ToUTF16(api_item.message)));
    }
  }

  if (options->is_clickable.get())
    optional_fields.clickable = *options->is_clickable;

  NotificationsApiDelegate* api_delegate(new NotificationsApiDelegate(
      this, GetProfile(), extension_->id(), id));  // ownership is passed to
                                                   // Notification
  Notification notification(
      type, title, message, icon,
      message_center::NotifierId(message_center::NotifierId::APPLICATION,
                                 extension_->id()),
      base::UTF8ToUTF16(extension_->name()), extension_->url(),
      api_delegate->id(), optional_fields, api_delegate);

  // Apply the "requireInteraction" flag. The value defaults to false.
  notification.set_never_timeout(options->require_interaction &&
                                 *options->require_interaction);

  g_browser_process->notification_ui_manager()->Add(notification, GetProfile());
  return true;
}

bool NotificationsApiFunction::UpdateNotification(
    const std::string& id,
    api::notifications::NotificationOptions* options,
    Notification* notification) {
#if !defined(OS_CHROMEOS)
  if (options->priority &&
      *options->priority < message_center::DEFAULT_PRIORITY) {
    SetError(kLowPriorityDeprecatedOnPlatform);
    return false;
  }
#endif

  NotificationBitmapSizes bitmap_sizes = GetNotificationBitmapSizes();
  float image_scale =
      ui::GetScaleForScaleFactor(ui::GetSupportedScaleFactors().back());

  // Update optional fields if provided.
  if (options->type != api::notifications::TEMPLATE_TYPE_NONE)
    notification->set_type(MapApiTemplateTypeToType(options->type));
  if (options->title)
    notification->set_title(base::UTF8ToUTF16(*options->title));
  if (options->message)
    notification->set_message(base::UTF8ToUTF16(*options->message));

  if (options->icon_bitmap.get()) {
    gfx::Image icon;
    if (!NotificationConversionHelper::NotificationBitmapToGfxImage(
            image_scale, bitmap_sizes.icon_size, *options->icon_bitmap,
            &icon)) {
      SetError(kUnableToDecodeIconError);
      return false;
    }
    notification->set_icon(icon);
  }

  if (options->app_icon_mask_bitmap.get()) {
    gfx::Image app_icon_mask;
    if (!NotificationConversionHelper::NotificationBitmapToGfxImage(
            image_scale, bitmap_sizes.app_icon_mask_size,
            *options->app_icon_mask_bitmap, &app_icon_mask)) {
      SetError(kUnableToDecodeIconError);
      return false;
    }
    notification->set_small_image(
        GetMaskedSmallImage(app_icon_mask.AsImageSkia()));
  }

  if (options->priority)
    notification->set_priority(*options->priority);

  if (options->event_time)
    notification->set_timestamp(base::Time::FromJsTime(*options->event_time));

  if (options->buttons) {
    // Currently we allow up to 2 buttons.
    size_t number_of_buttons = options->buttons->size();
    number_of_buttons = number_of_buttons > 2 ? 2 : number_of_buttons;

    std::vector<message_center::ButtonInfo> buttons;
    for (size_t i = 0; i < number_of_buttons; i++) {
      message_center::ButtonInfo button(
          base::UTF8ToUTF16((*options->buttons)[i].title));
      extensions::api::notifications::NotificationBitmap* icon_bitmap_ptr =
          (*options->buttons)[i].icon_bitmap.get();
      if (icon_bitmap_ptr) {
        NotificationConversionHelper::NotificationBitmapToGfxImage(
            image_scale, bitmap_sizes.button_icon_size, *icon_bitmap_ptr,
            &button.icon);
      }
      buttons.push_back(button);
    }
    notification->set_buttons(buttons);
  }

  if (options->context_message) {
    notification->set_context_message(
        base::UTF8ToUTF16(*options->context_message));
  }

  gfx::Image image;
  bool has_image =
      options->image_bitmap.get() &&
      NotificationConversionHelper::NotificationBitmapToGfxImage(
          image_scale, bitmap_sizes.image_size, *options->image_bitmap, &image);

  if (has_image) {
    // We should have an image if and only if the type is an image type.
    if (notification->type() != message_center::NOTIFICATION_TYPE_IMAGE) {
      SetError(kExtraImageProvided);
      return false;
    }
    notification->set_image(image);
  }

  if (options->progress) {
    // We should have progress if and only if the type is a progress type.
    if (notification->type() != message_center::NOTIFICATION_TYPE_PROGRESS) {
      SetError(kUnexpectedProgressValueForNonProgressType);
      return false;
    }
    int progress = *options->progress;
    // Progress value should range from 0 to 100.
    if (progress < 0 || progress > 100) {
      SetError(kInvalidProgressValue);
      return false;
    }
    notification->set_progress(progress);
  }

  if (options->items.get() && options->items->size() > 0) {
    // We should have list items if and only if the type is a multiple type.
    if (notification->type() != message_center::NOTIFICATION_TYPE_MULTIPLE) {
      SetError(kExtraListItemsProvided);
      return false;
    }

    std::vector<message_center::NotificationItem> items;
    using api::notifications::NotificationItem;
    for (const NotificationItem& api_item : *options->items) {
      items.push_back(message_center::NotificationItem(
          base::UTF8ToUTF16(api_item.title),
          base::UTF8ToUTF16(api_item.message)));
    }
    notification->set_items(items);
  }

  // Then override if it's already set.
  if (options->is_clickable.get())
    notification->set_clickable(*options->is_clickable);

  g_browser_process->notification_ui_manager()->Update(*notification,
                                                       GetProfile());
  return true;
}

bool NotificationsApiFunction::AreExtensionNotificationsAllowed() const {
  NotifierStateTracker* notifier_state_tracker =
      NotifierStateTrackerFactory::GetForProfile(GetProfile());

  return notifier_state_tracker->IsNotifierEnabled(
      message_center::NotifierId(message_center::NotifierId::APPLICATION,
                                 extension_->id()));
}

bool NotificationsApiFunction::IsNotificationsApiEnabled() const {
  return CanRunWhileDisabled() || AreExtensionNotificationsAllowed();
}

bool NotificationsApiFunction::CanRunWhileDisabled() const {
  return false;
}

bool NotificationsApiFunction::RunAsync() {
  if (IsNotificationsApiAvailable() && IsNotificationsApiEnabled()) {
    return RunNotificationsApi();
  } else {
    SendResponse(false);
    return true;
  }
}

message_center::NotificationType
NotificationsApiFunction::MapApiTemplateTypeToType(
    api::notifications::TemplateType type) {
  switch (type) {
    case api::notifications::TEMPLATE_TYPE_NONE:
    case api::notifications::TEMPLATE_TYPE_BASIC:
      return message_center::NOTIFICATION_TYPE_BASE_FORMAT;
    case api::notifications::TEMPLATE_TYPE_IMAGE:
      return message_center::NOTIFICATION_TYPE_IMAGE;
    case api::notifications::TEMPLATE_TYPE_LIST:
      return message_center::NOTIFICATION_TYPE_MULTIPLE;
    case api::notifications::TEMPLATE_TYPE_PROGRESS:
      return message_center::NOTIFICATION_TYPE_PROGRESS;
    default:
      // Gracefully handle newer application code that is running on an older
      // runtime that doesn't recognize the requested template.
      return message_center::NOTIFICATION_TYPE_BASE_FORMAT;
  }
}

NotificationsCreateFunction::NotificationsCreateFunction() {
}

NotificationsCreateFunction::~NotificationsCreateFunction() {
}

bool NotificationsCreateFunction::RunNotificationsApi() {
  params_ = api::notifications::Create::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  const std::string extension_id(extension_->id());
  std::string notification_id;
  if (params_->notification_id.get() && !params_->notification_id->empty()) {
    // If the caller provided a notificationId, use that.
    notification_id = *params_->notification_id;
  } else {
    // Otherwise, use a randomly created GUID. In case that GenerateGUID returns
    // the empty string, simply generate a random string.
    notification_id = base::GenerateGUID();
    if (notification_id.empty())
      notification_id = base::RandBytesAsString(16);
  }

  SetResult(base::MakeUnique<base::StringValue>(notification_id));

  // TODO(dewittj): Add more human-readable error strings if this fails.
  if (!CreateNotification(notification_id, &params_->options))
    return false;

  SendResponse(true);

  return true;
}

NotificationsUpdateFunction::NotificationsUpdateFunction() {
}

NotificationsUpdateFunction::~NotificationsUpdateFunction() {
}

bool NotificationsUpdateFunction::RunNotificationsApi() {
  params_ = api::notifications::Update::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  // We are in update.  If the ID doesn't exist, succeed but call the callback
  // with "false".
  const Notification* matched_notification =
      g_browser_process->notification_ui_manager()->FindById(
          CreateScopedIdentifier(extension_->id(), params_->notification_id),
          NotificationUIManager::GetProfileID(GetProfile()));
  if (!matched_notification) {
    SetResult(base::MakeUnique<base::FundamentalValue>(false));
    SendResponse(true);
    return true;
  }

  // Copy the existing notification to get a writable version of it.
  Notification notification = *matched_notification;

  // If we have trouble updating the notification (could be improper use of API
  // or some other reason), mark the function as failed, calling the callback
  // with false.
  // TODO(dewittj): Add more human-readable error strings if this fails.
  bool could_update_notification = UpdateNotification(
      params_->notification_id, &params_->options, &notification);
  SetResult(
      base::MakeUnique<base::FundamentalValue>(could_update_notification));
  if (!could_update_notification)
    return false;

  // No trouble, created the notification, send true to the callback and
  // succeed.
  SendResponse(true);
  return true;
}

NotificationsClearFunction::NotificationsClearFunction() {
}

NotificationsClearFunction::~NotificationsClearFunction() {
}

bool NotificationsClearFunction::RunNotificationsApi() {
  params_ = api::notifications::Clear::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  bool cancel_result = g_browser_process->notification_ui_manager()->CancelById(
      CreateScopedIdentifier(extension_->id(), params_->notification_id),
      NotificationUIManager::GetProfileID(GetProfile()));

  SetResult(base::MakeUnique<base::FundamentalValue>(cancel_result));
  SendResponse(true);

  return true;
}

NotificationsGetAllFunction::NotificationsGetAllFunction() {}

NotificationsGetAllFunction::~NotificationsGetAllFunction() {}

bool NotificationsGetAllFunction::RunNotificationsApi() {
  NotificationUIManager* notification_ui_manager =
      g_browser_process->notification_ui_manager();
  std::set<std::string> notification_ids =
      notification_ui_manager->GetAllIdsByProfileAndSourceOrigin(
          NotificationUIManager::GetProfileID(GetProfile()), extension_->url());

  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue());

  for (std::set<std::string>::iterator iter = notification_ids.begin();
       iter != notification_ids.end(); iter++) {
    result->SetBooleanWithoutPathExpansion(
        StripScopeFromIdentifier(extension_->id(), *iter), true);
  }

  SetResult(std::move(result));
  SendResponse(true);

  return true;
}

NotificationsGetPermissionLevelFunction::
NotificationsGetPermissionLevelFunction() {}

NotificationsGetPermissionLevelFunction::
~NotificationsGetPermissionLevelFunction() {}

bool NotificationsGetPermissionLevelFunction::CanRunWhileDisabled() const {
  return true;
}

bool NotificationsGetPermissionLevelFunction::RunNotificationsApi() {
  api::notifications::PermissionLevel result =
      AreExtensionNotificationsAllowed()
          ? api::notifications::PERMISSION_LEVEL_GRANTED
          : api::notifications::PERMISSION_LEVEL_DENIED;

  SetResult(base::MakeUnique<base::StringValue>(
      api::notifications::ToString(result)));
  SendResponse(true);

  return true;
}

}  // namespace extensions
