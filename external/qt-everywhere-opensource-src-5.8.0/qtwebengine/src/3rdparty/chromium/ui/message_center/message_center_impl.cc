// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/message_center_impl.h"

#include <algorithm>
#include <deque>
#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/message_center_switches.h"
#include "ui/message_center/message_center_types.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_blocker.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/notification_types.h"
#include "ui/message_center/popup_timers_controller.h"

namespace message_center {
namespace internal {

// ChangeQueue keeps track of all the changes that we need to make to the
// notification list once the visibility is set to VISIBILITY_TRANSIENT.
class ChangeQueue {
 public:
  enum ChangeType {
    CHANGE_TYPE_ADD = 0,
    CHANGE_TYPE_UPDATE,
    CHANGE_TYPE_DELETE
  };

  // Change represents an operation made on a notification.  Since it contains
  // the final state of the notification, we only keep the last change for a
  // particular notification that is in the notification list around.  There are
  // two ids; |id_| is the newest notification id that has been assigned by an
  // update, and |notification_list_id_| is the id of the notification it should
  // be updating as it exists in the notification list.
  class Change {
   public:
    Change(ChangeType type,
           const std::string& id,
           std::unique_ptr<Notification> notification);
    ~Change();

    // Used to transfer ownership of the contained notification.
    std::unique_ptr<Notification> PassNotification();

    Notification* notification() const { return notification_.get(); }
    // Returns the post-update ID. It means:
    // - ADD event: ID of the notification to be added.
    // - UPDATE event: ID of the notification after the change. If the change
    //   doesn't update its ID, this value is same as |notification_list_id|.
    // - DELETE event: ID of the notification to be deleted.
    const std::string& id() const { return id_; }
    ChangeType type() const { return type_; }
    bool by_user() const { return by_user_; }
    void set_by_user(bool by_user) { by_user_ = by_user; }
    // Returns the ID which is used in the notification list. In other word, it
    // means the ID before the change.
    const std::string& notification_list_id() const {
      return notification_list_id_;
    }
    void set_type(const ChangeType new_type) {
      type_ = new_type;
    }
    void ReplaceNotification(std::unique_ptr<Notification> new_notification);

   private:
    ChangeType type_;
    std::string id_;
    std::string notification_list_id_;
    bool by_user_;
    std::unique_ptr<Notification> notification_;

    DISALLOW_COPY_AND_ASSIGN(Change);
  };

  ChangeQueue();
  ~ChangeQueue();

  // Called when the message center has appropriate visibility.  Modifies
  // |message_center| but does not retain it.  This also causes the queue to
  // empty itself.
  void ApplyChanges(MessageCenterImpl* message_center);

  // Applies only the changes of the given ID.
  void ApplyChangesForId(MessageCenterImpl* message_center,
                         const std::string& id);

  // Causes a TYPE_ADD change to be added to the queue.
  void AddNotification(std::unique_ptr<Notification> notification);

  // Causes a TYPE_UPDATE change to be added to the queue.
  void UpdateNotification(const std::string& old_id,
                          std::unique_ptr<Notification> notification);

  // Causes a TYPE_DELETE change to be added to the queue.
  void EraseNotification(const std::string& id, bool by_user);

  // Returns whether the queue matches an id.  The id given will be matched
  // against the ID of all changes post-update, not the id of the notification
  // as it stands in the notification list.
  bool Has(const std::string& id) const;

  // Returns a Change that can be modified by the caller.  ChangeQueue retains
  // ownership of the Change; pointers should not be retained.
  Notification* GetLatestNotification(const std::string& id) const;

 private:
  void ApplyChangeInternal(MessageCenterImpl* message_center,
                           std::unique_ptr<Change> change);

  ScopedVector<Change> changes_;

  DISALLOW_COPY_AND_ASSIGN(ChangeQueue);
};

////////////////////////////////////////////////////////////////////////////////
// ChangeFinder

struct ChangeFinder {
  explicit ChangeFinder(const std::string& id) : id(id) {}
  bool operator()(ChangeQueue::Change* change) { return change->id() == id; }

