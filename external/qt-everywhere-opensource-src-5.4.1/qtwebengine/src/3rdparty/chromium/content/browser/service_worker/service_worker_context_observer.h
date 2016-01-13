// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_OBSERVER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_OBSERVER_H_

#include "base/strings/string16.h"
#include "url/gurl.h"

namespace content {

class ServiceWorkerContextObserver {
 public:
  struct ErrorInfo {
    ErrorInfo(const base::string16& message,
              int line,
              int column,
              const GURL& url)
        : error_message(message),
          line_number(line),
          column_number(column),
          source_url(url) {}
    const base::string16 error_message;
    const int line_number;
    const int column_number;
    const GURL source_url;
  };
  struct ConsoleMessage {
    ConsoleMessage(int source_identifier,
                   int message_level,
                   const base::string16& message,
                   int line_number,
                   const GURL& source_url)
        : source_identifier(source_identifier),
          message_level(message_level),
          message(message),
          line_number(line_number),
          source_url(source_url) {}
    const int source_identifier;
    const int message_level;
    const base::string16 message;
    const int line_number;
    const GURL source_url;
  };
  virtual void OnWorkerStarted(int64 version_id,
                               int process_id,
                               int thread_id) {}
  virtual void OnWorkerStopped(int64 version_id,
                               int process_id,
                               int thread_id) {}
  virtual void OnVersionStateChanged(int64 version_id) {}
  virtual void OnErrorReported(int64 version_id,
                               int process_id,
                               int thread_id,
                               const ErrorInfo& info) {}
  virtual void OnReportConsoleMessage(int64 version_id,
                                      int process_id,
                                      int thread_id,
                                      const ConsoleMessage& message) {}
  virtual void OnRegistrationStored(const GURL& pattern) {}
  virtual void OnRegistrationDeleted(const GURL& pattern) {}

 protected:
  virtual ~ServiceWorkerContextObserver() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_OBSERVER_H_
