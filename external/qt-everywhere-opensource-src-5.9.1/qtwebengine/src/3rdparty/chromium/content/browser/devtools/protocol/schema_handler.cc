// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/schema_handler.h"

namespace content {
namespace devtools {
namespace schema {

using Response = DevToolsProtocolClient::Response;

SchemaHandler::SchemaHandler() {
}

SchemaHandler::~SchemaHandler() {
}

Response SchemaHandler::GetDomains(
    std::vector<scoped_refptr<Domain>>* domains) {
  // TODO(kozyatisnkiy): get this from the target instead of hardcoding a list.
  static const char kVersion[] = "1.2";
  static const char* kDomains[] = {
    "Inspector", "Memory", "Page", "Rendering", "Emulation", "Security",
    "Network", "Database", "IndexedDB", "CacheStorage", "DOMStorage", "CSS",
    "ApplicationCache", "DOM", "IO", "DOMDebugger", "ServiceWorker",
    "Input", "LayerTree", "DeviceOrientation", "Tracing", "Animation",
    "Accessibility", "Storage", "Log", "Runtime", "Debugger",
    "Profiler", "HeapProfiler", "Schema", "Target"
  };
  for (size_t i = 0; i < arraysize(kDomains); ++i) {
    domains->push_back(Domain::Create()
        ->set_name(kDomains[i])
        ->set_version(kVersion));
  }
  return Response::OK();
}

}  // namespace schema
}  // namespace devtools
}  // namespace content
