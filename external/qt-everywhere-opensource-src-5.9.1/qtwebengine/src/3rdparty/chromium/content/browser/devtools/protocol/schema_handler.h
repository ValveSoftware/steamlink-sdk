// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SCHEMA_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SCHEMA_HANDLER_H_

#include "base/macros.h"
#include "content/browser/devtools/protocol/devtools_protocol_dispatcher.h"

namespace content {
namespace devtools {
namespace schema {

class SchemaHandler {
 public:
  using Response = DevToolsProtocolClient::Response;

  SchemaHandler();
  ~SchemaHandler();

  Response GetDomains(std::vector<scoped_refptr<Domain>>* domains);
 private:
  DISALLOW_COPY_AND_ASSIGN(SchemaHandler);
};

}  // namespace schema
}  // namespace devtools
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SCHEMA_HANDLER_H_
