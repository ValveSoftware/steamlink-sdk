/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebIDBCallbacks_h
#define WebIDBCallbacks_h

#include "public/platform/WebCommon.h"
#include "public/platform/WebString.h"
#include "public/platform/WebVector.h"

namespace blink {

class WebIDBCursor;
class WebIDBDatabase;
class WebIDBDatabaseError;
class WebIDBKey;
struct WebIDBMetadata;
struct WebIDBValue;

class WebIDBCallbacks {
 public:
  virtual ~WebIDBCallbacks() {}

  // Pointers transfer ownership.
  virtual void onError(const WebIDBDatabaseError&) = 0;
  virtual void onSuccess(const WebVector<WebString>&) = 0;
  virtual void onSuccess(WebIDBCursor*,
                         const WebIDBKey&,
                         const WebIDBKey& primaryKey,
                         const WebIDBValue&) = 0;
  virtual void onSuccess(WebIDBDatabase*, const WebIDBMetadata&) = 0;
  virtual void onSuccess(const WebIDBKey&) = 0;
  virtual void onSuccess(const WebIDBValue&) = 0;
  virtual void onSuccess(const WebVector<WebIDBValue>&) = 0;
  virtual void onSuccess(long long) = 0;
  virtual void onSuccess() = 0;
  virtual void onSuccess(const WebIDBKey&,
                         const WebIDBKey& primaryKey,
                         const WebIDBValue&) = 0;
  virtual void onBlocked(long long oldVersion) = 0;
  virtual void onUpgradeNeeded(long long oldVersion,
                               WebIDBDatabase*,
                               const WebIDBMetadata&,
                               unsigned short dataLoss,
                               WebString dataLossMessage) = 0;
  virtual void detach() = 0;
};

}  // namespace blink

#endif  // WebIDBCallbacks_h
