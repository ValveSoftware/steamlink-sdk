// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_NOTIFICATION_DELEGATE_H_
#define UI_MESSAGE_CENTER_NOTIFICATION_DELEGATE_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "ui/message_center/message_center_export.h"

namespace content {
class RenderViewHost;
}

namespace message_center {

// Delegate for a notification. This class has two roles: to implement callback
// methods for notification, and to provide an identity of the associated
// notification.
class MESSAGE_CENTER_EXPORT NotificationDelegate
    : public base::RefCountedThreadSafe<NotificationDelegate> {
 public:
  // To be called when the desktop notification is actually shown.
  virtual void Display() = 0;

  // To be called when the desktop notification cannot be shown due to an
  // error.
  virtual void Error() = 0;

  // To be called when the desktop notification is closed.  If closed by a
  // user explicitly (as opposed to timeout/script), |by_user| should be true.
  virtual void Close(bool by_user) = 0;

  // Returns true if the delegate can handle click event.
  virtual bool HasClickedListener();

  // To be called when a desktop notification is clicked.
  virtual void Click() = 0;

  // To be called when the user clicks a button in a notification. TODO(miket):
  // consider providing default implementations of the pure virtuals of this
  // interface, to avoid pinging so many OWNERs each time we enhance it.
  virtual void ButtonClick(int button_index);

 protected:
  virtual ~NotificationDelegate() {}

 private:
  friend class base::RefCountedThreadSafe<NotificationDelegate>;
};

// A simple notification delegate which invokes the passed closure when clicked.
class MESSAGE_CENTER_EXPORT HandleNotificationClickedDelegate
    : public NotificationDelegate {
 public:
  explicit HandleNotificationClickedDelegate(const base::Closure& closure);

  // message_center::NotificationDelegate overrides:
  virtual void Display() OVERRIDE;
  virtual void Error() OVERRIDE;
  virtual void Close(bool by_user) OVERRIDE;
  virtual bool HasClickedListener() OVERRIDE;
  virtual void Click() OVERRIDE;
  virtual void ButtonClick(int button_index) OVERRIDE;

 protected:
  virtual ~HandleNotificationClickedDelegate();

 private:
  std::string id_;
  base::Closure closure_;

  DISALLOW_COPY_AND_ASSIGN(HandleNotificationClickedDelegate);
};

// A notification delegate which invokes a callback when a notification button
// has been clicked.
class MESSAGE_CENTER_EXPORT HandleNotificationButtonClickDelegate
    : public NotificationDelegate {
 public:
  typedef base::Callback<void(int)> ButtonClickCallback;

  explicit HandleNotificationButtonClickDelegate(
      const ButtonClickCallback& button_callback);

  // message_center::NotificationDelegate overrides:
  virtual void Display() OVERRIDE;
  virtual void Error() OVERRIDE;
  virtual void Close(bool by_user) OVERRIDE;
  virtual void Click() OVERRIDE;
  virtual void ButtonClick(int button_index) OVERRIDE;

 protected:
  virtual ~HandleNotificationButtonClickDelegate();

 private:
  ButtonClickCallback button_callback_;

  DISALLOW_COPY_AND_ASSIGN(HandleNotificationButtonClickDelegate);
};

}  //  namespace message_center

#endif  // UI_MESSAGE_CENTER_NOTIFICATION_DELEGATE_H_
