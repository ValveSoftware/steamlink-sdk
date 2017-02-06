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
    DOMStorageContextWrapper* context,
    int64_t namepace_id_to_clone)
    : session_(DOMStorageSession::CloneFrom(context->context(),
                                            namepace_id_to_clone)) {}

SessionStorageNamespaceImpl::SessionStorageNamespaceImpl(
    DOMStorageContextWrapper* context, const std::string& persistent_id)
    : session_(new DOMStorageSession(context->context(), persistent_id)) {
}

int64_t SessionStorageNamespaceImpl::id() const {
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

}  // namespace content
