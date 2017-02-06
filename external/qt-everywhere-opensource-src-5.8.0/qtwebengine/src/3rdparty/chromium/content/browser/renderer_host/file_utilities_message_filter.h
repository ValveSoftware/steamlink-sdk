// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_FILE_UTILITIES_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_FILE_UTILITIES_MESSAGE_FILTER_H_

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "content/public/browser/browser_message_filter.h"

namespace IPC {
class Message;
}

namespace content {

class FileUtilitiesMessageFilter : public BrowserMessageFilter {
 public:
  explicit FileUtilitiesMessageFilter(int process_id);

  // BrowserMessageFilter implementation.
  void OverrideThreadForMessage(const IPC::Message& message,
                                BrowserThread::ID* thread) override;
  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  ~FileUtilitiesMessageFilter() override;

  typedef void (*FileInfoWriteFunc)(IPC::Message* reply_msg,
                                    const base::File::Info& file_info);

  void OnGetFileInfo(const base::FilePath& path,
                     base::File::Info* result,
                     base::File::Error* status);

  // The ID of this process.
  int process_id_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(FileUtilitiesMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_FILE_UTILITIES_MESSAGE_FILTER_H_
