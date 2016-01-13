// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/dom_storage_session.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/tracked_objects.h"
#include "content/browser/dom_storage/dom_storage_context_impl.h"
#include "content/browser/dom_storage/dom_storage_task_runner.h"

namespace content {

namespace {

void PostMergeTaskResult(
    const SessionStorageNamespace::MergeResultCallback& callback,
    SessionStorageNamespace::MergeResult result) {
  callback.Run(result);
}

void RunMergeTaskAndPostResult(
    const base::Callback<SessionStorageNamespace::MergeResult(void)>& task,
    scoped_refptr<base::SingleThreadTaskRunner> result_loop,
    const SessionStorageNamespace::MergeResultCallback& callback) {
  SessionStorageNamespace::MergeResult result = task.Run();
  result_loop->PostTask(
      FROM_HERE, base::Bind(&PostMergeTaskResult, callback, result));
}

}  // namespace

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

DOMStorageSession::DOMStorageSession(
    DOMStorageSession* master_dom_storage_session)
    : context_(master_dom_storage_session->context_),
      namespace_id_(master_dom_storage_session->context_->AllocateSessionId()),
      persistent_namespace_id_(
          master_dom_storage_session->persistent_namespace_id()),
      should_persist_(false) {
  context_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&DOMStorageContextImpl::CreateAliasSessionNamespace,
                 context_,
                 master_dom_storage_session->namespace_id(),
                 namespace_id_,
                 persistent_namespace_id_));
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
                                                int64 namepace_id_to_clone) {
  int64 clone_id = context->AllocateSessionId();
  std::string persistent_clone_id = context->AllocatePersistentSessionId();
  context->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&DOMStorageContextImpl::CloneSessionNamespace,
                 context, namepace_id_to_clone, clone_id, persistent_clone_id));
  return new DOMStorageSession(context, clone_id, persistent_clone_id);
}

DOMStorageSession::DOMStorageSession(DOMStorageContextImpl* context,
                                     int64 namespace_id,
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

void DOMStorageSession::AddTransactionLogProcessId(int process_id) {
  context_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&DOMStorageContextImpl::AddTransactionLogProcessId,
                 context_, namespace_id_, process_id));
}

void DOMStorageSession::RemoveTransactionLogProcessId(int process_id) {
  context_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&DOMStorageContextImpl::RemoveTransactionLogProcessId,
                 context_, namespace_id_, process_id));
}

void DOMStorageSession::Merge(
    bool actually_merge,
    int process_id,
    DOMStorageSession* other,
    const SessionStorageNamespace::MergeResultCallback& callback) {
  scoped_refptr<base::SingleThreadTaskRunner> current_loop(
      base::ThreadTaskRunnerHandle::Get());
  SessionStorageNamespace::MergeResultCallback cb =
      base::Bind(&DOMStorageSession::ProcessMergeResult,
                 this,
                 actually_merge,
                 callback,
                 other->persistent_namespace_id());
  context_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&RunMergeTaskAndPostResult,
                 base::Bind(&DOMStorageContextImpl::MergeSessionStorage,
                            context_, namespace_id_, actually_merge, process_id,
                            other->namespace_id_),
                 current_loop,
                 cb));
}

void DOMStorageSession::ProcessMergeResult(
    bool actually_merge,
    const SessionStorageNamespace::MergeResultCallback& callback,
    const std::string& new_persistent_namespace_id,
    SessionStorageNamespace::MergeResult result) {
  if (actually_merge &&
      (result == SessionStorageNamespace::MERGE_RESULT_MERGEABLE ||
       result == SessionStorageNamespace::MERGE_RESULT_NO_TRANSACTIONS)) {
    persistent_namespace_id_ = new_persistent_namespace_id;
  }
  callback.Run(result);
}

}  // namespace content
