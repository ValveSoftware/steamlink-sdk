// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DataConsumerHandleUtil_h
#define DataConsumerHandleUtil_h

#include "modules/ModulesExport.h"
#include "modules/fetch/FetchDataConsumerHandle.h"
#include "public/platform/WebDataConsumerHandle.h"
#include "wtf/Allocator.h"
#include "wtf/WeakPtr.h"
#include <memory>

namespace blink {

// Returns a handle that returns ShouldWait for read / beginRead and
// UnexpectedError for endRead.
MODULES_EXPORT std::unique_ptr<WebDataConsumerHandle> createWaitingDataConsumerHandle();

// Returns a handle that returns Done for read / beginRead and
// UnexpectedError for endRead.
MODULES_EXPORT std::unique_ptr<WebDataConsumerHandle> createDoneDataConsumerHandle();

// Returns a handle that returns UnexpectedError for read / beginRead /
// endRead.
MODULES_EXPORT std::unique_ptr<WebDataConsumerHandle> createUnexpectedErrorDataConsumerHandle();

// Returns a FetchDataConsumerHandle that wraps WebDataConsumerHandle.
MODULES_EXPORT std::unique_ptr<FetchDataConsumerHandle> createFetchDataConsumerHandleFromWebHandle(std::unique_ptr<WebDataConsumerHandle>);

// A helper class to call Client::didGetReadable() asynchronously after
// Reader creation.
// NotifyOnReaderCreationHelper must be owned by a reader and
// |client| must be the client of the reader.
class NotifyOnReaderCreationHelper final {
    DISALLOW_NEW();
public:
    NotifyOnReaderCreationHelper(WebDataConsumerHandle::Client* /* client */);

private:
    void notify(WebDataConsumerHandle::Client*);

    WeakPtrFactory<NotifyOnReaderCreationHelper> m_factory;
};

} // namespace blink

#endif // DataConsumerHandleUtil_h
