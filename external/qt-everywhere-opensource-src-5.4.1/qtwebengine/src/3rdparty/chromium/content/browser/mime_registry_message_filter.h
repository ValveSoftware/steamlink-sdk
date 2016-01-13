// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MIME_REGISTRY_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_MIME_REGISTRY_MESSAGE_FILTER_H_

#include "base/files/file_path.h"
#include "content/public/browser/browser_message_filter.h"

namespace content {

class MimeRegistryMessageFilter : public BrowserMessageFilter {
 public:
  MimeRegistryMessageFilter();

  virtual void OverrideThreadForMessage(
      const IPC::Message& message,
      BrowserThread::ID* thread) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  virtual ~MimeRegistryMessageFilter();

  void OnGetMimeTypeFromExtension(const base::FilePath::StringType& ext,
                                  std::string* mime_type);
  void OnGetMimeTypeFromFile(const base::FilePath& file_path,
                             std::string* mime_type);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MIME_REGISTRY_MESSAGE_FILTER_H_
