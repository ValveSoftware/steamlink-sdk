// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IDBObserver_h
#define IDBObserver_h

#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/ModulesExport.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebVector.h"
#include "public/platform/modules/indexeddb/WebIDBTypes.h"

namespace blink {

class ExceptionState;
class IDBDatabase;
class IDBObserverCallback;
class IDBObserverInit;
class IDBTransaction;
struct WebIDBObservation;

class MODULES_EXPORT IDBObserver final : public GarbageCollected<IDBObserver>,
                                         public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static IDBObserver* create(IDBObserverCallback*);

  void removeObserver(int32_t id);
  void onChange(int32_t id,
                const WebVector<WebIDBObservation>&,
                const WebVector<int32_t>& observationIndex);

  // Implement the IDBObserver IDL.
  void observe(IDBDatabase*,
               IDBTransaction*,
               const IDBObserverInit&,
               ExceptionState&);
  void unobserve(IDBDatabase*, ExceptionState&);

  DECLARE_TRACE();

 private:
  explicit IDBObserver(IDBObserverCallback*);

  Member<IDBObserverCallback> m_callback;
  HeapHashMap<int32_t, WeakMember<IDBDatabase>> m_observerIds;
};

}  // namespace blink

#endif  // IDBObserver_h
