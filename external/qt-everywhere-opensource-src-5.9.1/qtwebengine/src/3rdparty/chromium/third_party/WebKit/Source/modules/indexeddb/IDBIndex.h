/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef IDBIndex_h
#define IDBIndex_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/indexeddb/IDBCursor.h"
#include "modules/indexeddb/IDBKeyPath.h"
#include "modules/indexeddb/IDBKeyRange.h"
#include "modules/indexeddb/IDBMetadata.h"
#include "modules/indexeddb/IDBRequest.h"
#include "public/platform/modules/indexeddb/WebIDBCursor.h"
#include "public/platform/modules/indexeddb/WebIDBDatabase.h"
#include "public/platform/modules/indexeddb/WebIDBTypes.h"
#include "wtf/Forward.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExceptionState;
class IDBObjectStore;

class IDBIndex final : public GarbageCollectedFinalized<IDBIndex>,
                       public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static IDBIndex* create(RefPtr<IDBIndexMetadata> metadata,
                          IDBObjectStore* objectStore,
                          IDBTransaction* transaction) {
    return new IDBIndex(std::move(metadata), objectStore, transaction);
  }
  ~IDBIndex();
  DECLARE_TRACE();

  // Implement the IDL
  const String& name() const { return metadata().name; }
  void setName(const String& name, ExceptionState&);
  IDBObjectStore* objectStore() const { return m_objectStore.get(); }
  ScriptValue keyPath(ScriptState*) const;
  bool unique() const { return metadata().unique; }
  bool multiEntry() const { return metadata().multiEntry; }

  IDBRequest* openCursor(ScriptState*,
                         const ScriptValue& key,
                         const String& direction,
                         ExceptionState&);
  IDBRequest* openKeyCursor(ScriptState*,
                            const ScriptValue& range,
                            const String& direction,
                            ExceptionState&);
  IDBRequest* count(ScriptState*, const ScriptValue& range, ExceptionState&);
  IDBRequest* get(ScriptState*, const ScriptValue& key, ExceptionState&);
  IDBRequest* getAll(ScriptState*, const ScriptValue& range, ExceptionState&);
  IDBRequest* getAll(ScriptState*,
                     const ScriptValue& range,
                     unsigned long maxCount,
                     ExceptionState&);
  IDBRequest* getKey(ScriptState*, const ScriptValue& key, ExceptionState&);
  IDBRequest* getAllKeys(ScriptState*,
                         const ScriptValue& range,
                         ExceptionState&);
  IDBRequest* getAllKeys(ScriptState*,
                         const ScriptValue& range,
                         uint32_t maxCount,
                         ExceptionState&);

  void markDeleted() {
    DCHECK(m_transaction->isVersionChange())
        << "Index deleted outside versionchange transaction.";
    m_deleted = true;
  }
  bool isDeleted() const { return m_deleted; }
  int64_t id() const { return metadata().id; }

  // True if this index was created in its associated transaction.
  // Only valid if the index's associated transaction is a versionchange.
  bool isNewlyCreated(
      const IDBObjectStoreMetadata& oldObjectStoreMetadata) const {
    DCHECK(m_transaction->isVersionChange());

    // Index IDs are allocated sequentially, so we can tell if an index was
    // created in this transaction by comparing its ID against the object
    // store's maximum index ID at the time when the transaction was started.
    return id() > oldObjectStoreMetadata.maxIndexId;
  }

  void revertMetadata(RefPtr<IDBIndexMetadata> oldMetadata);

  // Used internally and by InspectorIndexedDBAgent:
  IDBRequest* openCursor(ScriptState*, IDBKeyRange*, WebIDBCursorDirection);

  WebIDBDatabase* backendDB() const;

 private:
  IDBIndex(RefPtr<IDBIndexMetadata>, IDBObjectStore*, IDBTransaction*);

  const IDBIndexMetadata& metadata() const { return *m_metadata; }

  IDBRequest* getInternal(ScriptState*,
                          const ScriptValue& key,
                          ExceptionState&,
                          bool keyOnly);
  IDBRequest* getAllInternal(ScriptState*,
                             const ScriptValue& range,
                             unsigned long maxCount,
                             ExceptionState&,
                             bool keyOnly);

  RefPtr<IDBIndexMetadata> m_metadata;
  Member<IDBObjectStore> m_objectStore;
  Member<IDBTransaction> m_transaction;
  bool m_deleted = false;
};

}  // namespace blink

#endif  // IDBIndex_h
