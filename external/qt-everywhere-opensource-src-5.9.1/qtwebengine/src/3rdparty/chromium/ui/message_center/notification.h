// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_NOTIFICATION_H_
#define UI_MESSAGE_CENTER_NOTIFICATION_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "base/time/time.h"
#include "base/values.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/message_center_export.h"
#include "ui/message_center/notification_delegate.h"
#include "ui/message_center/notification_types.h"
#include "ui/message_center/notifier_settings.h"
#include "url/gurl.h"

#if !defined(OS_IOS)
#include "mojo/public/cpp/bindings/struct_traits.h"  // nogncheck
#endif

namespace message_center {

#if !defined(OS_IOS)
namespace mojom {
class NotificationDataView;
}
#endif

struct MESSAGE_CENTER_EXPORT NotificationItem {
  base::string16 title;
  base::string16 message;

  NotificationItem(const base::string16& title, const base::string16& message);
};

enum class ButtonType { BUTTON, TEXT };

struct MESSAGE_CENTER_EXPORT ButtonInfo {
  base::string16 title;
  gfx::Image icon;
  ButtonType type = ButtonType::BUTTON;
  base::string16 placeholder;

  explicit ButtonInfo(const base::string16& title);
  ButtonInfo(const ButtonInfo& other);
  ~ButtonInfo();
  ButtonInfo& operator=(const ButtonInfo& other);
};

class MESSAGE_CENTER_EXPORT RichNotificationData {
 public:
  RichNotificationData();
  RichNotificationData(const RichNotificationData& other);
  ~RichNotificationData();

  int priority;
  bool never_timeout;
  base::Time timestamp;
  base::string16 context_message;
  gfx::Image image;
  gfx::Image small_image;
  std::vector<NotificationItem> items;
  int progress;
  std::vector<ButtonInfo> buttons;
  bool should_make_spoken_feedback_for_popup_updates;
  bool clickable;
#if defined(OS_CHROMEOS)
  // Flag if the notification is pinned. If true, the notification is pinned
  // and user can't remove it.
  bool pinned;
#endif  // defined(OS_CHROMEOS)
  std::vector<int> vibration_pattern;
  bool renotify;
  bool silent;
  base::string16 accessible_name;
};

class MESSAGE_CENTER_EXPORT Notification {
 public:
  // Default constructor needed for generated mojom files
  Notification();

  Notification(NotificationType type,
               const std::string& id,
               const base::string16& title,
               const base::string16& message,
               const gfx::Image& icon,
               const base::string16& display_source,
               const GURL& origin_url,
               const NotifierId& notifier_id,
               const RichNotificationData& optional_fields,
               NotificationDelegate* delegate);

  Notification(const std::string& id, const Notification& other);

  Notification(const Notification& other);

  Notification& operator=(const Notification& other);

  virtual ~Notification();

  // Copies the internal on-memory state from |base|, i.e. shown_as_popup,
  // is_read and never_timeout.
  void CopyState(Notification* base);

  NotificationType type() const { return type_; }
  void set_type(NotificationType type) { type_ = type; }

  // Uniquely identifies a notification in the message center. For
  // notification front ends that support multiple profiles, this id should
  // identify a unique profile + frontend_notification_id combination. You can
  // Use this id against the MessageCenter interface but not the
  // NotificationUIManager interface.
  const std::string& id() const { return id_; }

  const base::string16& title() const { return title_; }
  void set_title(const base::string16& title) { title_ = title; }

  const base::string16& message() const { return message_; }
  void set_message(const base::string16& message) { message_ = message; }

  // The origin URL of the script which requested the notification.
  // Can be empty if the notification is requested by an extension or
  // Chrome app.
  const GURL& origin_url() const { return origin_url_; }

  // A display string for the source of the notification.
  const base::string16& display_source() const { return display_source_; }

  const NotifierId& notifier_id() const { return notifier_id_; }

  void set_profile_id(const std::string& profile_id) {
    notifier_id_.profile_id = profile_id;
  }

  // Begin unpacked values from optional_fields.
  int priority() const { return optional_fields_.priority; }
  void set_priority(int priority) { optional_fields_.priority = priority; }

  // This vibration_pattern property currently has no effect on
  // non-Android platforms.
  const std::vector<int>& vibration_pattern() const {
    return optional_fields_.vibration_pattern;
  }
  void set_vibration_pattern(const std::vector<int>& vibration_pattern) {
    optional_fields_.vibration_pattern = vibration_pattern;
  }

  // This property currently only works in platforms that support native
  // notifications.
  // It determines whether the sound and vibration effects should signal
  // if the notification is replacing another notification.
  bool renotify() const { return optional_fields_.renotify; }
  void set_renotify(bool renotify) { optional_fields_.renotify = renotify; }

  // This property currently has no effect on non-Android platforms.
  bool silent() const { return optional_fields_.silent; }
  void set_silent(bool silent) { optional_fields_.silent = silent; }

