// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "MockWebIDBDatabase.h"

#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

MockWebIDBDatabase::MockWebIDBDatabase() {}

MockWebIDBDatabase::~MockWebIDBDatabase() {}

std::unique_ptr<MockWebIDBDatabase> MockWebIDBDatabase::create()
{
    return wrapUnique(new MockWebIDBDatabase());
}

} // namespace blink
