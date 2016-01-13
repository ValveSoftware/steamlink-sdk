// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_INDEXED_DB_WEBIDBFACTORY_IMPL_H_
#define CONTENT_CHILD_INDEXED_DB_WEBIDBFACTORY_IMPL_H_

#include "base/memory/ref_counted.h"
#include "third_party/WebKit/public/platform/WebIDBCallbacks.h"
#include "third_party/WebKit/public/platform/WebIDBDatabaseCallbacks.h"
#include "third_party/WebKit/public/platform/WebIDBFactory.h"
#include "third_party/WebKit/public/platform/WebVector.h"

namespace blink {
class WebString;
}

namespace content {
class ThreadSafeSender;

class WebIDBFactoryImpl : public blink::WebIDBFactory {
 public:
  explicit WebIDBFactoryImpl(ThreadSafeSender* thread_safe_sender);
  virtual ~WebIDBFactoryImpl();

  // See WebIDBFactory.h for documentation on these functions.
  virtual void getDatabaseNames(blink::WebIDBCallbacks* callbacks,
                                const blink::WebString& database_identifier);
  virtual void open(const blink::WebString& name,
                    long long version,
                    long long transaction_id,
                    blink::WebIDBCallbacks* callbacks,
                    blink::WebIDBDatabaseCallbacks* databaseCallbacks,
                    const blink::WebString& database_identifier);
  virtual void deleteDatabase(const blink::WebString& name,
                              blink::WebIDBCallbacks* callbacks,
                              const blink::WebString& database_identifier);

 private:
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
};

}  // namespace content

#endif  // CONTENT_CHILD_INDEXED_DB_WEBIDBFACTORY_IMPL_H_
