// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/listener/non_blocking_push_client.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "jingle/notifier/listener/push_client_observer.h"

namespace notifier {

// All methods are called on the delegate thread unless specified
// otherwise.
class NonBlockingPushClient::Core
    : public base::RefCountedThreadSafe<NonBlockingPushClient::Core>,
      public PushClientObserver {
 public:
  // Called on the parent thread.
  explicit Core(
      const scoped_refptr<base::SingleThreadTaskRunner>&
          delegate_task_runner,
      const base::WeakPtr<NonBlockingPushClient>& parent_push_client);

  // Must be called after being created.
  //
  // This is separated out from the constructor since posting tasks
  // from the constructor is dangerous.
  void CreateOnDelegateThread(
      const CreateBlockingPushClientCallback&
          create_blocking_push_client_callback);

  // Must be called before being destroyed.
  void DestroyOnDelegateThread();

  void UpdateSubscriptions(const SubscriptionList& subscriptions);
  void UpdateCredentials(const std::string& email, const std::string& token);
  void SendNotification(const Notification& data);
  void SendPing();

  virtual void OnNotificationsEnabled() OVERRIDE;
  virtual void OnNotificationsDisabled(
      NotificationsDisabledReason reason) OVERRIDE;
  virtual void OnIncomingNotification(
      const Notification& notification) OVERRIDE;
  virtual void OnPingResponse() OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<NonBlockingPushClient::Core>;

  // Called on either the parent thread or the delegate thread.
  virtual ~Core();

  const scoped_refptr<base::SingleThreadTaskRunner> parent_task_runner_;
  const scoped_refptr<base::SingleThreadTaskRunner> delegate_task_runner_;

  const base::WeakPtr<NonBlockingPushClient> parent_push_client_;
  scoped_ptr<PushClient> delegate_push_client_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

NonBlockingPushClient::Core::Core(
    const scoped_refptr<base::SingleThreadTaskRunner>& delegate_task_runner,
    const base::WeakPtr<NonBlockingPushClient>& parent_push_client)
    : parent_task_runner_(base::MessageLoopProxy::current()),
      delegate_task_runner_(delegate_task_runner),
      parent_push_client_(parent_push_client) {}

NonBlockingPushClient::Core::~Core() {
  DCHECK(parent_task_runner_->BelongsToCurrentThread() ||
         delegate_task_runner_->BelongsToCurrentThread());
  DCHECK(!delegate_push_client_.get());
}

void NonBlockingPushClient::Core::CreateOnDelegateThread(
    const CreateBlockingPushClientCallback&
        create_blocking_push_client_callback) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
  DCHECK(!delegate_push_client_.get());
  delegate_push_client_ = create_blocking_push_client_callback.Run();
  delegate_push_client_->AddObserver(this);
}

void NonBlockingPushClient::Core::DestroyOnDelegateThread() {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
  DCHECK(delegate_push_client_.get());
  delegate_push_client_->RemoveObserver(this);
  delegate_push_client_.reset();
}

void NonBlockingPushClient::Core::UpdateSubscriptions(
    const SubscriptionList& subscriptions) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
  DCHECK(delegate_push_client_.get());
  delegate_push_client_->UpdateSubscriptions(subscriptions);
}

void NonBlockingPushClient::Core::UpdateCredentials(
      const std::string& email, const std::string& token) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
  DCHECK(delegate_push_client_.get());
  delegate_push_client_->UpdateCredentials(email, token);
}

void NonBlockingPushClient::Core::SendNotification(
    const Notification& notification) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
  DCHECK(delegate_push_client_.get());
  delegate_push_client_->SendNotification(notification);
}

void NonBlockingPushClient::Core::SendPing() {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
  DCHECK(delegate_push_client_.get());
  delegate_push_client_->SendPing();
}

void NonBlockingPushClient::Core::OnNotificationsEnabled() {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
  parent_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&NonBlockingPushClient::OnNotificationsEnabled,
                 parent_push_client_));
}

void NonBlockingPushClient::Core::OnNotificationsDisabled(
    NotificationsDisabledReason reason) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
  parent_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&NonBlockingPushClient::OnNotificationsDisabled,
                 parent_push_client_, reason));
}

void NonBlockingPushClient::Core::OnIncomingNotification(
    const Notification& notification) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
  parent_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&NonBlockingPushClient::OnIncomingNotification,
                 parent_push_client_, notification));
}

void NonBlockingPushClient::Core::OnPingResponse() {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
  parent_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&NonBlockingPushClient::OnPingResponse,
                 parent_push_client_));
}

NonBlockingPushClient::NonBlockingPushClient(
    const scoped_refptr<base::SingleThreadTaskRunner>& delegate_task_runner,
    const CreateBlockingPushClientCallback&
        create_blocking_push_client_callback)
    : delegate_task_runner_(delegate_task_runner),
      weak_ptr_factory_(this) {
  core_ = new Core(delegate_task_runner_, weak_ptr_factory_.GetWeakPtr());
  delegate_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&NonBlockingPushClient::Core::CreateOnDelegateThread,
                 core_.get(), create_blocking_push_client_callback));
}

NonBlockingPushClient::~NonBlockingPushClient() {
  DCHECK(thread_checker_.CalledOnValidThread());
  delegate_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&NonBlockingPushClient::Core::DestroyOnDelegateThread,
                 core_.get()));
}

void NonBlockingPushClient::AddObserver(PushClientObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.AddObserver(observer);
}

void NonBlockingPushClient::RemoveObserver(PushClientObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.RemoveObserver(observer);
}

void NonBlockingPushClient::UpdateSubscriptions(
    const SubscriptionList& subscriptions) {
  DCHECK(thread_checker_.CalledOnValidThread());
  delegate_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&NonBlockingPushClient::Core::UpdateSubscriptions,
                 core_.get(), subscriptions));
}

void NonBlockingPushClient::UpdateCredentials(
    const std::string& email, const std::string& token) {
  DCHECK(thread_checker_.CalledOnValidThread());
  delegate_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&NonBlockingPushClient::Core::UpdateCredentials,
                 core_.get(), email, token));
}

void NonBlockingPushClient::SendNotification(
    const Notification& notification) {
  DCHECK(thread_checker_.CalledOnValidThread());
  delegate_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&NonBlockingPushClient::Core::SendNotification, core_.get(),
                 notification));
}

void NonBlockingPushClient::SendPing() {
  DCHECK(thread_checker_.CalledOnValidThread());
  delegate_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&NonBlockingPushClient::Core::SendPing, core_.get()));
}

void NonBlockingPushClient::OnNotificationsEnabled() {
  DCHECK(thread_checker_.CalledOnValidThread());
  FOR_EACH_OBSERVER(PushClientObserver, observers_,
                    OnNotificationsEnabled());
}

void NonBlockingPushClient::OnNotificationsDisabled(
    NotificationsDisabledReason reason) {
  DCHECK(thread_checker_.CalledOnValidThread());
  FOR_EACH_OBSERVER(PushClientObserver, observers_,
                    OnNotificationsDisabled(reason));
}

void NonBlockingPushClient::OnIncomingNotification(
    const Notification& notification) {
  DCHECK(thread_checker_.CalledOnValidThread());
  FOR_EACH_OBSERVER(PushClientObserver, observers_,
                    OnIncomingNotification(notification));
}

void NonBlockingPushClient::OnPingResponse() {
  DCHECK(thread_checker_.CalledOnValidThread());
  FOR_EACH_OBSERVER(PushClientObserver, observers_, OnPingResponse());
}

}  // namespace notifier
