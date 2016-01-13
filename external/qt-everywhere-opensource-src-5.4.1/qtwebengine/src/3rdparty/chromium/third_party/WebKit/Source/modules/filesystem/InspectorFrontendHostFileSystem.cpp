// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/filesystem/InspectorFrontendHostFileSystem.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/InspectorFrontendHost.h"
#include "core/page/Page.h"
#include "modules/filesystem/DOMFileSystem.h"
#include "platform/JSONValues.h"

namespace WebCore {

DOMFileSystem* InspectorFrontendHostFileSystem::isolatedFileSystem(InspectorFrontendHost& host, const String& fileSystemName, const String& rootURL)
{
    ExecutionContext* context = host.frontendPage()->deprecatedLocalMainFrame()->document();
    return DOMFileSystem::create(context, fileSystemName, FileSystemTypeIsolated, KURL(ParsedURLString, rootURL));
}

void InspectorFrontendHostFileSystem::upgradeDraggedFileSystemPermissions(InspectorFrontendHost& host, DOMFileSystem* domFileSystem)
{
    RefPtr<JSONObject> message = JSONObject::create();
    message->setNumber("id", 0);
    message->setString("method", "upgradeDraggedFileSystemPermissions");
    RefPtr<JSONArray> params = JSONArray::create();
    message->setArray("params", params);
    params->pushString(domFileSystem->rootURL().string());
    host.sendMessageToEmbedder(message->toJSONString());
}

InspectorFrontendHostFileSystem::InspectorFrontendHostFileSystem() { }

InspectorFrontendHostFileSystem::~InspectorFrontendHostFileSystem() { }


} // namespace WebCore
