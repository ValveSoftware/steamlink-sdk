// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/indexeddb/IDBObserver.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/modules/v8/ToV8ForModules.h"
#include "bindings/modules/v8/V8BindingForModules.h"
#include "modules/indexeddb/IDBObserverCallback.h"
#include "modules/indexeddb/IDBObserverInit.h"

namespace blink {

IDBObserver* IDBObserver::create(IDBObserverCallback& callback, const IDBObserverInit& options)
{
    return new IDBObserver(callback, options);
}

IDBObserver::IDBObserver(IDBObserverCallback& callback, const IDBObserverInit& options)
    : m_callback(&callback)
    , m_transaction(options.transaction())
    , m_values(options.values())
    , m_noRecords(options.noRecords())
{
}

void IDBObserver::observe(IDBDatabase* database, IDBTransaction* transaction, ExceptionState& exceptionState)
{
    // TODO(palakj): Finish implementation.
}

DEFINE_TRACE(IDBObserver)
{
    visitor->trace(m_callback);
}

} // namespace blink
