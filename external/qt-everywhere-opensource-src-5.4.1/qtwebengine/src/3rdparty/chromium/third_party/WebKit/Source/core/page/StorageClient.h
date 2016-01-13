// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StorageClient_h
#define StorageClient_h

#include "core/storage/StorageArea.h"
#include "wtf/PassOwnPtr.h"

namespace WebCore {

class StorageNamespace;

class StorageClient {
public:
    virtual ~StorageClient() { }

    virtual PassOwnPtr<StorageNamespace> createSessionStorageNamespace() = 0;
    virtual bool canAccessStorage(LocalFrame*, StorageType) const = 0;
};

} // namespace WebCore

#endif // StorageClient_h
