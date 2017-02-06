// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_INDEXED_DB_WEBIDBFACTORY_IMPL_H_
#define CONTENT_CHILD_INDEXED_DB_WEBIDBFACTORY_IMPL_H_

#include "base/memory/ref_counted.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBCallbacks.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBDatabaseCallbacks.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBFactory.h"

namespace blink {
class WebSecurityOrigin;
class WebString;
}

namespace content {
class ThreadSafeSender;

class WebIDBFactoryImpl : public blink::WebIDBFactory {
 public:
  explicit WebIDBFactoryImpl(ThreadSafeSender* thread_safe_sender);
  ~WebIDBFactoryImpl() override;

  // See WebIDBFactory.h for documentation on these functions.
  void getDatabaseNames(blink::WebIDBCallbacks* callbacks,
                        const blink::WebSecurityOrigin& origin) override;
  void open(const blink::WebString& name,
            long long version,
            long long transaction_id,
            blink::WebIDBCallbacks* callbacks,
            blink::WebIDBDatabaseCallbacks* databaseCallbacks,
            const blink::WebSecurityOrigin& origin) override;
  void deleteDatabase(const blink::WebString& name,
                      blink::WebIDBCallbacks* callbacks,
                      const blink::WebSecurityOrigin& origin) override;

 private:
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
};

}  // namespace content

#endif  // CONTENT_CHILD_INDEXED_DB_WEBIDBFACTORY_IMPL_H_
