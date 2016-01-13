// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_DB_MESSAGE_FILTER_H_
#define CONTENT_CHILD_DB_MESSAGE_FILTER_H_

#include "base/strings/string16.h"
#include "ipc/message_filter.h"

namespace content {

// Receives database messages from the browser process and processes them on the
// IO thread.
class DBMessageFilter : public IPC::MessageFilter {
 public:
  DBMessageFilter();

  // IPC::MessageFilter
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 protected:
  virtual ~DBMessageFilter() {}

 private:
  void OnDatabaseUpdateSize(const std::string& origin_identifier,
                            const base::string16& database_name,
                            int64 database_size);
  void OnDatabaseUpdateSpaceAvailable(const std::string& origin_identifier,
                                      int64 space_available);
  void OnDatabaseResetSpaceAvailable(const std::string& origin_identifier);
  void OnDatabaseCloseImmediately(const std::string& origin_identifier,
                                  const base::string16& database_name);
};

}  // namespace content

#endif  // CONTENT_CHILD_DB_MESSAGE_FILTER_H_
