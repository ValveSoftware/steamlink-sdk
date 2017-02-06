// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/indexed_db/webidbfactory_impl.h"

#include "content/child/indexed_db/indexed_db_dispatcher.h"
#include "content/child/storage_util.h"
#include "content/child/thread_safe_sender.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebString.h"

using blink::WebIDBCallbacks;
using blink::WebIDBDatabase;
using blink::WebIDBDatabaseCallbacks;
using blink::WebSecurityOrigin;
using blink::WebString;

namespace content {

WebIDBFactoryImpl::WebIDBFactoryImpl(ThreadSafeSender* thread_safe_sender)
    : thread_safe_sender_(thread_safe_sender) {}

WebIDBFactoryImpl::~WebIDBFactoryImpl() {}

void WebIDBFactoryImpl::getDatabaseNames(WebIDBCallbacks* callbacks,
                                         const WebSecurityOrigin& origin) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance(thread_safe_sender_.get());
  dispatcher->RequestIDBFactoryGetDatabaseNames(callbacks, origin);
}

void WebIDBFactoryImpl::open(const WebString& name,
                             long long version,
                             long long transaction_id,
                             WebIDBCallbacks* callbacks,
                             WebIDBDatabaseCallbacks* database_callbacks,
                             const WebSecurityOrigin& origin) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance(thread_safe_sender_.get());

  dispatcher->RequestIDBFactoryOpen(name, version, transaction_id, callbacks,
                                    database_callbacks, origin);
}

void WebIDBFactoryImpl::deleteDatabase(const WebString& name,
                                       WebIDBCallbacks* callbacks,
                                       const WebSecurityOrigin& origin) {
  IndexedDBDispatcher* dispatcher =
      IndexedDBDispatcher::ThreadSpecificInstance(thread_safe_sender_.get());
  dispatcher->RequestIDBFactoryDeleteDatabase(name, callbacks, origin);
}

}  // namespace content
