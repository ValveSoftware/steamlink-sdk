// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_IO_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_IO_HANDLER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/devtools/protocol/devtools_protocol_dispatcher.h"

namespace base {
class RefCountedString;
}

namespace content {
namespace devtools {
class DevToolsIOContext;

namespace io {

class IOHandler {
 public:
  using Response = DevToolsProtocolClient::Response;

  explicit IOHandler(DevToolsIOContext* io_context);
  ~IOHandler();

  void SetClient(std::unique_ptr<Client> client);

  // Protocol methods.
  Response Read(DevToolsCommandId command_id, const std::string& handle,
      const int* offset, const int* max_size);
  Response Close(const std::string& handle);

 private:
  void ReadComplete(DevToolsCommandId command_id,
      const scoped_refptr<base::RefCountedString>& data, int status);

  std::unique_ptr<Client> client_;
  DevToolsIOContext* io_context_;
  base::WeakPtrFactory<IOHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(IOHandler);
};

}  // namespace io
}  // namespace devtools
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TRACING_HANDLER_H_
