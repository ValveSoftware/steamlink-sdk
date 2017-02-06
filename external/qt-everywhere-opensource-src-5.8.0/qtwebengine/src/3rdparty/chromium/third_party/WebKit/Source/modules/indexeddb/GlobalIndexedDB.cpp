// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/indexeddb/GlobalIndexedDB.h"

#include "core/frame/LocalDOMWindow.h"
#include "core/workers/WorkerGlobalScope.h"
#include "modules/indexeddb/IDBFactory.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

namespace {

template <typename T>
class GlobalIndexedDBImpl final : public GarbageCollected<GlobalIndexedDBImpl<T>>, public Supplement<T> {
    USING_GARBAGE_COLLECTED_MIXIN(GlobalIndexedDBImpl);
public:
    static GlobalIndexedDBImpl& from(T& supplementable)
    {
        GlobalIndexedDBImpl* supplement = static_cast<GlobalIndexedDBImpl*>(Supplement<T>::from(supplementable, name()));
        if (!supplement) {
            supplement = new GlobalIndexedDBImpl;
            Supplement<T>::provideTo(supplementable, name(), supplement);
        }
        return *supplement;
    }

    IDBFactory* idbFactory(T& fetchingScope)
    {
        if (!m_idbFactory)
            m_idbFactory = IDBFactory::create();
        return m_idbFactory;
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_idbFactory);
        Supplement<T>::trace(visitor);
    }

private:
    GlobalIndexedDBImpl()
    {
    }

    static const char* name() { return "IndexedDB"; }

    Member<IDBFactory> m_idbFactory;
};

} // namespace

IDBFactory* GlobalIndexedDB::indexedDB(DOMWindow& window)
{
    return GlobalIndexedDBImpl<LocalDOMWindow>::from(toLocalDOMWindow(window)).idbFactory(toLocalDOMWindow(window));
}

IDBFactory* GlobalIndexedDB::indexedDB(WorkerGlobalScope& worker)
{
    return GlobalIndexedDBImpl<WorkerGlobalScope>::from(worker).idbFactory(worker);
}

} // namespace blink
