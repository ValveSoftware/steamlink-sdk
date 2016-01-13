// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/message_center_impl.h"

#include <algorithm>

#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/message_center_types.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_blocker.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/notification_types.h"

namespace {

base::TimeDelta GetTimeoutForPriority(int priority) {
  if (priority > message_center::DEFAULT_PRIORITY) {
    return base::TimeDelta::FromSeconds(
        message_center::kAutocloseHighPriorityDelaySeconds);
  }
  return base::TimeDelta::FromSeconds(
      message_center::kAutocloseDefaultDelaySeconds);
}

}  // namespace

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
           scoped_ptr<Notification> notification);
    ~Change();

    // Used to transfer ownership of the contained notification.
    scoped_ptr<Notification> PassNotification();

    Notification* notification() const { return notification_.get(); }
    const std::string& id() const { return id_; }
    ChangeType type() const { return type_; }
    bool by_user() const { return by_user_; }
    void set_by_user(bool by_user) { by_user_ = by_user; }
    const std::string& notification_list_id() const {
      return notification_list_id_;
    }
    void set_notification_list_id(const std::string& id) {
      notification_list_id_ = id;
    }

   private:
    const ChangeType type_;
    const std::string id_;
    std::string notification_list_id_;
    bool by_user_;
    scoped_ptr<Notification> notification_;

    DISALLOW_COPY_AND_ASSIGN(Change);
  };

  ChangeQueue();
  ~ChangeQueue();

  // Called when the message center has appropriate visibility.  Modifies
  // |message_center| but does not retain it.  This also causes the queue to
  // empty itself.
  void ApplyChanges(MessageCenter* message_center);

  // Causes a TYPE_ADD change to be added to the queue.
  void AddNotification(scoped_ptr<Notification> notification);

  // Causes a TYPE_UPDATE change to be added to the queue.
  void UpdateNotification(const std::string& old_id,
                          scoped_ptr<Notification> notification);

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
  void Replace(const std::string& id, scoped_ptr<Change> change);

  ScopedVector<Change> changes_;
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
                            scoped_ptr<Notification> notification)
    : type_(type),
      id_(id),
      notification_list_id_(id),
      by_user_(false),
      notification_(notification.Pass()) {
  DCHECK(!id.empty() &&
         (type != CHANGE_TYPE_DELETE || notification_.get() == NULL));
}

ChangeQueue::Change::~Change() {}

scoped_ptr<Notification> ChangeQueue::Change::PassNotification() {
  return notification_.Pass();
}

////////////////////////////////////////////////////////////////////////////////
// ChangeQueue

ChangeQueue::ChangeQueue() {}

ChangeQueue::~ChangeQueue() {}

void ChangeQueue::ApplyChanges(MessageCenter* message_center) {
  // This method is re-entrant.
  while (!changes_.empty()) {
    ScopedVector<Change>::iterator iter = changes_.begin();
    scoped_ptr<Change> change(*iter);
    // TODO(dewittj): Replace changes_ with a deque.
    changes_.weak_erase(iter);
    // |message_center| is taking ownership of each element here.
    switch (change->type()) {
      case CHANGE_TYPE_ADD:
        message_center->AddNotification(change->PassNotification());
        break;
      case CHANGE_TYPE_UPDATE:
        message_center->UpdateNotification(change->notification_list_id(),
                                           change->PassNotification());
        break;
      case CHANGE_TYPE_DELETE:
        message_center->RemoveNotification(change->notification_list_id(),
                                           change->by_user());
        break;
      default:
        NOTREACHED();
    }
  }
}

void ChangeQueue::AddNotification(scoped_ptr<Notification> notification) {
  std::string id = notification->id();

  scoped_ptr<Change> change(
      new Change(CHANGE_TYPE_ADD, id, notification.Pass()));
  Replace(id, change.Pass());
}

void ChangeQueue::UpdateNotification(const std::string& old_id,
                                     scoped_ptr<Notification> notification) {
  std::string new_id = notification->id();
  scoped_ptr<Change> change(
      new Change(CHANGE_TYPE_UPDATE, new_id, notification.Pass()));
  Replace(old_id, change.Pass());
}

