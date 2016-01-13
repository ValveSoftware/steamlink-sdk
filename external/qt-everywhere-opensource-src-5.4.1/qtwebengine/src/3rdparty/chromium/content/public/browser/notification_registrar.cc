// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/notification_registrar.h"

#include <algorithm>

#include "base/logging.h"
#include "content/browser/notification_service_impl.h"

namespace content {

struct NotificationRegistrar::Record {
  bool operator==(const Record& other) const;

  NotificationObserver* observer;
  int type;
  NotificationSource source;
};

bool NotificationRegistrar::Record::operator==(const Record& other) const {
  return observer == other.observer &&
         type == other.type &&
         source == other.source;
}

NotificationRegistrar::NotificationRegistrar() {
  // Force the NotificationService to be constructed (if it isn't already).
  // This ensures the NotificationService will be registered on the
  // AtExitManager before any objects which access it via NotificationRegistrar.
  // This in turn means it will be destroyed after these objects, so they will
  // never try to access the NotificationService after it's been destroyed.
  NotificationServiceImpl::current();
  // It is OK to create a NotificationRegistrar instance on one thread and then
  // use it (exclusively) on another, so we detach from the initial thread.
  DetachFromThread();
}

NotificationRegistrar::~NotificationRegistrar() {
  RemoveAll();
}

void NotificationRegistrar::Add(NotificationObserver* observer,
                                int type,
                                const NotificationSource& source) {
  DCHECK(CalledOnValidThread());
  DCHECK(!IsRegistered(observer, type, source)) << "Duplicate registration.";

  Record record = { observer, type, source };
  registered_.push_back(record);

  NotificationServiceImpl::current()->AddObserver(observer, type, source);
}

void NotificationRegistrar::Remove(NotificationObserver* observer,
                                   int type,
                                   const NotificationSource& source) {
  DCHECK(CalledOnValidThread());

  Record record = { observer, type, source };
  RecordVector::iterator found = std::find(
      registered_.begin(), registered_.end(), record);
  DCHECK(found != registered_.end());

  registered_.erase(found);

  // This can be NULL if our owner outlives the NotificationService, e.g. if our
  // owner is a Singleton.
  NotificationServiceImpl* service = NotificationServiceImpl::current();
  if (service)
    service->RemoveObserver(observer, type, source);
}

void NotificationRegistrar::RemoveAll() {
  CHECK(CalledOnValidThread());
  // Early-exit if no registrations, to avoid calling
  // NotificationService::current.  If we've constructed an object with a
  // NotificationRegistrar member, but haven't actually used the notification
  // service, and we reach prgram exit, then calling current() below could try
  // to initialize the service's lazy TLS pointer during exit, which throws
  // wrenches at things.
  if (registered_.empty())
    return;

  // This can be NULL if our owner outlives the NotificationService, e.g. if our
  // owner is a Singleton.
  NotificationServiceImpl* service = NotificationServiceImpl::current();
  if (service) {
    for (size_t i = 0; i < registered_.size(); i++) {
      service->RemoveObserver(registered_[i].observer,
                              registered_[i].type,
                              registered_[i].source);
    }
  }
  registered_.clear();
}

bool NotificationRegistrar::IsEmpty() const {
  DCHECK(CalledOnValidThread());
  return registered_.empty();
}

bool NotificationRegistrar::IsRegistered(NotificationObserver* observer,
                                         int type,
                                         const NotificationSource& source) {
  DCHECK(CalledOnValidThread());
  Record record = { observer, type, source };
  return std::find(registered_.begin(), registered_.end(), record) !=
      registered_.end();
}

}  // namespace content
