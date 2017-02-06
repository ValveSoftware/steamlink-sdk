// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/dom_storage_session.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/tracked_objects.h"
#include "content/browser/dom_storage/dom_storage_context_impl.h"
#include "content/browser/dom_storage/dom_storage_task_runner.h"

namespace content {

DOMStorageSession::DOMStorageSession(DOMStorageContextImpl* context)
    : context_(context),
      namespace_id_(context->AllocateSessionId()),
      persistent_namespace_id_(context->AllocatePersistentSessionId()),
      should_persist_(false) {
  context->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&DOMStorageContextImpl::CreateSessionNamespace,
                 context_, namespace_id_, persistent_namespace_id_));
}

DOMStorageSession::DOMStorageSession(DOMStorageContextImpl* context,
                                     const std::string& persistent_namespace_id)
    : context_(context),
      namespace_id_(context->AllocateSessionId()),
      persistent_namespace_id_(persistent_namespace_id),
      should_persist_(false) {
  context->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&DOMStorageContextImpl::CreateSessionNamespace,
                 context_, namespace_id_, persistent_namespace_id_));
}

void DOMStorageSession::SetShouldPersist(bool should_persist) {
  should_persist_ = should_persist;
}

bool DOMStorageSession::should_persist() const {
  return should_persist_;
}

bool DOMStorageSession::IsFromContext(DOMStorageContextImpl* context) {
  return context_.get() == context;
}

DOMStorageSession* DOMStorageSession::Clone() {
  return CloneFrom(context_.get(), namespace_id_);
}

// static
DOMStorageSession* DOMStorageSession::CloneFrom(DOMStorageContextImpl* context,
                                                int64_t namepace_id_to_clone) {
  int64_t clone_id = context->AllocateSessionId();
  std::string persistent_clone_id = context->AllocatePersistentSessionId();
  context->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&DOMStorageContextImpl::CloneSessionNamespace,
                 context, namepace_id_to_clone, clone_id, persistent_clone_id));
  return new DOMStorageSession(context, clone_id, persistent_clone_id);
}

DOMStorageSession::DOMStorageSession(DOMStorageContextImpl* context,
                                     int64_t namespace_id,
                                     const std::string& persistent_namespace_id)
    : context_(context),
      namespace_id_(namespace_id),
      persistent_namespace_id_(persistent_namespace_id),
      should_persist_(false) {
  // This ctor is intended for use by the Clone() method.
}

DOMStorageSession::~DOMStorageSession() {
  context_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&DOMStorageContextImpl::DeleteSessionNamespace,
                 context_, namespace_id_, should_persist_));
}

}  // namespace content