void ChangeQueue::EraseNotification(const std::string& id, bool by_user) {
  scoped_ptr<Change> change(
      new Change(CHANGE_TYPE_DELETE, id, scoped_ptr<Notification>()));
  change->set_by_user(by_user);
  Replace(id, change.Pass());
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

void ChangeQueue::Replace(const std::string& changed_id,
                          scoped_ptr<Change> new_change) {
  ScopedVector<Change>::iterator iter =
      std::find_if(changes_.begin(), changes_.end(), ChangeFinder(changed_id));
  if (iter != changes_.end()) {
    Change* old_change = *iter;
    new_change->set_notification_list_id(old_change->notification_list_id());
    changes_.erase(iter);
  } else {
    new_change->set_notification_list_id(changed_id);
  }

  changes_.push_back(new_change.release());
}

////////////////////////////////////////////////////////////////////////////////
// PopupTimer

PopupTimer::PopupTimer(const std::string& id,
                       base::TimeDelta timeout,
                       base::WeakPtr<PopupTimersController> controller)
    : id_(id),
      timeout_(timeout),
      timer_controller_(controller),
      timer_(new base::OneShotTimer<PopupTimersController>) {}

PopupTimer::~PopupTimer() {
  if (!timer_)
    return;

  if (timer_->IsRunning())
    timer_->Stop();
}

void PopupTimer::Start() {
  if (timer_->IsRunning())
    return;
  base::TimeDelta timeout_to_close =
      timeout_ <= passed_ ? base::TimeDelta() : timeout_ - passed_;
  start_time_ = base::Time::Now();
  timer_->Start(
      FROM_HERE,
      timeout_to_close,
      base::Bind(
          &PopupTimersController::TimerFinished, timer_controller_, id_));
}

void PopupTimer::Pause() {
  if (!timer_.get() || !timer_->IsRunning())
    return;

  timer_->Stop();
  passed_ += base::Time::Now() - start_time_;
}

void PopupTimer::Reset() {
  if (timer_)
    timer_->Stop();
  passed_ = base::TimeDelta();
}

////////////////////////////////////////////////////////////////////////////////
// PopupTimersController

PopupTimersController::PopupTimersController(MessageCenter* message_center)
    : message_center_(message_center), popup_deleter_(&popup_timers_) {
  message_center_->AddObserver(this);
}

PopupTimersController::~PopupTimersController() {
  message_center_->RemoveObserver(this);
}

void PopupTimersController::StartTimer(const std::string& id,
                                       const base::TimeDelta& timeout) {
  PopupTimerCollection::iterator iter = popup_timers_.find(id);
  if (iter != popup_timers_.end()) {
    DCHECK(iter->second);
    iter->second->Start();
    return;
  }

  PopupTimer* timer = new PopupTimer(id, timeout, AsWeakPtr());

  timer->Start();
  popup_timers_[id] = timer;
}

void PopupTimersController::StartAll() {
  std::map<std::string, PopupTimer*>::iterator iter;
  for (iter = popup_timers_.begin(); iter != popup_timers_.end(); iter++) {
    iter->second->Start();
  }
}

void PopupTimersController::ResetTimer(const std::string& id,
                                       const base::TimeDelta& timeout) {
  CancelTimer(id);
  StartTimer(id, timeout);
}

void PopupTimersController::PauseTimer(const std::string& id) {
  PopupTimerCollection::iterator iter = popup_timers_.find(id);
  if (iter == popup_timers_.end())
    return;
  iter->second->Pause();
}

void PopupTimersController::PauseAll() {
  std::map<std::string, PopupTimer*>::iterator iter;
  for (iter = popup_timers_.begin(); iter != popup_timers_.end(); iter++) {
    iter->second->Pause();
  }
}

void PopupTimersController::CancelTimer(const std::string& id) {
  PopupTimerCollection::iterator iter = popup_timers_.find(id);
  if (iter == popup_timers_.end())
    return;

  PopupTimer* timer = iter->second;
  delete timer;

  popup_timers_.erase(iter);
}

void PopupTimersController::CancelAll() {
  STLDeleteValues(&popup_timers_);
  popup_timers_.clear();
}

void PopupTimersController::TimerFinished(const std::string& id) {
  PopupTimerCollection::iterator iter = popup_timers_.find(id);
  if (iter == popup_timers_.end())
    return;

  CancelTimer(id);
  message_center_->MarkSinglePopupAsShown(id, false);
}

void PopupTimersController::OnNotificationDisplayed(
    const std::string& id,
    const DisplaySource source) {
  OnNotificationUpdated(id);
}

void PopupTimersController::OnNotificationUpdated(const std::string& id) {
  NotificationList::PopupNotifications popup_notifications =
      message_center_->GetPopupNotifications();

  if (!popup_notifications.size()) {
    CancelAll();
    return;
  }

  NotificationList::PopupNotifications::const_iterator iter =
      popup_notifications.begin();
  for (; iter != popup_notifications.end(); iter++) {
    if ((*iter)->id() == id)
      break;
  }

  if (iter == popup_notifications.end() || (*iter)->never_timeout()) {
    CancelTimer(id);
    return;
  }

  // Start the timer if not yet.
  if (popup_timers_.find(id) == popup_timers_.end())
    StartTimer(id, GetTimeoutForPriority((*iter)->priority()));
}

void PopupTimersController::OnNotificationRemoved(const std::string& id,
                                                  bool by_user) {
  CancelTimer(id);
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
  for (NotificationList::Notifications::const_iterator iter =
           visible_notifications.begin();
       iter != visible_notifications.end(); ++iter) {
    if (!(*iter)->IsRead())
      ++unread_count;
  }
}

////////////////////////////////////////////////////////////////////////////////
// MessageCenterImpl

MessageCenterImpl::MessageCenterImpl()
    : MessageCenter(),
      popup_timers_controller_(new internal::PopupTimersController(this)),
      settings_provider_(NULL) {
  notification_list_.reset(new NotificationList());
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
  if (std::find(blockers_.begin(), blockers_.end(), blocker) !=
      blockers_.end()) {
    return;
  }
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

  for (std::list<std::string>::const_iterator iter = blocked_ids.begin();
       iter != blocked_ids.end(); ++iter) {
    // Do not call MessageCenterImpl::MarkSinglePopupAsShown() directly here
    // just for performance reason. MessageCenterImpl::MarkSinglePopupAsShown()
    // calls NotificationList::MarkSinglePopupAsShown() and then updates the
    // unread count, but the whole cache will be recreated below.
    notification_list_->MarkSinglePopupAsShown((*iter), true);
    FOR_EACH_OBSERVER(MessageCenterObserver,
                      observer_list_,
                      OnNotificationUpdated(*iter));
  }
  notification_cache_.Rebuild(
      notification_list_->GetVisibleNotifications(blockers_));
  FOR_EACH_OBSERVER(MessageCenterObserver,
                    observer_list_,
                    OnBlockingStateChanged(blocker));
}

void MessageCenterImpl::UpdateIconImage(
  const NotifierId& notifier_id,const gfx::Image& icon) {}

void MessageCenterImpl::NotifierGroupChanged() {}

void MessageCenterImpl::NotifierEnabledChanged(
  const NotifierId& notifier_id, bool enabled) {
  if (!enabled) {
    RemoveNotificationsForNotifierId(notifier_id);
  }
}

void MessageCenterImpl::SetVisibility(Visibility visibility) {
  std::set<std::string> updated_ids;
  notification_list_->SetMessageCenterVisible(
      (visibility == VISIBILITY_MESSAGE_CENTER), &updated_ids);
  notification_cache_.RecountUnread();

  for (std::set<std::string>::const_iterator iter = updated_ids.begin();
       iter != updated_ids.end();
       ++iter) {
    FOR_EACH_OBSERVER(
        MessageCenterObserver, observer_list_, OnNotificationUpdated(*iter));
  }

  if (visibility == VISIBILITY_TRANSIENT)
    notification_queue_->ApplyChanges(this);

  FOR_EACH_OBSERVER(MessageCenterObserver,
                    observer_list_,
                    OnCenterVisibilityChanged(visibility));
}

bool MessageCenterImpl::IsMessageCenterVisible() const {
  return notification_list_->is_message_center_visible();
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

//------------------------------------------------------------------------------
// Client code interface.
void MessageCenterImpl::AddNotification(scoped_ptr<Notification> notification) {
  DCHECK(notification.get());
  const std::string id = notification->id();
  for (size_t i = 0; i < blockers_.size(); ++i)
    blockers_[i]->CheckState();

  if (notification_list_->is_message_center_visible()) {
    notification_queue_->AddNotification(notification.Pass());
    return;
  }

  // Sometimes the notification can be added with the same id and the
  // |notification_list| will replace the notification instead of adding new.
  // This is essentially an update rather than addition.
  bool already_exists = (notification_list_->GetNotificationById(id) != NULL);
  notification_list_->AddNotification(notification.Pass());
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
    scoped_ptr<Notification> new_notification) {
  for (size_t i = 0; i < blockers_.size(); ++i)
    blockers_[i]->CheckState();

  if (notification_list_->is_message_center_visible()) {
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
      notification_queue_->UpdateNotification(old_id, new_notification.Pass());
      return;
    }
  }

  std::string new_id = new_notification->id();
  notification_list_->UpdateNotificationMessage(old_id,
                                                new_notification.Pass());
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
  if (!by_user && notification_list_->is_message_center_visible()) {
    notification_queue_->EraseNotification(id, by_user);
    return;
  }

  if (FindVisibleNotificationById(id) == NULL)
    return;

  // In many cases |id| is a reference to an existing notification instance
  // but the instance can be destructed in RemoveNotification(). Hence
  // copies the id explicitly here.
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
  for (NotificationList::Notifications::const_iterator iter =
           notifications.begin(); iter != notifications.end(); ++iter) {
    RemoveNotification((*iter)->id(), false);
  }
  if (!notifications.empty()) {
    notification_cache_.Rebuild(
        notification_list_->GetVisibleNotifications(blockers_));
  }
}

void MessageCenterImpl::RemoveAllNotifications(bool by_user) {
  // Using not |blockers_| but an empty list since it wants to remove literally
  // all notifications.
  RemoveNotifications(by_user, NotificationBlockers());
}

void MessageCenterImpl::RemoveAllVisibleNotifications(bool by_user) {
  RemoveNotifications(by_user, blockers_);
}

void MessageCenterImpl::RemoveNotifications(
    bool by_user,
    const NotificationBlockers& blockers) {
  const NotificationList::Notifications notifications =
      notification_list_->GetVisibleNotifications(blockers);
  std::set<std::string> ids;
  for (NotificationList::Notifications::const_iterator iter =
           notifications.begin(); iter != notifications.end(); ++iter) {
    ids.insert((*iter)->id());
    scoped_refptr<NotificationDelegate> delegate = (*iter)->delegate();
    if (delegate.get())
      delegate->Close(by_user);
    notification_list_->RemoveNotification((*iter)->id());
  }

  if (!ids.empty()) {
    notification_cache_.Rebuild(
        notification_list_->GetVisibleNotifications(blockers_));
  }
  for (std::set<std::string>::const_iterator iter = ids.begin();
       iter != ids.end(); ++iter) {
    FOR_EACH_OBSERVER(MessageCenterObserver,
                      observer_list_,
                      OnNotificationRemoved(*iter, by_user));
  }
}

void MessageCenterImpl::SetNotificationIcon(const std::string& notification_id,
                                        const gfx::Image& image) {
  bool updated = false;
  Notification* queue_notification = notification_queue_->GetLatestNotification(
      notification_id);

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
  Notification* queue_notification = notification_queue_->GetLatestNotification(
      notification_id);

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
  Notification* queue_notification = notification_queue_->GetLatestNotification(
      notification_id);

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
  if (HasPopupNotifications())
    MarkSinglePopupAsShown(id, true);
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
  if (HasPopupNotifications())
    MarkSinglePopupAsShown(id, true);
  scoped_refptr<NotificationDelegate> delegate =
      notification_list_->GetNotificationDelegate(id);
  if (delegate.get())
    delegate->ButtonClick(button_index);
  FOR_EACH_OBSERVER(
      MessageCenterObserver, observer_list_, OnNotificationButtonClicked(
          id, button_index));
}

void MessageCenterImpl::MarkSinglePopupAsShown(const std::string& id,
                                               bool mark_notification_as_read) {
  if (FindVisibleNotificationById(id) == NULL)
    return;
  notification_list_->MarkSinglePopupAsShown(id, mark_notification_as_read);
  notification_cache_.RecountUnread();
  FOR_EACH_OBSERVER(
      MessageCenterObserver, observer_list_, OnNotificationUpdated(id));
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

void MessageCenterImpl::EnterQuietModeWithExpire(
    const base::TimeDelta& expires_in) {
  if (quiet_mode_timer_.get()) {
    // Note that the capital Reset() is the method to restart the timer, not
    // scoped_ptr::reset().
    quiet_mode_timer_->Reset();
  } else {
    notification_list_->SetQuietMode(true);
    FOR_EACH_OBSERVER(
        MessageCenterObserver, observer_list_, OnQuietModeChanged(true));

    quiet_mode_timer_.reset(new base::OneShotTimer<MessageCenterImpl>);
    quiet_mode_timer_->Start(
        FROM_HERE,
        expires_in,
        base::Bind(
            &MessageCenterImpl::SetQuietMode, base::Unretained(this), false));
  }
}

void MessageCenterImpl::RestartPopupTimers() {
  if (popup_timers_controller_.get())
    popup_timers_controller_->StartAll();
}

void MessageCenterImpl::PausePopupTimers() {
  if (popup_timers_controller_.get())
    popup_timers_controller_->PauseAll();
}

void MessageCenterImpl::DisableTimersForTest() {
  popup_timers_controller_.reset();
}

}  // namespace message_center
