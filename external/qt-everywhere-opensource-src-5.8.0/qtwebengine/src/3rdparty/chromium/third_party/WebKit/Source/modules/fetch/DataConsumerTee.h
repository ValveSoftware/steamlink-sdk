// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DataConsumerTee_h
#define DataConsumerTee_h

#include "modules/ModulesExport.h"
#include "modules/fetch/FetchDataConsumerHandle.h"
#include "public/platform/WebDataConsumerHandle.h"
#include "wtf/Allocator.h"
#include <memory>

namespace blink {

class ExecutionContext;

class MODULES_EXPORT DataConsumerTee {
    STATIC_ONLY(DataConsumerTee);
public:
    // Create two handles from one. |src| must be a valid unlocked handle.
    static void create(ExecutionContext*, std::unique_ptr<WebDataConsumerHandle> src, std::unique_ptr<WebDataConsumerHandle>* dest1, std::unique_ptr<WebDataConsumerHandle>* dest2);
    static void create(ExecutionContext*, std::unique_ptr<FetchDataConsumerHandle> src, std::unique_ptr<FetchDataConsumerHandle>* dest1, std::unique_ptr<FetchDataConsumerHandle>* dest2);
};

} // namespace blink

#endif // DataConsumerTee_h
