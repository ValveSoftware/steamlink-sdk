// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mime_registry_message_filter.h"

#include "content/common/mime_registry_messages.h"
#include "net/base/mime_util.h"

namespace content {

MimeRegistryMessageFilter::MimeRegistryMessageFilter()
    : BrowserMessageFilter(MimeRegistryMsgStart) {
}

MimeRegistryMessageFilter::~MimeRegistryMessageFilter() {
}

void MimeRegistryMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message,
    BrowserThread::ID* thread) {
  if (IPC_MESSAGE_CLASS(message) == MimeRegistryMsgStart)
    *thread = BrowserThread::FILE;
}

bool MimeRegistryMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MimeRegistryMessageFilter, message)
    IPC_MESSAGE_HANDLER(MimeRegistryMsg_GetMimeTypeFromExtension,
                        OnGetMimeTypeFromExtension)
    IPC_MESSAGE_HANDLER(MimeRegistryMsg_GetMimeTypeFromFile,
                        OnGetMimeTypeFromFile)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MimeRegistryMessageFilter::OnGetMimeTypeFromExtension(
    const base::FilePath::StringType& ext, std::string* mime_type) {
  net::GetMimeTypeFromExtension(ext, mime_type);
}

void MimeRegistryMessageFilter::OnGetMimeTypeFromFile(
    const base::FilePath& file_path, std::string* mime_type) {
  net::GetMimeTypeFromFile(file_path, mime_type);
}

}  // namespace content
