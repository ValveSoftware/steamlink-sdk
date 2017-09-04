// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NOTIFICATIONS_NOTIFICATION_EVENT_DISPATCHER_IMPL_H_
#define CONTENT_BROWSER_NOTIFICATIONS_NOTIFICATION_EVENT_DISPATCHER_IMPL_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "content/public/browser/notification_database_data.h"
#include "content/public/browser/notification_event_dispatcher.h"

namespace content {

class NotificationEventDispatcherImpl : public NotificationEventDispatcher {
 public:
  // Returns the instance of the NotificationEventDispatcherImpl. Must be called
  // on the UI thread.
  static NotificationEventDispatcherImpl* GetInstance();

  // NotificationEventDispatcher implementation.
  void DispatchNotificationClickEvent(
      BrowserContext* browser_context,
      const std::string& notification_id,
      const GURL& origin,
      int action_index,
      const base::NullableString16& reply,
      const NotificationDispatchCompleteCallback& dispatch_complete_callback)
      override;
  void DispatchNotificationCloseEvent(
      BrowserContext* browser_context,
      const std::string& notification_id,
      const GURL& origin,
      bool by_user,
      const NotificationDispatchCompleteCallback& dispatch_complete_callback)
      override;

 private:
  NotificationEventDispatcherImpl();
  ~NotificationEventDispatcherImpl() override;

  friend struct base::DefaultSingletonTraits<NotificationEventDispatcherImpl>;

  DISALLOW_COPY_AND_ASSIGN(NotificationEventDispatcherImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NOTIFICATIONS_NOTIFICATION_EVENT_DISPATCHER_IMPL_H_
