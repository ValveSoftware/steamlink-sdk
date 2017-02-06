// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_DB_MESSAGE_FILTER_H_
#define CONTENT_CHILD_DB_MESSAGE_FILTER_H_

#include <stdint.h>

#include "base/strings/string16.h"
#include "ipc/message_filter.h"

namespace url {
class Origin;
}  // namespace url

namespace content {

// Receives database messages from the browser process and processes them on the
// IO thread.
class DBMessageFilter : public IPC::MessageFilter {
 public:
  DBMessageFilter();

  // IPC::MessageFilter
  bool OnMessageReceived(const IPC::Message& message) override;

 protected:
  ~DBMessageFilter() override {}

 private:
  void OnDatabaseUpdateSize(const url::Origin& origin,
                            const base::string16& database_name,
                            int64_t database_size);
  void OnDatabaseUpdateSpaceAvailable(const url::Origin& origin,
                                      int64_t space_available);
  void OnDatabaseResetSpaceAvailable(const url::Origin& origin);
  void OnDatabaseCloseImmediately(const url::Origin& origin,
                                  const base::string16& database_name);
};

}  // namespace content

#endif  // CONTENT_CHILD_DB_MESSAGE_FILTER_H_
