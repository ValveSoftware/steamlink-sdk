// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.+

#ifndef IDBObservation_h
#define IDBObservation_h

#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/indexeddb/WebIDBTypes.h"
#include "wtf/PassRefPtr.h"

namespace blink {

class IDBKeyRange;
class IDBValue;
class ScriptState;
struct WebIDBObservation;

class IDBObservation final : public GarbageCollectedFinalized<IDBObservation>,
                             public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static WebIDBOperationType stringToOperationType(const String&);
  static IDBObservation* create(const WebIDBObservation&);
  ~IDBObservation();

  DECLARE_TRACE();

  // Implement the IDL
  ScriptValue key(ScriptState*);
  ScriptValue value(ScriptState*);
  const String& type() const;

 private:
  IDBObservation(const WebIDBObservation&);
  Member<IDBKeyRange> m_keyRange;
  RefPtr<IDBValue> m_value;
  const WebIDBOperationType m_operationType;
};

}  // namespace blink

#endif  // IDBObservation_h
