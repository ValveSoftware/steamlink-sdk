// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/session_storage_namespace_impl.h"

#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/dom_storage/dom_storage_session.h"

namespace content {

SessionStorageNamespaceImpl::SessionStorageNamespaceImpl(
    DOMStorageContextWrapper* context)
    : session_(new DOMStorageSession(context->context())) {
}

SessionStorageNamespaceImpl::SessionStorageNamespaceImpl(
    DOMStorageContextWrapper* context, int64 namepace_id_to_clone)
    : session_(DOMStorageSession::CloneFrom(context->context(),
                                            namepace_id_to_clone)) {
}

SessionStorageNamespaceImpl::SessionStorageNamespaceImpl(
    DOMStorageContextWrapper* context, const std::string& persistent_id)
    : session_(new DOMStorageSession(context->context(), persistent_id)) {
}

SessionStorageNamespaceImpl::SessionStorageNamespaceImpl(
    SessionStorageNamespaceImpl* master_session_storage_namespace)
    : session_(new DOMStorageSession(
          master_session_storage_namespace->session_)) {
}


int64 SessionStorageNamespaceImpl::id() const {
  return session_->namespace_id();
}

const std::string& SessionStorageNamespaceImpl::persistent_id() const {
  return session_->persistent_namespace_id();
}

void SessionStorageNamespaceImpl::SetShouldPersist(bool should_persist) {
  session_->SetShouldPersist(should_persist);
}

bool SessionStorageNamespaceImpl::should_persist() const {
  return session_->should_persist();
}

SessionStorageNamespaceImpl* SessionStorageNamespaceImpl::Clone() {
  return new SessionStorageNamespaceImpl(session_->Clone());
}

bool SessionStorageNamespaceImpl::IsFromContext(
    DOMStorageContextWrapper* context) {
  return session_->IsFromContext(context->context());
}

SessionStorageNamespaceImpl::SessionStorageNamespaceImpl(
    DOMStorageSession* clone)
    : session_(clone) {
}

SessionStorageNamespaceImpl::~SessionStorageNamespaceImpl() {
}

void SessionStorageNamespaceImpl::AddTransactionLogProcessId(int process_id) {
  session_->AddTransactionLogProcessId(process_id);
}

void SessionStorageNamespaceImpl::RemoveTransactionLogProcessId(
    int process_id) {
  session_->RemoveTransactionLogProcessId(process_id);
}

void SessionStorageNamespaceImpl::Merge(
    bool actually_merge,
    int process_id,
    SessionStorageNamespace* other,
    const MergeResultCallback& callback) {
  SessionStorageNamespaceImpl* other_impl =
      static_cast<SessionStorageNamespaceImpl*>(other);
  session_->Merge(actually_merge, process_id, other_impl->session_, callback);
}

bool SessionStorageNamespaceImpl::IsAliasOf(SessionStorageNamespace* other) {
  return persistent_id() == other->persistent_id();
}

SessionStorageNamespace* SessionStorageNamespaceImpl::CreateAlias() {
  return new SessionStorageNamespaceImpl(this);
}

}  // namespace content
