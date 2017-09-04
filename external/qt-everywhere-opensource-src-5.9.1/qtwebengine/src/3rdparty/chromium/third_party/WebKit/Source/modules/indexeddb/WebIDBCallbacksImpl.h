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

#ifndef WebIDBCallbacksImpl_h
#define WebIDBCallbacksImpl_h

#include "public/platform/modules/indexeddb/WebIDBCallbacks.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include <memory>

namespace blink {

class IDBRequest;
class WebIDBCursor;
class WebIDBDatabase;
class WebIDBDatabaseError;
class WebIDBKey;
struct WebIDBMetadata;
struct WebIDBValue;

class WebIDBCallbacksImpl final : public WebIDBCallbacks {
  USING_FAST_MALLOC(WebIDBCallbacksImpl);

 public:
  static std::unique_ptr<WebIDBCallbacksImpl> create(IDBRequest*);

  ~WebIDBCallbacksImpl() override;

  // Pointers transfer ownership.
  void onError(const WebIDBDatabaseError&) override;
  void onSuccess(const WebVector<WebString>&) override;
  void onSuccess(WebIDBCursor*,
                 const WebIDBKey&,
                 const WebIDBKey& primaryKey,
                 const WebIDBValue&) override;
  void onSuccess(WebIDBDatabase*, const WebIDBMetadata&) override;
  void onSuccess(const WebIDBKey&) override;
  void onSuccess(const WebIDBValue&) override;
  void onSuccess(const WebVector<WebIDBValue>&) override;
  void onSuccess(long long) override;
  void onSuccess() override;
  void onSuccess(const WebIDBKey&,
                 const WebIDBKey& primaryKey,
                 const WebIDBValue&) override;
  void onBlocked(long long oldVersion) override;
  void onUpgradeNeeded(long long oldVersion,
                       WebIDBDatabase*,
                       const WebIDBMetadata&,
                       unsigned short dataLoss,
                       WebString dataLossMessage) override;
  void detach() override;

 private:
  explicit WebIDBCallbacksImpl(IDBRequest*);

  Persistent<IDBRequest> m_request;
};

}  // namespace blink

#endif  // WebIDBCallbacksImpl_h
