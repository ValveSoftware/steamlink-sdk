// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorFrontendHostFileSystem_h
#define InspectorFrontendHostFileSystem_h

#include "platform/heap/Handle.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class DOMFileSystem;
class InspectorFrontendHost;

class InspectorFrontendHostFileSystem {
public:
    static DOMFileSystem* isolatedFileSystem(InspectorFrontendHost&, const String& fileSystemName, const String& rootURL);
    static void upgradeDraggedFileSystemPermissions(InspectorFrontendHost&, DOMFileSystem*);
private:
    InspectorFrontendHostFileSystem();
    ~InspectorFrontendHostFileSystem();
};

} // namespace WebCore

#endif // !defined(InspectorFrontendHostFileSystem_h)
