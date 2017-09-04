// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StorageClient_h
#define StorageClient_h

#include "modules/storage/StorageArea.h"
#include <memory>

namespace blink {

class StorageNamespace;

class StorageClient {
 public:
  virtual ~StorageClient() {}

  virtual std::unique_ptr<StorageNamespace> createSessionStorageNamespace() = 0;
  virtual bool canAccessStorage(LocalFrame*, StorageType) const = 0;
};

}  // namespace blink

#endif  // StorageClient_h
