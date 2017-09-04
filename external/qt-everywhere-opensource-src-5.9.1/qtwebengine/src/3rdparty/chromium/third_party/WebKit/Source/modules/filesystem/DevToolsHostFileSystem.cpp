// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/filesystem/DevToolsHostFileSystem.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/DevToolsHost.h"
#include "core/page/Page.h"
#include "modules/filesystem/DOMFileSystem.h"
#include "platform/json/JSONValues.h"

namespace blink {

DOMFileSystem* DevToolsHostFileSystem::isolatedFileSystem(
    DevToolsHost& host,
    const String& fileSystemName,
    const String& rootURL) {
  ExecutionContext* context = host.frontendFrame()->document();
  return DOMFileSystem::create(context, fileSystemName, FileSystemTypeIsolated,
                               KURL(ParsedURLString, rootURL));
}

void DevToolsHostFileSystem::upgradeDraggedFileSystemPermissions(
    DevToolsHost& host,
    DOMFileSystem* domFileSystem) {
  std::unique_ptr<JSONObject> message = JSONObject::create();
  message->setInteger("id", 0);
  message->setString("method", "upgradeDraggedFileSystemPermissions");
  std::unique_ptr<JSONArray> params = JSONArray::create();
  params->pushString(domFileSystem->rootURL().getString());
  message->setArray("params", std::move(params));
  host.sendMessageToEmbedder(message->toJSONString());
}

}  // namespace blink
