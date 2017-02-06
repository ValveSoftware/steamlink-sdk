// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/io_handler.h"

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted_delete_on_message_loop.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/devtools/devtools_io_context.h"
#include "content/public/browser/browser_thread.h"

namespace content {
namespace devtools {
namespace io {

using Response = DevToolsProtocolClient::Response;

IOHandler::IOHandler(DevToolsIOContext* io_context)
    : io_context_(io_context)
    , weak_factory_(this) {}

IOHandler::~IOHandler() {}

void IOHandler::SetClient(std::unique_ptr<Client> client) {
  client_.swap(client);
}

Response IOHandler::Read(DevToolsCommandId command_id,
    const std::string& handle, const int* offset, const int* max_size) {
  static const size_t kDefaultChunkSize = 10 * 1024 * 1024;

  scoped_refptr<DevToolsIOContext::Stream> stream =
      io_context_->GetByHandle(handle);
  if (!stream)
    return Response::InvalidParams("Invalid stream handle");
  stream->Read(offset ? *offset : -1,
               max_size && *max_size ? *max_size : kDefaultChunkSize,
               base::Bind(&IOHandler::ReadComplete,
                          weak_factory_.GetWeakPtr(), command_id));
  return Response::OK();
}

void IOHandler::ReadComplete(DevToolsCommandId command_id,
                             const scoped_refptr<base::RefCountedString>& data,
                             int status) {
  if (status == DevToolsIOContext::Stream::StatusFailure) {
    client_->SendError(command_id, Response::ServerError("Read failed"));
    return;
  }
  bool eof = status == DevToolsIOContext::Stream::StatusEOF;
  client_->SendReadResponse(command_id,
      ReadResponse::Create()->set_data(data->data())->set_eof(eof));
}

Response IOHandler::Close(const std::string& handle) {
  return io_context_->Close(handle) ? Response::OK()
      : Response::InvalidParams("Invalid stream handle");
}

}  // namespace io
}  // namespace devtools
}  // namespace content
