/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. AND ITS CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE INC.
 * OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/indexeddb/WebIDBCallbacksImpl.h"

#include "core/dom/DOMException.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "modules/IndexedDBNames.h"
#include "modules/indexeddb/IDBMetadata.h"
#include "modules/indexeddb/IDBRequest.h"
#include "modules/indexeddb/IDBValue.h"
#include "platform/SharedBuffer.h"
#include "public/platform/modules/indexeddb/WebIDBCursor.h"
#include "public/platform/modules/indexeddb/WebIDBDatabase.h"
#include "public/platform/modules/indexeddb/WebIDBDatabaseError.h"
#include "public/platform/modules/indexeddb/WebIDBKey.h"
#include "public/platform/modules/indexeddb/WebIDBValue.h"
#include "wtf/PtrUtil.h"
#include <memory>

using blink::WebIDBCursor;
using blink::WebIDBDatabase;
using blink::WebIDBDatabaseError;
using blink::WebIDBKey;
using blink::WebIDBKeyPath;
using blink::WebIDBMetadata;
using blink::WebIDBValue;
using blink::WebVector;

namespace blink {

// static
std::unique_ptr<WebIDBCallbacksImpl> WebIDBCallbacksImpl::create(
    IDBRequest* request) {
  return wrapUnique(new WebIDBCallbacksImpl(request));
}

WebIDBCallbacksImpl::WebIDBCallbacksImpl(IDBRequest* request)
    : m_request(request) {
  InspectorInstrumentation::asyncTaskScheduled(
      m_request->getExecutionContext(), IndexedDBNames::IndexedDB, this, true);
}

WebIDBCallbacksImpl::~WebIDBCallbacksImpl() {
  if (m_request) {
    InspectorInstrumentation::asyncTaskCanceled(
        m_request->getExecutionContext(), this);
    m_request->webCallbacksDestroyed();
  }
}

void WebIDBCallbacksImpl::onError(const WebIDBDatabaseError& error) {
  if (!m_request)
    return;

  InspectorInstrumentation::AsyncTask asyncTask(
      m_request->getExecutionContext(), this);
  m_request->onError(DOMException::create(error.code(), error.message()));
}

void WebIDBCallbacksImpl::onSuccess(const WebVector<WebString>& webStringList) {
  if (!m_request)
    return;

  Vector<String> stringList;
  for (size_t i = 0; i < webStringList.size(); ++i)
    stringList.append(webStringList[i]);
  InspectorInstrumentation::AsyncTask asyncTask(
      m_request->getExecutionContext(), this);
  m_request->onSuccess(stringList);
}

void WebIDBCallbacksImpl::onSuccess(WebIDBCursor* cursor,
                                    const WebIDBKey& key,
                                    const WebIDBKey& primaryKey,
                                    const WebIDBValue& value) {
  if (!m_request)
    return;

  InspectorInstrumentation::AsyncTask asyncTask(
      m_request->getExecutionContext(), this);
  m_request->onSuccess(wrapUnique(cursor), key, primaryKey,
                       IDBValue::create(value));
}

void WebIDBCallbacksImpl::onSuccess(WebIDBDatabase* backend,
                                    const WebIDBMetadata& metadata) {
  std::unique_ptr<WebIDBDatabase> db = wrapUnique(backend);
  if (m_request) {
    InspectorInstrumentation::AsyncTask asyncTask(
        m_request->getExecutionContext(), this);
    m_request->onSuccess(std::move(db), IDBDatabaseMetadata(metadata));
  } else if (db) {
    db->close();
  }
}

void WebIDBCallbacksImpl::onSuccess(const WebIDBKey& key) {
  if (!m_request)
    return;

  InspectorInstrumentation::AsyncTask asyncTask(
      m_request->getExecutionContext(), this);
  m_request->onSuccess(key);
}

void WebIDBCallbacksImpl::onSuccess(const WebIDBValue& value) {
  if (!m_request)
    return;

  InspectorInstrumentation::AsyncTask asyncTask(
      m_request->getExecutionContext(), this);
  m_request->onSuccess(IDBValue::create(value));
}

void WebIDBCallbacksImpl::onSuccess(const WebVector<WebIDBValue>& values) {
  if (!m_request)
    return;

  InspectorInstrumentation::AsyncTask asyncTask(
      m_request->getExecutionContext(), this);
  Vector<RefPtr<IDBValue>> idbValues(values.size());
  for (size_t i = 0; i < values.size(); ++i)
    idbValues[i] = IDBValue::create(values[i]);
  m_request->onSuccess(idbValues);
}

void WebIDBCallbacksImpl::onSuccess(long long value) {
  if (!m_request)
    return;

  InspectorInstrumentation::AsyncTask asyncTask(
      m_request->getExecutionContext(), this);
  m_request->onSuccess(value);
}

void WebIDBCallbacksImpl::onSuccess() {
  if (!m_request)
    return;

  InspectorInstrumentation::AsyncTask asyncTask(
      m_request->getExecutionContext(), this);
  m_request->onSuccess();
}

void WebIDBCallbacksImpl::onSuccess(const WebIDBKey& key,
                                    const WebIDBKey& primaryKey,
                                    const WebIDBValue& value) {
  if (!m_request)
    return;

  InspectorInstrumentation::AsyncTask asyncTask(
      m_request->getExecutionContext(), this);
  m_request->onSuccess(key, primaryKey, IDBValue::create(value));
}

void WebIDBCallbacksImpl::onBlocked(long long oldVersion) {
  if (!m_request)
    return;

  InspectorInstrumentation::AsyncTask asyncTask(
      m_request->getExecutionContext(), this);
  m_request->onBlocked(oldVersion);
}

void WebIDBCallbacksImpl::onUpgradeNeeded(long long oldVersion,
                                          WebIDBDatabase* database,
                                          const WebIDBMetadata& metadata,
                                          unsigned short dataLoss,
                                          WebString dataLossMessage) {
  std::unique_ptr<WebIDBDatabase> db = wrapUnique(database);
  if (m_request) {
    InspectorInstrumentation::AsyncTask asyncTask(
        m_request->getExecutionContext(), this);
    m_request->onUpgradeNeeded(
        oldVersion, std::move(db), IDBDatabaseMetadata(metadata),
        static_cast<WebIDBDataLoss>(dataLoss), dataLossMessage);
  } else {
    db->close();
  }
}

void WebIDBCallbacksImpl::detach() {
  m_request.clear();
}

}  // namespace blink
