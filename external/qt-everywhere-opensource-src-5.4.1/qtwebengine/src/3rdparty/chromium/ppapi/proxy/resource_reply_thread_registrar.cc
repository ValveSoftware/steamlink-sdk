// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/resource_reply_thread_registrar.h"

#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/tracked_callback.h"

namespace ppapi {
namespace proxy {

ResourceReplyThreadRegistrar::ResourceReplyThreadRegistrar(
    scoped_refptr<base::MessageLoopProxy> default_thread)
    : default_thread_(default_thread) {
}

ResourceReplyThreadRegistrar::~ResourceReplyThreadRegistrar() {
}

void ResourceReplyThreadRegistrar::Register(
    PP_Resource resource,
    int32_t sequence_number,
    scoped_refptr<TrackedCallback> reply_thread_hint) {
  ProxyLock::AssertAcquiredDebugOnly();

  // Use the default thread if |reply_thread_hint| is NULL or blocking.
  if (!reply_thread_hint || reply_thread_hint->is_blocking())
    return;

  DCHECK(reply_thread_hint->target_loop());
  scoped_refptr<base::MessageLoopProxy> reply_thread(
      reply_thread_hint->target_loop()->GetMessageLoopProxy());
  {
    base::AutoLock auto_lock(lock_);

    if (reply_thread == default_thread_)
      return;

    map_[resource][sequence_number] = reply_thread;
  }
}

void ResourceReplyThreadRegistrar::Unregister(PP_Resource resource) {
  base::AutoLock auto_lock(lock_);
  map_.erase(resource);
}

scoped_refptr<base::MessageLoopProxy>
ResourceReplyThreadRegistrar::GetTargetThreadAndUnregister(
    PP_Resource resource,
    int32_t sequence_number) {
  base::AutoLock auto_lock(lock_);
  ResourceMap::iterator resource_iter = map_.find(resource);
  if (resource_iter == map_.end())
    return default_thread_;

  SequenceNumberMap::iterator sequence_number_iter =
      resource_iter->second.find(sequence_number);
  if (sequence_number_iter == resource_iter->second.end())
    return default_thread_;

  scoped_refptr<base::MessageLoopProxy> target = sequence_number_iter->second;
  resource_iter->second.erase(sequence_number_iter);
  return target;
}

}  // namespace proxy
}  // namespace ppapi
