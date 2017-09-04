// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_INDEXED_DB_WEBIDBFACTORY_IMPL_H_
#define CONTENT_CHILD_INDEXED_DB_WEBIDBFACTORY_IMPL_H_

#include "base/memory/ref_counted.h"
#include "content/common/indexed_db/indexed_db.mojom.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBCallbacks.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBDatabaseCallbacks.h"
#include "third_party/WebKit/public/platform/modules/indexeddb/WebIDBFactory.h"

namespace blink {
class WebSecurityOrigin;
class WebString;
}

namespace IPC {
class SyncMessageFilter;
}

namespace content {
class ThreadSafeSender;

class WebIDBFactoryImpl : public blink::WebIDBFactory {
 public:
  WebIDBFactoryImpl(scoped_refptr<IPC::SyncMessageFilter> sync_message_filter,
                    scoped_refptr<ThreadSafeSender> thread_safe_sender,
                    scoped_refptr<base::SingleThreadTaskRunner> io_runner);
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
  class IOThreadHelper;

  IOThreadHelper* io_helper_;
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  scoped_refptr<base::SingleThreadTaskRunner> io_runner_;
};

}  // namespace content

#endif  // CONTENT_CHILD_INDEXED_DB_WEBIDBFACTORY_IMPL_H_