  base::Time timestamp() const { return optional_fields_.timestamp; }
  void set_timestamp(const base::Time& timestamp) {
    optional_fields_.timestamp = timestamp;
  }

  const base::string16 context_message() const {
    return optional_fields_.context_message;
  }

  void set_context_message(const base::string16& context_message) {
    optional_fields_.context_message = context_message;
  }

  // Decides if the notification origin should be used as a context message
  bool UseOriginAsContextMessage() const;

  const std::vector<NotificationItem>& items() const {
    return optional_fields_.items;
  }
  void set_items(const std::vector<NotificationItem>& items) {
    optional_fields_.items = items;
  }

  int progress() const { return optional_fields_.progress; }
  void set_progress(int progress) { optional_fields_.progress = progress; }
  // End unpacked values.

  // Images fetched asynchronously.
  const gfx::Image& icon() const { return icon_; }
  void set_icon(const gfx::Image& icon) { icon_ = icon; }

  const gfx::Image& image() const { return optional_fields_.image; }
  void set_image(const gfx::Image& image) { optional_fields_.image = image; }

  const gfx::Image& small_image() const { return optional_fields_.small_image; }
  void set_small_image(const gfx::Image& image) {
    optional_fields_.small_image = image;
  }

  // Buttons, with icons fetched asynchronously.
  const std::vector<ButtonInfo>& buttons() const {
    return optional_fields_.buttons;
  }
  void set_buttons(const std::vector<ButtonInfo>& buttons) {
    optional_fields_.buttons = buttons;
  }
  void SetButtonIcon(size_t index, const gfx::Image& icon);

  bool shown_as_popup() const { return shown_as_popup_; }
  void set_shown_as_popup(bool shown_as_popup) {
    shown_as_popup_ = shown_as_popup;
  }

  // Read status in the message center.
  bool IsRead() const;
  void set_is_read(bool read) { is_read_ = read; }

  // Used to keep the order of notifications with the same timestamp.
  // The notification with lesser serial_number is considered 'older'.
  unsigned serial_number() { return serial_number_; }

  // Gets and sets whether the notifiction should remain onscreen permanently.
  bool never_timeout() const { return optional_fields_.never_timeout; }
  void set_never_timeout(bool never_timeout) {
    optional_fields_.never_timeout = never_timeout;
  }

  bool clickable() const { return optional_fields_.clickable; }
  void set_clickable(bool clickable) {
    optional_fields_.clickable = clickable;
  }

  bool pinned() const {
#if defined(OS_CHROMEOS)
    return optional_fields_.pinned;
#else
    return false;
#endif  // defined(OS_CHROMEOS)
  }
#if defined(OS_CHROMEOS)
  void set_pinned(bool pinned) { optional_fields_.pinned = pinned; }
#endif  // defined(OS_CHROMEOS)

  // Gets a text for spoken feedback.
  const base::string16& accessible_name() const {
    return optional_fields_.accessible_name;
  }

  NotificationDelegate* delegate() const { return delegate_.get(); }

  const RichNotificationData& rich_notification_data() const {
    return optional_fields_;
  }

  // Set the priority to SYSTEM. The system priority user needs to call this
  // method explicitly, to avoid setting it accidentally.
  void SetSystemPriority();

  // Delegate actions.
  void Display() const { delegate()->Display(); }
  bool HasClickedListener() const { return delegate()->HasClickedListener(); }
  void Click() const { delegate()->Click(); }
  void ButtonClick(int index) const { delegate()->ButtonClick(index); }
  void Close(bool by_user) const { delegate()->Close(by_user); }

  // Helper method to create a simple system notification. |click_callback|
  // will be invoked when the notification is clicked.
  static std::unique_ptr<Notification> CreateSystemNotification(
      const std::string& notification_id,
      const base::string16& title,
      const base::string16& message,
      const gfx::Image& icon,
      const std::string& system_component_id,
      const base::Closure& click_callback);

 protected:
  // The type of notification we'd like displayed.
  NotificationType type_;

  std::string id_;
  base::string16 title_;
  base::string16 message_;

  // Image data for the associated icon, used by Ash when available.
  gfx::Image icon_;

  // The display string for the source of the notification.  Could be
  // the same as origin_url_, or the name of an extension.
  base::string16 display_source_;

 private:
  // The origin URL of the script which requested the notification.
  // Can be empty if requested through a chrome app or extension or if
  // it's a system notification.
  GURL origin_url_;
  NotifierId notifier_id_;
  unsigned serial_number_;
  RichNotificationData optional_fields_;
  bool shown_as_popup_;  // True if this has been shown as a popup.
  bool is_read_;  // True if this has been seen in the message center.

  // A proxy object that allows access back to the JavaScript object that
  // represents the notification, for firing events.
  scoped_refptr<NotificationDelegate> delegate_;

#if !defined(OS_IOS)
  friend struct mojo::StructTraits<mojom::NotificationDataView, Notification>;
#endif
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_NOTIFICATION_H_