  std::string id;
};

////////////////////////////////////////////////////////////////////////////////
// ChangeQueue::Change

ChangeQueue::Change::Change(ChangeType type,
                            const std::string& id,
                            std::unique_ptr<Notification> notification)
    : type_(type),
      notification_list_id_(id),
      by_user_(false),
      notification_(std::move(notification)) {
  DCHECK(!id.empty());
  DCHECK(type != CHANGE_TYPE_DELETE || !notification_);

  id_ = notification_ ? notification_->id() : notification_list_id_;
}

ChangeQueue::Change::~Change() {}

std::unique_ptr<Notification> ChangeQueue::Change::PassNotification() {
  return std::move(notification_);
}

void ChangeQueue::Change::ReplaceNotification(
    std::unique_ptr<Notification> new_notification) {
  id_ = new_notification ? new_notification->id() : notification_list_id_;
  notification_.swap(new_notification);
}

////////////////////////////////////////////////////////////////////////////////
// ChangeQueue

ChangeQueue::ChangeQueue() {}

ChangeQueue::~ChangeQueue() {}

void ChangeQueue::ApplyChanges(MessageCenterImpl* message_center) {
  // This method is re-entrant.
  while (!changes_.empty()) {
    ScopedVector<Change>::iterator iter = changes_.begin();
    std::unique_ptr<Change> change(*iter);
    // TODO(dewittj): Replace changes_ with a deque.
    changes_.weak_erase(iter);
    ApplyChangeInternal(message_center, std::move(change));
  }
}

void ChangeQueue::ApplyChangesForId(MessageCenterImpl* message_center,
                                    const std::string& id) {
  std::deque<Change*> changes_for_id;
  std::string interesting_id = id;

  // Traverses the queue in reverse so shat we can track changes which change
  // the notification's ID.
  ScopedVector<Change>::iterator iter = changes_.end();
  while (iter != changes_.begin()) {
    --iter;
    if (interesting_id != (*iter)->id())
      continue;
    std::unique_ptr<Change> change(*iter);

    interesting_id = change->notification_list_id();

    iter = changes_.weak_erase(iter);
    changes_for_id.push_back(change.release());
  }

  while (!changes_for_id.empty()) {
    ApplyChangeInternal(message_center,
                        std::unique_ptr<Change>(changes_for_id.back()));
    changes_for_id.pop_back();
  }
}

void ChangeQueue::AddNotification(std::unique_ptr<Notification> notification) {
  std::string id = notification->id();
  changes_.push_back(new Change(CHANGE_TYPE_ADD, id, std::move(notification)));
}

void ChangeQueue::UpdateNotification(
    const std::string& old_id,
    std::unique_ptr<Notification> notification) {
  ScopedVector<Change>::reverse_iterator iter =
    std::find_if(changes_.rbegin(), changes_.rend(), ChangeFinder(old_id));
  if (iter == changes_.rend()) {
    changes_.push_back(
        new Change(CHANGE_TYPE_UPDATE, old_id, std::move(notification)));
    return;
  }

  Change* change = *iter;
  switch (change->type()) {
    case CHANGE_TYPE_ADD: {
      std::string id = notification->id();
      // Needs to add the change at the last, because if this change updates
      // its ID, some previous changes may affect new ID.
      // (eg. Add A, Update B->C, and This update A->B).
      changes_.erase(--(iter.base()));
      changes_.push_back(
          new Change(CHANGE_TYPE_ADD, id, std::move(notification)));
      break;
    }
    case CHANGE_TYPE_UPDATE:
      if (notification->id() == old_id) {
        // Safe to place the change at the previous place.
        change->ReplaceNotification(std::move(notification));
      } else if (change->id() == change->notification_list_id()) {
        std::string id = notification->id();
        // Safe to place the change at the last.
        changes_.erase(--(iter.base()));
        changes_.push_back(
            new Change(CHANGE_TYPE_ADD, id, std::move(notification)));
      } else {
        // Complex case: gives up to optimize.
        changes_.push_back(
            new Change(CHANGE_TYPE_UPDATE, old_id, std::move(notification)));
      }
      break;
    case CHANGE_TYPE_DELETE:
      // DELETE -> UPDATE. Something is wrong. Treats the UPDATE as ADD.
      changes_.push_back(
          new Change(CHANGE_TYPE_ADD, old_id, std::move(notification)));
      break;
    default:
      NOTREACHED();
  }
}

void ChangeQueue::EraseNotification(const std::string& id, bool by_user) {
  ScopedVector<Change>::reverse_iterator iter =
    std::find_if(changes_.rbegin(), changes_.rend(), ChangeFinder(id));
  if (iter == changes_.rend()) {
    std::unique_ptr<Change> change(new Change(CHANGE_TYPE_DELETE, id, nullptr));
    change->set_by_user(by_user);
    changes_.push_back(std::move(change));
    return;
  }

  Change* change = *iter;
  switch (change->type()) {
    case CHANGE_TYPE_ADD:
      // ADD -> DELETE. Just removes both.
      changes_.erase(--(iter.base()));
      break;
    case CHANGE_TYPE_UPDATE:
      // UPDATE -> DELETE. Changes the previous UPDATE to DELETE.
      change->set_type(CHANGE_TYPE_DELETE);
      change->set_by_user(by_user);
      change->ReplaceNotification(nullptr);
      break;
    case CHANGE_TYPE_DELETE:
      // DELETE -> DELETE. Something is wrong. Combines them with overriding
      // the |by_user| flag.
      change->set_by_user(!change->by_user() && by_user);
      break;
    default:
      NOTREACHED();
  }
}

bool ChangeQueue::Has(const std::string& id) const {
  ScopedVector<Change>::const_iterator iter =
      std::find_if(changes_.begin(), changes_.end(), ChangeFinder(id));
  return iter != changes_.end();
}

Notification* ChangeQueue::GetLatestNotification(const std::string& id) const {
  ScopedVector<Change>::const_iterator iter =
      std::find_if(changes_.begin(), changes_.end(), ChangeFinder(id));
  if (iter == changes_.end())
    return NULL;

  return (*iter)->notification();
}

void ChangeQueue::ApplyChangeInternal(MessageCenterImpl* message_center,
                                      std::unique_ptr<Change> change) {
  switch (change->type()) {
    case CHANGE_TYPE_ADD:
      message_center->AddNotificationImmediately(change->PassNotification());
      break;
    case CHANGE_TYPE_UPDATE:
      message_center->UpdateNotificationImmediately(
          change->notification_list_id(), change->PassNotification());
      break;
    case CHANGE_TYPE_DELETE:
      message_center->RemoveNotificationImmediately(
          change->notification_list_id(), change->by_user());
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace internal

////////////////////////////////////////////////////////////////////////////////
// MessageCenterImpl::NotificationCache

MessageCenterImpl::NotificationCache::NotificationCache()
    : unread_count(0) {}

MessageCenterImpl::NotificationCache::~NotificationCache() {}

void MessageCenterImpl::NotificationCache::Rebuild(
    const NotificationList::Notifications& notifications) {
  visible_notifications = notifications;
  RecountUnread();
}

void MessageCenterImpl::NotificationCache::RecountUnread() {
  unread_count = 0;
  for (const auto& notification : visible_notifications) {
    if (!notification->IsRead())
      ++unread_count;
  }
}

////////////////////////////////////////////////////////////////////////////////
// MessageCenterImpl

MessageCenterImpl::MessageCenterImpl()
    : MessageCenter(),
      popup_timers_controller_(new PopupTimersController(this)),
      settings_provider_(NULL) {
  notification_list_.reset(new NotificationList(this));

  bool enable_message_center_changes_while_open = true;  // enable by default
  std::string arg = base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kMessageCenterChangesWhileOpen);
  if (!arg.empty()) {
    if (arg == "enabled")
      enable_message_center_changes_while_open = true;
    else if (arg == "disabled")
      enable_message_center_changes_while_open = false;
  }

  if (!enable_message_center_changes_while_open)
    notification_queue_.reset(new internal::ChangeQueue());
}

MessageCenterImpl::~MessageCenterImpl() {
  SetNotifierSettingsProvider(NULL);
}

void MessageCenterImpl::AddObserver(MessageCenterObserver* observer) {
  observer_list_.AddObserver(observer);
}

void MessageCenterImpl::RemoveObserver(MessageCenterObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void MessageCenterImpl::AddNotificationBlocker(NotificationBlocker* blocker) {
  if (ContainsValue(blockers_, blocker))
    return;

  blocker->AddObserver(this);
  blockers_.push_back(blocker);
}

void MessageCenterImpl::RemoveNotificationBlocker(
    NotificationBlocker* blocker) {
  std::vector<NotificationBlocker*>::iterator iter =
      std::find(blockers_.begin(), blockers_.end(), blocker);
  if (iter == blockers_.end())
    return;
  blocker->RemoveObserver(this);
  blockers_.erase(iter);
}

void MessageCenterImpl::OnBlockingStateChanged(NotificationBlocker* blocker) {
  std::list<std::string> blocked_ids;
  NotificationList::PopupNotifications popups =
      notification_list_->GetPopupNotifications(blockers_, &blocked_ids);

  for (const auto& id : blocked_ids) {
    // Do not call MessageCenterImpl::MarkSinglePopupAsShown() directly here
    // just for performance reason. MessageCenterImpl::MarkSinglePopupAsShown()
    // calls NotificationList::MarkSinglePopupAsShown() and then updates the
    // unread count, but the whole cache will be recreated below.
    notification_list_->MarkSinglePopupAsShown(id, true);
  }
  notification_cache_.Rebuild(
      notification_list_->GetVisibleNotifications(blockers_));

  for (const auto& id : blocked_ids) {
    FOR_EACH_OBSERVER(MessageCenterObserver,
                      observer_list_,
                      OnNotificationUpdated(id));
  }
  FOR_EACH_OBSERVER(MessageCenterObserver,
                    observer_list_,
                    OnBlockingStateChanged(blocker));
}

void MessageCenterImpl::UpdateIconImage(
    const NotifierId& notifier_id, const gfx::Image& icon) {}

void MessageCenterImpl::NotifierGroupChanged() {}

void MessageCenterImpl::NotifierEnabledChanged(
    const NotifierId& notifier_id, bool enabled) {
  if (!enabled) {
    RemoveNotificationsForNotifierId(notifier_id);
  }
}

void MessageCenterImpl::SetVisibility(Visibility visibility) {
  visible_ = (visibility == VISIBILITY_MESSAGE_CENTER);

  if (visible_ && !locked_) {
    std::set<std::string> updated_ids;
    notification_list_->SetNotificationsShown(blockers_, &updated_ids);
    notification_cache_.RecountUnread();

    for (const auto& id : updated_ids) {
      FOR_EACH_OBSERVER(
          MessageCenterObserver, observer_list_, OnNotificationUpdated(id));
    }
  }

  if (notification_queue_ &&
      visibility == VISIBILITY_TRANSIENT) {
    notification_queue_->ApplyChanges(this);
  }

  FOR_EACH_OBSERVER(MessageCenterObserver,
                    observer_list_,
                    OnCenterVisibilityChanged(visibility));
}

bool MessageCenterImpl::IsMessageCenterVisible() const {
  return visible_;
}

size_t MessageCenterImpl::NotificationCount() const {
  return notification_cache_.visible_notifications.size();
}

size_t MessageCenterImpl::UnreadNotificationCount() const {
  return notification_cache_.unread_count;
}

bool MessageCenterImpl::HasPopupNotifications() const {
  return !IsMessageCenterVisible() &&
      notification_list_->HasPopupNotifications(blockers_);
}

bool MessageCenterImpl::IsQuietMode() const {
  return notification_list_->quiet_mode();
}

bool MessageCenterImpl::IsLockedState() const {
  return locked_;
}

bool MessageCenterImpl::HasClickedListener(const std::string& id) {
  scoped_refptr<NotificationDelegate> delegate =
      notification_list_->GetNotificationDelegate(id);
  return delegate.get() && delegate->HasClickedListener();
}

message_center::Notification* MessageCenterImpl::FindVisibleNotificationById(
    const std::string& id) {
  return notification_list_->GetNotificationById(id);
}

const NotificationList::Notifications&
MessageCenterImpl::GetVisibleNotifications() {
  return notification_cache_.visible_notifications;
}

NotificationList::PopupNotifications
    MessageCenterImpl::GetPopupNotifications() {
  return notification_list_->GetPopupNotifications(blockers_, NULL);
}

void MessageCenterImpl::ForceNotificationFlush(const std::string& id) {
  if (notification_queue_)
    notification_queue_->ApplyChangesForId(this, id);
}

//------------------------------------------------------------------------------
// Client code interface.
void MessageCenterImpl::AddNotification(
    std::unique_ptr<Notification> notification) {
  DCHECK(notification);
  const std::string id = notification->id();
  for (size_t i = 0; i < blockers_.size(); ++i)
    blockers_[i]->CheckState();

  if (notification_queue_ && visible_) {
    notification_queue_->AddNotification(std::move(notification));
    return;
  }

  AddNotificationImmediately(std::move(notification));
}

void MessageCenterImpl::AddNotificationImmediately(
    std::unique_ptr<Notification> notification) {
  const std::string id = notification->id();

  // Sometimes the notification can be added with the same id and the
  // |notification_list| will replace the notification instead of adding new.
  // This is essentially an update rather than addition.
  bool already_exists = (notification_list_->GetNotificationById(id) != NULL);
  notification_list_->AddNotification(std::move(notification));
  notification_cache_.Rebuild(
      notification_list_->GetVisibleNotifications(blockers_));

  if (already_exists) {
    FOR_EACH_OBSERVER(
        MessageCenterObserver, observer_list_, OnNotificationUpdated(id));
  } else {
    FOR_EACH_OBSERVER(
        MessageCenterObserver, observer_list_, OnNotificationAdded(id));
  }
}

void MessageCenterImpl::UpdateNotification(
    const std::string& old_id,
    std::unique_ptr<Notification> new_notification) {
  for (size_t i = 0; i < blockers_.size(); ++i)
    blockers_[i]->CheckState();

  if (notification_queue_ && visible_) {
    // We will allow notifications that are progress types (and stay progress
    // types) to be updated even if the message center is open.  There are 3
    // requirements here:
    //  * Notification of type PROGRESS exists with same ID in the center
    //  * There are no queued updates for this notification (they imply a change
    //    that violates the PROGRESS invariant
    //  * The new notification is type PROGRESS.
    // TODO(dewittj): Ensure this works when the ID is changed by the caller.
    // This shouldn't be an issue in practice since only W3C notifications
    // change the ID on update, and they don't have progress type notifications.
    bool update_keeps_progress_type =
        new_notification->type() == NOTIFICATION_TYPE_PROGRESS &&
        !notification_queue_->Has(old_id) &&
        notification_list_->HasNotificationOfType(old_id,
                                                  NOTIFICATION_TYPE_PROGRESS);
    if (!update_keeps_progress_type) {
      // Updates are allowed only for progress notifications.
      notification_queue_->UpdateNotification(old_id,
                                              std::move(new_notification));
      return;
    }
  }

  UpdateNotificationImmediately(old_id, std::move(new_notification));
}

void MessageCenterImpl::UpdateNotificationImmediately(
    const std::string& old_id,
    std::unique_ptr<Notification> new_notification) {
  std::string new_id = new_notification->id();
  notification_list_->UpdateNotificationMessage(old_id,
                                                std::move(new_notification));
  notification_cache_.Rebuild(
     notification_list_->GetVisibleNotifications(blockers_));
  if (old_id == new_id) {
    FOR_EACH_OBSERVER(
        MessageCenterObserver, observer_list_, OnNotificationUpdated(new_id));
  } else {
    FOR_EACH_OBSERVER(MessageCenterObserver, observer_list_,
                      OnNotificationRemoved(old_id, false));
    FOR_EACH_OBSERVER(MessageCenterObserver, observer_list_,
                      OnNotificationAdded(new_id));
  }
}

void MessageCenterImpl::RemoveNotification(const std::string& id,
                                           bool by_user) {
  if (notification_queue_ && !by_user && visible_) {
    notification_queue_->EraseNotification(id, by_user);
    return;
  }

  RemoveNotificationImmediately(id, by_user);
}

void MessageCenterImpl::RemoveNotificationImmediately(
    const std::string& id, bool by_user) {
  if (FindVisibleNotificationById(id) == NULL)
    return;

  // In many cases |id| is a reference to an existing notification instance
  // but the instance can be destructed in this method. Hence copies the id
  // explicitly here.
  std::string copied_id(id);

  scoped_refptr<NotificationDelegate> delegate =
      notification_list_->GetNotificationDelegate(copied_id);
  if (delegate.get())
    delegate->Close(by_user);

  notification_list_->RemoveNotification(copied_id);
  notification_cache_.Rebuild(
      notification_list_->GetVisibleNotifications(blockers_));
  FOR_EACH_OBSERVER(MessageCenterObserver,
                    observer_list_,
                    OnNotificationRemoved(copied_id, by_user));
}

void MessageCenterImpl::RemoveNotificationsForNotifierId(
    const NotifierId& notifier_id) {
  NotificationList::Notifications notifications =
      notification_list_->GetNotificationsByNotifierId(notifier_id);
  for (const auto& notification : notifications)
    RemoveNotification(notification->id(), false);
  if (!notifications.empty()) {
    notification_cache_.Rebuild(
        notification_list_->GetVisibleNotifications(blockers_));
  }
}

void MessageCenterImpl::RemoveAllNotifications(bool by_user, RemoveType type) {
  bool remove_pinned = (type == RemoveType::NON_PINNED);

  const NotificationBlockers& blockers =
      (type == RemoveType::ALL ? NotificationBlockers() /* empty blockers */
                               : blockers_ /* use default blockers */);

  const NotificationList::Notifications notifications =
      notification_list_->GetVisibleNotifications(blockers);
  std::set<std::string> ids;
  for (const auto& notification : notifications) {
    if (!remove_pinned && notification->pinned())
      continue;

    ids.insert(notification->id());
    scoped_refptr<NotificationDelegate> delegate = notification->delegate();
    if (delegate.get())
      delegate->Close(by_user);
    notification_list_->RemoveNotification(notification->id());
  }

  if (!ids.empty()) {
    notification_cache_.Rebuild(
        notification_list_->GetVisibleNotifications(blockers_));
  }
  for (const auto& id : ids) {
    FOR_EACH_OBSERVER(MessageCenterObserver,
                      observer_list_,
                      OnNotificationRemoved(id, by_user));
  }
}

void MessageCenterImpl::SetNotificationIcon(const std::string& notification_id,
                                            const gfx::Image& image) {
  bool updated = false;
  Notification* queue_notification =
      notification_queue_
          ? notification_queue_->GetLatestNotification(notification_id)
          : NULL;

  if (queue_notification) {
    queue_notification->set_icon(image);
    updated = true;
  } else {
    updated = notification_list_->SetNotificationIcon(notification_id, image);
  }

  if (updated) {
    FOR_EACH_OBSERVER(MessageCenterObserver, observer_list_,
                      OnNotificationUpdated(notification_id));
  }
}

void MessageCenterImpl::SetNotificationImage(const std::string& notification_id,
                                             const gfx::Image& image) {
  bool updated = false;
  Notification* queue_notification =
      notification_queue_
          ? notification_queue_->GetLatestNotification(notification_id)
          : NULL;

  if (queue_notification) {
    queue_notification->set_image(image);
    updated = true;
  } else {
    updated = notification_list_->SetNotificationImage(notification_id, image);
  }

  if (updated) {
    FOR_EACH_OBSERVER(MessageCenterObserver, observer_list_,
                      OnNotificationUpdated(notification_id));
  }
}

void MessageCenterImpl::SetNotificationButtonIcon(
    const std::string& notification_id, int button_index,
    const gfx::Image& image) {
  bool updated = false;
  Notification* queue_notification =
      notification_queue_
          ? notification_queue_->GetLatestNotification(notification_id)
          : NULL;

  if (queue_notification) {
    queue_notification->SetButtonIcon(button_index, image);
    updated = true;
  } else {
    updated = notification_list_->SetNotificationButtonIcon(
        notification_id, button_index, image);
  }

  if (updated) {
    FOR_EACH_OBSERVER(MessageCenterObserver, observer_list_,
                      OnNotificationUpdated(notification_id));
  }
}

void MessageCenterImpl::DisableNotificationsByNotifier(
    const NotifierId& notifier_id) {
  if (settings_provider_) {
    // TODO(mukai): SetNotifierEnabled can just accept notifier_id?
    Notifier notifier(notifier_id, base::string16(), true);
    settings_provider_->SetNotifierEnabled(notifier, false);
    // The settings provider will call back to remove the notifications
    // belonging to the notifier id.
  } else {
    RemoveNotificationsForNotifierId(notifier_id);
  }
}

void MessageCenterImpl::ClickOnNotification(const std::string& id) {
  if (FindVisibleNotificationById(id) == NULL)
    return;
#if defined(OS_CHROMEOS)
  if (HasPopupNotifications())
    MarkSinglePopupAsShown(id, true);
#endif
  scoped_refptr<NotificationDelegate> delegate =
      notification_list_->GetNotificationDelegate(id);
  if (delegate.get())
    delegate->Click();
  FOR_EACH_OBSERVER(
      MessageCenterObserver, observer_list_, OnNotificationClicked(id));
}

void MessageCenterImpl::ClickOnNotificationButton(const std::string& id,
                                                  int button_index) {
  if (FindVisibleNotificationById(id) == NULL)
    return;
#if defined(OS_CHROMEOS)
  if (HasPopupNotifications())
    MarkSinglePopupAsShown(id, true);
#endif
  scoped_refptr<NotificationDelegate> delegate =
      notification_list_->GetNotificationDelegate(id);
  if (delegate.get())
    delegate->ButtonClick(button_index);
  FOR_EACH_OBSERVER(
      MessageCenterObserver, observer_list_, OnNotificationButtonClicked(
          id, button_index));
}

void MessageCenterImpl::ClickOnSettingsButton(const std::string& id) {
  scoped_refptr<NotificationDelegate> delegate =
      notification_list_->GetNotificationDelegate(id);
  if (delegate.get())
    delegate->SettingsClick();
  FOR_EACH_OBSERVER(MessageCenterObserver, observer_list_,
                    OnNotificationSettingsClicked());
}

void MessageCenterImpl::MarkSinglePopupAsShown(const std::string& id,
                                               bool mark_notification_as_read) {
  if (FindVisibleNotificationById(id) == NULL)
    return;
#if !defined(OS_CHROMEOS)
  return this->RemoveNotification(id, false);
#else
  notification_list_->MarkSinglePopupAsShown(id, mark_notification_as_read);
  notification_cache_.RecountUnread();
  FOR_EACH_OBSERVER(
      MessageCenterObserver, observer_list_, OnNotificationUpdated(id));
#endif  // defined(OS_CHROMEOS)
}

void MessageCenterImpl::DisplayedNotification(
    const std::string& id,
    const DisplaySource source) {
  if (FindVisibleNotificationById(id) == NULL)
    return;

  if (HasPopupNotifications())
    notification_list_->MarkSinglePopupAsDisplayed(id);
  notification_cache_.RecountUnread();
  scoped_refptr<NotificationDelegate> delegate =
      notification_list_->GetNotificationDelegate(id);
  if (delegate.get())
    delegate->Display();
  FOR_EACH_OBSERVER(
      MessageCenterObserver,
      observer_list_,
      OnNotificationDisplayed(id, source));
}

void MessageCenterImpl::SetNotifierSettingsProvider(
    NotifierSettingsProvider* provider) {
  if (settings_provider_) {
    settings_provider_->RemoveObserver(this);
    settings_provider_ = NULL;
  }
  settings_provider_ = provider;
  if (settings_provider_)
    settings_provider_->AddObserver(this);
}

NotifierSettingsProvider* MessageCenterImpl::GetNotifierSettingsProvider() {
  return settings_provider_;
}

void MessageCenterImpl::SetQuietMode(bool in_quiet_mode) {
  if (in_quiet_mode != notification_list_->quiet_mode()) {
    notification_list_->SetQuietMode(in_quiet_mode);
    FOR_EACH_OBSERVER(MessageCenterObserver,
                      observer_list_,
                      OnQuietModeChanged(in_quiet_mode));
  }
  quiet_mode_timer_.reset();
}

void MessageCenterImpl::SetLockedState(bool locked) {
  if (locked != locked_) {
    locked_ = locked;
    FOR_EACH_OBSERVER(MessageCenterObserver, observer_list_,
                      OnLockedStateChanged(locked));
  }
}

void MessageCenterImpl::EnterQuietModeWithExpire(
    const base::TimeDelta& expires_in) {
  if (quiet_mode_timer_) {
    // Note that the capital Reset() is the method to restart the timer, not
    // scoped_ptr::reset().
    quiet_mode_timer_->Reset();
  } else {
    notification_list_->SetQuietMode(true);
    FOR_EACH_OBSERVER(
        MessageCenterObserver, observer_list_, OnQuietModeChanged(true));

    quiet_mode_timer_.reset(new base::OneShotTimer);
    quiet_mode_timer_->Start(
        FROM_HERE,
        expires_in,
        base::Bind(
            &MessageCenterImpl::SetQuietMode, base::Unretained(this), false));
  }
}

void MessageCenterImpl::RestartPopupTimers() {
  if (popup_timers_controller_)
    popup_timers_controller_->StartAll();
}

void MessageCenterImpl::PausePopupTimers() {
  if (popup_timers_controller_)
    popup_timers_controller_->PauseAll();
}

void MessageCenterImpl::DisableTimersForTest() {
  popup_timers_controller_.reset();
}

void MessageCenterImpl::EnableChangeQueueForTest(bool enable) {
  if (enable)
    notification_queue_.reset(new internal::ChangeQueue());
  else
    notification_queue_.reset();
}

}  // namespace message_center
